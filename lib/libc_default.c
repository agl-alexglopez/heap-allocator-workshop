/// Author: NOT Alexander G. Lopez :)
/// File: libc_default.c
/// ---------------------
/// With the timing tools I use and how the scripts isolate allocator
/// behavior, we can actually time libc, kind of. This implementation
/// should only be incorporated into the mini stats program because that
/// just calls the basic heap allocator functions like malloc, realloc, and free
/// with no additional checks or requirements. I could possibly explore the
/// libc heap debug functions because I believe they have some that share
/// state and opaque pointers to internal data but I don't want to mess with
/// that. In my initial testing the timing information from libc allocator seems
/// accurate and I have included this implementation in the performance testing
/// graphs so we can see how other allocators stack up!
#include "allocator.h"
#include "print_utility.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

size_t
wget_free_total(void)
{
    return 0;
}

bool
winit(void *heap_start, size_t heap_size)
{
    (void)heap_start;
    (void)heap_size;
    return true;
}

void *
wmalloc(size_t requested_size)
{
    return malloc(requested_size);
}

void *
wrealloc(void *old_ptr, size_t new_size)
{
    return realloc(old_ptr, new_size);
}

void
wfree(void *ptr)
{
    free(ptr);
}

bool
wvalidate_heap(void)
{
    return true;
}

size_t
wheap_align(size_t request)
{
    (void)request;
    return 0;
}

size_t
wheap_capacity(void)
{
    return 0;
}

void
wheap_diff(const struct heap_block expected[], struct heap_block actual[],
           size_t len)
{
    (void)expected;
    (void)actual;
    (void)len;
}

void
wprint_free_nodes(enum print_style style)
{
    (void)style;
}

void
wdump_heap(void)
{}
