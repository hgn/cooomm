/*
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** Author: Hagen Paul Pfeifer <hagen.pfeifer@protocollabs.com>
**
*/

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <malloc.h>
#include <limits.h>

#undef __always_inline
#if __GNUC_PREREQ (3,2)
#define __always_inline __inline __attribute__ ((__always_inline__))
#else
#define __always_inline __inline
#endif

/*
 * See if our compiler is known to support flexible array members.
 */
#ifndef FLEX_ARRAY
#if defined(__STDC_VERSION__) && \
	(__STDC_VERSION__ >= 199901L) && \
	(!defined(__SUNPRO_C) || (__SUNPRO_C > 0x580))
#define FLEX_ARRAY		/* empty */
#elif defined(__GNUC__)
#if (__GNUC__ >= 3)
#define FLEX_ARRAY		/* empty */
#else
#define FLEX_ARRAY 0		/* older GNU extension */
#endif
#endif
#ifndef FLEX_ARRAY
#define FLEX_ARRAY 1
#endif
#endif

#define min(x,y) ({             \
        typeof(x) _x = (x);     \
        typeof(y) _y = (y);     \
        (void) (&_x == &_y);    \
        _x < _y ? _x : _y; })

#define max(x,y) ({             \
        typeof(x) _x = (x);     \
        typeof(y) _y = (y);     \
        (void) (&_x == &_y);    \
        _x > _y ? _x : _y; })

/* determine the size of an array */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define BITSIZEOF(x)  (CHAR_BIT * sizeof(x))

enum rbtree_color { RED, BLACK };

typedef int (*rbtree_compare_func) (void *left_key, void *right_key);

struct rbtree_node {
	struct rbtree_node *left;
	struct rbtree_node *right;
	struct rbtree_node *parent;
	enum rbtree_color color;
	void *key;
	void *data;
};

struct rbtree {
	struct rbtree_node *root;
	rbtree_compare_func compare;
	size_t size;
};

void rbtree_init(struct rbtree *, int (*cmp) (void *, void *));
void rbtree_init_int(struct rbtree *);
void rbtree_init_double(struct rbtree *);
void *rbtree_lookup(struct rbtree *, void *);
void rbtree_insert(struct rbtree *, struct rbtree_node *);
struct rbtree_node *rbtree_delete(struct rbtree *, void *);
size_t rbtree_size(struct rbtree *);
struct rbtree_node *rbtree_node_alloc(void);
void rbtree_node_free(struct rbtree_node *);

static struct rbtree_node *sibling(struct rbtree_node *n);
static struct rbtree_node *uncle(struct rbtree_node *n);
static enum rbtree_color node_color(struct rbtree_node *n);

static struct rbtree_node *lookup_node(struct rbtree *, void *);
static void rotate_left(struct rbtree *, struct rbtree_node *);
static void rotate_right(struct rbtree *, struct rbtree_node *);

static struct rbtree_node *maximum_node(struct rbtree_node *);
static void replace_node(struct rbtree *, struct rbtree_node *,
			 struct rbtree_node *);
static void insert_case1(struct rbtree *, struct rbtree_node *);
static void insert_case2(struct rbtree *, struct rbtree_node *);
static void insert_case3(struct rbtree *, struct rbtree_node *);
static void insert_case4(struct rbtree *, struct rbtree_node *);
static void insert_case5(struct rbtree *, struct rbtree_node *);
static void delete_case1(struct rbtree *, struct rbtree_node *);
static void delete_case2(struct rbtree *, struct rbtree_node *);
static void delete_case3(struct rbtree *, struct rbtree_node *);
static void delete_case4(struct rbtree *, struct rbtree_node *);
static void delete_case5(struct rbtree *, struct rbtree_node *);
static void delete_case6(struct rbtree *, struct rbtree_node *);

size_t rbtree_size(struct rbtree *tree)
{
	return tree->size;
}

struct rbtree_node *rbtree_node_alloc(void)
{
	return malloc(sizeof(struct rbtree_node));
}

void rbtree_node_free(struct rbtree_node *n)
{
	free(n);
}

static struct rbtree_node *grandparent(struct rbtree_node *n)
{
	assert(n != NULL);
	assert(n->parent != NULL);	/* Not the root struct rbtree_node* */
	assert(n->parent->parent != NULL);	/* Not child of root */
	return n->parent->parent;
}

