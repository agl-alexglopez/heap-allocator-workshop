#include "allocator.h"
#include "debug_break.h"
#include <stdbool.h>
#include <stddef.h>

typedef size_t header;

struct free_node
{
    struct free_node *links[2];
};

enum tree_link
{
    L = 0,
    R = 1
};

enum list_link
{
    N = 0,
    P = 1
};

///////////////////////////////   Shared Heap Functions   ////////////////////////////////

bool myinit( void *heap_start, size_t heap_size )
{
    (void)heap_start;
    (void)heap_size;
    unimplemented();
}

void *mymalloc( size_t requested_size )
{
    (void)requested_size;
    unimplemented();
}

void *myrealloc( void *old_ptr, size_t new_size )
{
    (void)old_ptr;
    (void)new_size;
    unimplemented();
}

void myfree( void *ptr )
{
    (void)ptr;
    unimplemented();
}

bool validate_heap( void ) { unimplemented(); }

size_t get_free_total( void ) { unimplemented(); }

void print_free_nodes( print_style style )
{
    (void)style;
    unimplemented();
}
