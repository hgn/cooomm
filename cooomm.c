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
# define __always_inline __inline __attribute__ ((__always_inline__))
#else
# define __always_inline __inline
#endif

/*
 * See if our compiler is known to support flexible array members.
 */
#ifndef FLEX_ARRAY
#if defined(__STDC_VERSION__) && \
	(__STDC_VERSION__ >= 199901L) && \
	(!defined(__SUNPRO_C) || (__SUNPRO_C > 0x580))
# define FLEX_ARRAY /* empty */
#elif defined(__GNUC__)
# if (__GNUC__ >= 3)
#  define FLEX_ARRAY /* empty */
# else
#  define FLEX_ARRAY 0 /* older GNU extension */
# endif
#endif
#ifndef FLEX_ARRAY
# define FLEX_ARRAY 1
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

static void my_init_hook (void);
static void *my_malloc_hook (size_t, const void *);
static void my_free_hook (void*, const void *);

void (*volatile __malloc_initialize_hook)(void) = my_init_hook;

static void *(*old_malloc_hook)(size_t size, const void *caller);
static void (*old_free_hook)(void *ptr, const void *caller);


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

	(void) caller;

	/* Restore all old hooks */
	__malloc_hook = old_malloc_hook;
	__free_hook = old_free_hook;

	/* Call recursively */
	result = malloc(size);

	/* Save underlying hooks */
	old_malloc_hook = __malloc_hook;
	old_free_hook = __free_hook;

	/* printf might call malloc, so protect it too. */
	printf ("malloc (%u) returns %p\n", (unsigned int) size, result);

	/* Restore our own hooks */
	__malloc_hook = my_malloc_hook;
	__free_hook = my_free_hook;
	return result;
}


static void my_free_hook(void *ptr, const void *caller)
{
	(void) caller;

	/* Restore all old hooks */
	__malloc_hook = old_malloc_hook;
	__free_hook = old_free_hook;

	/* Call recursively */
	free(ptr);

	/* Save underlying hooks */
	old_malloc_hook = __malloc_hook;
	old_free_hook = __free_hook;

	/* printf might call free, so protect it too. */
	printf ("freed pointer %p\n", ptr);

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


static void (*oom_callback)(int subsystem, unsigned int severity)

void coomm_register_oom_callback(void (*cb)(int subsystem, unsigned int severity))
{
	oom_callback = cb;
}


void *xmalloc(int component, size_t size)
{
	void *ptr;

	ptr = malloc(size);
	if (!ptr)
		return ptr;

	coomm_account[component].allocated =+ size;

	return ptr;
}


void xfree_full(int component, void *ptr, size_t size)
{
	if (coomm_account[component].allocated >
	    coomm_account[component].max_allocated) {
		coomm_account[component].max_allocated =
			coomm_account[component].allocated;
	}

	coomm_account[component].allocated =- size;

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
	(void) ac;
	(void) av;

	init();

	component_alpha_init();

	return 0;
}