struct rbtree_node *sibling(struct rbtree_node *n)
{
	assert(n != NULL);
	assert(n->parent != NULL);	/* Root struct rbtree_node* has no sibling */
	if (n == n->parent->left)
		return n->parent->right;
	else
		return n->parent->left;
}

struct rbtree_node *uncle(struct rbtree_node *n)
{
	assert(n != NULL);
	assert(n->parent != NULL);	/* Root struct rbtree_node* has no uncle */
	assert(n->parent->parent != NULL);	/* Children of root have no uncle */
	return sibling(n->parent);
}

enum rbtree_color node_color(struct rbtree_node *n)
{
	return n == NULL ? BLACK : n->color;
}

void rbtree_init(struct rbtree *tree, rbtree_compare_func compare)
{
	tree->root = NULL;
	tree->compare = compare;
	tree->size = 0;
}

static int compare_int(void *l, void *r)
{
	uintptr_t left = (uintptr_t) l;
	uintptr_t right = (uintptr_t) r;
	if (left < right)
		return -1;
	else if (left > right)
		return 1;
	else {
		assert(left == right);
		return 0;
	}
}

void rbtree_init_int(struct rbtree *tree)
{
	return rbtree_init(tree, compare_int);
}

static int compare_double(void *leftp, void *rightp)
{
	double *left = (double *)leftp;
	double *right = (double *)rightp;
	if (*left < *right)
		return -1;
	else if (*left > *right)
		return 1;
	else {
		assert(*left == *right);
		return 0;
	}
}

void rbtree_init_double(struct rbtree *tree)
{
	return rbtree_init(tree, compare_double);
}

struct rbtree_node *lookup_node(struct rbtree *t, void *key)
{
	struct rbtree_node *n = t->root;
	while (n != NULL) {
		int comp_result = t->compare(key, n->key);
		if (comp_result == 0) {
			return n;
		} else if (comp_result < 0) {
			n = n->left;
		} else {
			assert(comp_result > 0);
			n = n->right;
		}
	}
	return n;
}

void *rbtree_lookup(struct rbtree *t, void *key)
{
	struct rbtree_node *n = lookup_node(t, key);
	return n == NULL ? NULL : n->data;
}

void rotate_left(struct rbtree *t, struct rbtree_node *n)
{
	struct rbtree_node *r = n->right;
	replace_node(t, n, r);
	n->right = r->left;
	if (r->left != NULL) {
		r->left->parent = n;
	}
	r->left = n;
	n->parent = r;
}

void rotate_right(struct rbtree *t, struct rbtree_node *n)
{
	struct rbtree_node *L = n->left;
	replace_node(t, n, L);
	n->left = L->right;
	if (L->right != NULL) {
		L->right->parent = n;
	}
	L->right = n;
	n->parent = L;
}

void replace_node(struct rbtree *t, struct rbtree_node *oldn,
		  struct rbtree_node *newn)
{
	if (oldn->parent == NULL) {
		t->root = newn;
	} else {
		if (oldn == oldn->parent->left)
			oldn->parent->left = newn;
		else
			oldn->parent->right = newn;
	}
	if (newn != NULL) {
		newn->parent = oldn->parent;
	}
}

void rbtree_insert(struct rbtree *t, struct rbtree_node *inserted_node)
{

	inserted_node->color = RED;
	inserted_node->left = NULL;
	inserted_node->right = NULL;
	inserted_node->parent = NULL;

	if (t->root == NULL) {
		t->root = inserted_node;
	} else {
		struct rbtree_node *n = t->root;
		while (1) {
			int comp_result =
			    t->compare(inserted_node->key, n->key);
			if (comp_result == 0) {
				/* duplicate */
				n->data = inserted_node->data;
				return;
			} else if (comp_result < 0) {
				if (n->left == NULL) {
					n->left = inserted_node;
					break;
				} else {
					n = n->left;
				}
			} else {
				assert(comp_result > 0);
				if (n->right == NULL) {
					n->right = inserted_node;
					break;
				} else {
					n = n->right;
				}
			}
		}
		inserted_node->parent = n;
	}
	t->size++;
	insert_case1(t, inserted_node);
}

void insert_case1(struct rbtree *t, struct rbtree_node *n)
{
	if (n->parent == NULL)
		n->color = BLACK;
	else
		insert_case2(t, n);
}

