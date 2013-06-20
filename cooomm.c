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
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
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


#define INIT_ACCOUNTER_ELEMENTS 256

struct cooom_accounter_entry {
	uintptr_t addr;
	size_t size;
	int component;
};

unsigned int cooom_accounter_entries_no = INIT_ACCOUNTER_ELEMENTS;
struct cooom_accounter_entry *cooom_accounter_entries;

static int cooom_init_accounter(void)
{
	const size_t init_size = sizeof(struct cooom_accounter_entry) *
					INIT_ACCOUNTER_ELEMENTS;

	cooom_accounter_entries = malloc(init_size);
	if (!cooom_accounter_entries)
		return -ENOMEM;

	memset(cooom_accounter_entries, 0, init_size);

	return 0;
}


struct coomm_subsystem {
	unsigned int id;
	const char *name;
	size_t min_required;
	unsigned int priority;
	void (*cb)(struct coomm_subsystem *, unsigned int severity);

	/* private members */
	size_t allocated;
	size_t max_allocated;
};

static struct coomm_subsystem *coomm_subsystem_saved;
size_t coomm_subsystem_max_saved;

int coomm_init(struct coomm_subsystem *ss, size_t ss_max, unsigned int flags)
{
	size_t i;

	if (!ss || ss_max <= 0)
		return -EINVAL;

	if (flags)
		return -EINVAL;

	if (coomm_subsystem_saved) {
		/* already called! */
		return -EINPROGRESS;
	}

	coomm_subsystem_saved = ss;
	coomm_subsystem_max_saved = ss_max;

	for (i = 0; i < ss_max; i++) {
		ss[i].allocated = 0;
		ss[i].max_allocated = 0;
	}

	cooom_init_accounter();

	return 0;
}


int coom_malloc_account(void *addr, unsigned int component, size_t size)
{
	unsigned int i;
	struct cooom_accounter_entry *cooom_accounter_entry;

	if (component >= coomm_subsystem_max_saved)
		return -EINVAL;

	coomm_subsystem_saved[component].allocated += size;

	for (i = 0; i < cooom_accounter_entries_no; i++) {
		cooom_accounter_entry = &cooom_accounter_entries[i];

		/* search free entry, 0 is invalid pointer
		 * thus we do mark unsused chunks as 0 */
		if (!cooom_accounter_entry->addr) {
			cooom_accounter_entry->addr      = (uintptr_t)addr;
			cooom_accounter_entry->size      = size;
			cooom_accounter_entry->component = component;
			return 0;
		}
	}

	fprintf(stderr, "Out of mem, realloc called\n");

	return -ENOMEM;
}


void coom_free_account(void *addr)
{
	unsigned int i;
	struct cooom_accounter_entry *cooom_accounter_entry;

	for (i = 0; i < cooom_accounter_entries_no; i++) {
		cooom_accounter_entry = &cooom_accounter_entries[i];
		if (!cooom_accounter_entry->addr)
			continue;

		if (cooom_accounter_entry->addr == (uintptr_t)addr) {
			if (coomm_subsystem_saved[cooom_accounter_entry->component].allocated >
					coomm_subsystem_saved[cooom_accounter_entry->component].max_allocated) {
				coomm_subsystem_saved[cooom_accounter_entry->component].max_allocated =
					coomm_subsystem_saved[cooom_accounter_entry->component].allocated;
			}
			coomm_subsystem_saved[cooom_accounter_entry->component].allocated -=
				cooom_accounter_entry->size;
			memset(cooom_accounter_entry, 0, sizeof(*cooom_accounter_entry));
			return;
		}
	}
}

void coomm_statistics_show(void)
{
	unsigned int i;

	for (i = 0; i < coomm_subsystem_max_saved; i++) {
		fprintf(stderr, "component: %s -> currently allocated: %zd, max allocated: %zd\n",
			coomm_subsystem_saved[i].name,
			coomm_subsystem_saved[i].allocated,
			coomm_subsystem_saved[i].max_allocated);

	}
}


/* must be freed by xfree_full */
void *xmalloc_full(unsigned int component, size_t size)
{
	void *ptr;

	/* this is not standard complain, but
	 * to make thinks easier we do not allow
	 * a allocation size of 0 */
	assert(size > 0);

	ptr = malloc(size);
	if (!ptr)
		return ptr;

	coom_malloc_account(ptr, component, size);

	return ptr;
}


void *xmalloc(int component, size_t size)
{
	void *ptr;

	ptr = malloc(size);
	if (!ptr)
		return ptr;

	coomm_subsystem_saved[component].allocated += size;

	return ptr;
}


void xfree_full(unsigned int component, void *ptr, size_t size)
{
	assert(component <= coomm_subsystem_max_saved);

	if (coomm_subsystem_saved[component].allocated >
	    coomm_subsystem_saved[component].max_allocated) {
		coomm_subsystem_saved[component].max_allocated =
		    coomm_subsystem_saved[component].allocated;
	}

	coomm_subsystem_saved[component].allocated -= size;

	free(ptr);
}


void xfree(void *ptr)
{
	coom_free_account(ptr);

	free(ptr);
}


static void component_generic_oom_cb(struct coomm_subsystem *cs, unsigned int severity)
{
	printf("subsystem %s should reclaim memory [severity: %d]\n",
	       cs->name, severity);

}


static struct coomm_subsystem cs[] =
{
#define COOMM_SS_ALPHA 0
	{
	  .id           = COOMM_SS_ALPHA,
	  .name         = "Alpha",
	  .priority     = 10,
	  .min_required = 2048,
	  .cb           = component_generic_oom_cb
	},
#define COOMM_SS_BETA  1
	{
	  .id           = COOMM_SS_BETA,
	  .name         = "Beta",
	  .priority     = 5,
	  .min_required = 8192,
	  .cb           = component_generic_oom_cb
	},
};


static void component_alpha_init(void)
{
	char *ptr, *ptr2;

	ptr = xmalloc_full(COOMM_SS_ALPHA, 100);
	coomm_statistics_show();

	xfree_full(COOMM_SS_ALPHA, ptr, 100);
	coomm_statistics_show();

	ptr = xmalloc_full(COOMM_SS_ALPHA, 100);
	coomm_statistics_show();

	ptr2 = xmalloc_full(COOMM_SS_ALPHA, 100);
	coomm_statistics_show();

	xfree_full(COOMM_SS_ALPHA, ptr, 100);
	xfree_full(COOMM_SS_ALPHA, ptr2, 100);
	coomm_statistics_show();
}


static void component_beta_init(void)
{
	char *ptr;

	ptr = xmalloc(COOMM_SS_BETA, 100);
	coomm_statistics_show();

	xfree(ptr);
	coomm_statistics_show();

	ptr = xmalloc(COOMM_SS_BETA, 100);
	coomm_statistics_show();


	xfree(ptr);
}



int main(int ac, char **av)
{
	(void)ac;
	(void)av;

	coomm_init(cs, ARRAY_SIZE(cs), 0);

	component_alpha_init();
	component_beta_init();
	coomm_statistics_show();

	return 0;
}