void insert_case2(struct rbtree *t, struct rbtree_node *n)
{
	if (node_color(n->parent) == BLACK)
		return;
	else
		insert_case3(t, n);
}

void insert_case3(struct rbtree *t, struct rbtree_node *n)
{
	if (node_color(uncle(n)) == RED) {
		n->parent->color = BLACK;
		uncle(n)->color = BLACK;
		grandparent(n)->color = RED;
		insert_case1(t, grandparent(n));
	} else {
		insert_case4(t, n);
	}
}

void insert_case4(struct rbtree *t, struct rbtree_node *n)
{
	if (n == n->parent->right && n->parent == grandparent(n)->left) {
		rotate_left(t, n->parent);
		n = n->left;
	} else if (n == n->parent->left && n->parent == grandparent(n)->right) {
		rotate_right(t, n->parent);
		n = n->right;
	}
	insert_case5(t, n);
}

void insert_case5(struct rbtree *t, struct rbtree_node *n)
{
	n->parent->color = BLACK;
	grandparent(n)->color = RED;
	if (n == n->parent->left && n->parent == grandparent(n)->left) {
		rotate_right(t, grandparent(n));
	} else {
		assert(n == n->parent->right
		       && n->parent == grandparent(n)->right);
		rotate_left(t, grandparent(n));
	}
}

struct rbtree_node *rbtree_delete(struct rbtree *t, void *key)
{
	struct rbtree_node *child;
	struct rbtree_node *n = lookup_node(t, key);
	if (n == NULL)
		return NULL;	/* Key not found, do nothing */
	if (n->left != NULL && n->right != NULL) {
		/* Copy key/data from predecessor and then delete it instead */
		struct rbtree_node *pred = maximum_node(n->left);
		n->key = pred->key;
		n->data = pred->data;
		n = pred;
	}

	assert(n->left == NULL || n->right == NULL);
	child = n->right == NULL ? n->left : n->right;
	if (node_color(n) == BLACK) {
		n->color = node_color(child);
		delete_case1(t, n);
	}
	replace_node(t, n, child);
	if (n->parent == NULL && child != NULL)
		child->color = BLACK;

	return n;
}

static struct rbtree_node *maximum_node(struct rbtree_node *n)
{
	assert(n != NULL);
	while (n->right != NULL) {
		n = n->right;
	}
	return n;
}

void delete_case1(struct rbtree *t, struct rbtree_node *n)
{
	if (n->parent == NULL)
		return;
	else
		delete_case2(t, n);
}

void delete_case2(struct rbtree *t, struct rbtree_node *n)
{
	if (node_color(sibling(n)) == RED) {
		n->parent->color = RED;
		sibling(n)->color = BLACK;
		if (n == n->parent->left)
			rotate_left(t, n->parent);
		else
			rotate_right(t, n->parent);
	}
	delete_case3(t, n);
}

void delete_case3(struct rbtree *t, struct rbtree_node *n)
{
	if (node_color(n->parent) == BLACK &&
	    node_color(sibling(n)) == BLACK &&
	    node_color(sibling(n)->left) == BLACK &&
	    node_color(sibling(n)->right) == BLACK) {
		sibling(n)->color = RED;
		delete_case1(t, n->parent);
	} else
		delete_case4(t, n);
}

void delete_case4(struct rbtree *t, struct rbtree_node *n)
{
	if (node_color(n->parent) == RED &&
	    node_color(sibling(n)) == BLACK &&
	    node_color(sibling(n)->left) == BLACK &&
	    node_color(sibling(n)->right) == BLACK) {
		sibling(n)->color = RED;
		n->parent->color = BLACK;
	} else
		delete_case5(t, n);
}

void delete_case5(struct rbtree *t, struct rbtree_node *n)
{
	if (n == n->parent->left &&
	    node_color(sibling(n)) == BLACK &&
	    node_color(sibling(n)->left) == RED &&
	    node_color(sibling(n)->right) == BLACK) {
		sibling(n)->color = RED;
		sibling(n)->left->color = BLACK;
		rotate_right(t, sibling(n));
	} else if (n == n->parent->right &&
		   node_color(sibling(n)) == BLACK &&
		   node_color(sibling(n)->right) == RED &&
		   node_color(sibling(n)->left) == BLACK) {
		sibling(n)->color = RED;
		sibling(n)->right->color = BLACK;
		rotate_left(t, sibling(n));
	}
	delete_case6(t, n);
}

void delete_case6(struct rbtree *t, struct rbtree_node *n)
{
	sibling(n)->color = node_color(n->parent);
	n->parent->color = BLACK;

	if (n == n->parent->left) {
		assert(node_color(sibling(n)->right) == RED);
		sibling(n)->right->color = BLACK;
		rotate_left(t, n->parent);
	} else {
		assert(node_color(sibling(n)->left) == RED);
		sibling(n)->left->color = BLACK;
		rotate_right(t, n->parent);
	}
}

static void my_init_hook(void);
static void *my_malloc_hook(size_t, const void *);
static void my_free_hook(void *, const void *);

void (*volatile __malloc_initialize_hook) (void) = my_init_hook;

static void *(*old_malloc_hook) (size_t size, const void *caller);
static void (*old_free_hook) (void *ptr, const void *caller);

static void my_init_hook(void)
{
	old_malloc_hook = __malloc_hook;
	old_free_hook = __free_hook;
	__malloc_hook = my_malloc_hook;
	__free_hook = my_free_hook;
}

static void *my_malloc_hook(size_t size, const void *caller)
{
	void *result;

	(void)caller;

	/* Restore all old hooks */
	__malloc_hook = old_malloc_hook;
	__free_hook = old_free_hook;

	/* Call recursively */
	result = malloc(size);

	/* Save underlying hooks */
	old_malloc_hook = __malloc_hook;
	old_free_hook = __free_hook;

	/* printf might call malloc, so protect it too. */
	printf("malloc (%u) returns %p\n", (unsigned int)size, result);

	/* Restore our own hooks */
	__malloc_hook = my_malloc_hook;
	__free_hook = my_free_hook;
	return result;
}

static void my_free_hook(void *ptr, const void *caller)
{
	(void)caller;

	/* Restore all old hooks */
	__malloc_hook = old_malloc_hook;
	__free_hook = old_free_hook;

	/* Call recursively */
	free(ptr);

	/* Save underlying hooks */
	old_malloc_hook = __malloc_hook;
	old_free_hook = __free_hook;

	/* printf might call free, so protect it too. */
	printf("freed pointer %p\n", ptr);

	/* Restore our own hooks */
	__malloc_hook = my_malloc_hook;
	__free_hook = my_free_hook;
}

/* definition of memory sub systems */
enum {
	MEMORY_SS_COMPONENT_ALPHA,
	MEMORY_SS_COMPONENT_BETA,

	MEMORY_SS_COMPONENT_MAX
};

struct coomm_account {
	size_t allocated;
	size_t max_allocated;
};

struct coomm_account coomm_account[MEMORY_SS_COMPONENT_MAX];

void coomm_init(void)
{
	memset(coomm_account, 0, sizeof(coomm_account));
}

static void (*oom_callback) (int subsystem, unsigned int severity);

void
coomm_register_oom_callback(void (*cb) (int subsystem, unsigned int severity))
{
	oom_callback = cb;
}

void *xmalloc(int component, size_t size)
{
	void *ptr;

	ptr = malloc(size);
	if (!ptr)
		return ptr;

	coomm_account[component].allocated += size;

	return ptr;
}

void xfree_full(int component, void *ptr, size_t size)
{
	if (coomm_account[component].allocated >
	    coomm_account[component].max_allocated) {
		coomm_account[component].max_allocated =
		    coomm_account[component].allocated;
	}

	coomm_account[component].allocated -= size;

	free(ptr);
}

void xfree(void *ptr)
{
	/* now search in tree to find entry by
	 * memory address. If we do not find an valid
	 * entry the user has called malloc_full() which
	 * he should not call or the pointer was never allocated.
	 * We print a warning/error message to correct this
	 */

	/* finally we call free anyway, just to provide a expected behavior */
	free(ptr);
}

static void coom_cb(int subsystem, unsigned int severity)
{
	printf("subsystem %d should reclaim memory [severity: %d]\n",
	       subsystem, severity);
}

static void init(void)
{
	coomm_init();
	coomm_register_oom_callback(coom_cb);
}

static void component_alpha_init(void)
{
	char *ptr;

	ptr = xmalloc(MEMORY_SS_COMPONENT_ALPHA, 100);

	xfree_full(MEMORY_SS_COMPONENT_ALPHA, ptr, 100);
}

int main(int ac, char **av)
{
	(void)ac;
	(void)av;

	init();

	component_alpha_init();

	return 0;
}
