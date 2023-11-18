/// Author: Alexander G. Lopez
/// File: list_segregated.c
/// ---------------------
/// This is a basic implementation of a segregated free list (aka fits) heap allocator. We
/// maintain 15 list sizes and do a first fit search on loosely sorted lists, approximating a best
/// fit search of the heap. We simply add new elements to a list class at the front to speed things
/// up rather than maintaining 15 sorted lists. This helps bring us closer to a O(lgN) runtime for
/// a list based allocator and only costs a small amount of utilization.
/// Citations:
/// -------------------
/// 1. Bryant and O'Hallaron, Computer Systems: A Programmer's Perspective, Chapter 9.
///    I used the explicit free fits outline from the textbook, specifically
///    regarding how to implement left and right coalescing. I even used their suggested
///    optimization of an extra control bit so that the footers to the left can be overwritten
///    if the block is allocated so the user can have more space. I also took their basic outline
///    for a segregated fits list to implement this allocator.
///
#include "allocator.h"
#include "list_segregated_utilities.h"
#include "print_utility.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///  Static Heap Tracking ///

// NOLINTBEGIN(*-non-const-global-variables)

/// Size Order Classes Maintained by an Array of segregated fits lists
///     - Our size classes stand for the minimum size of a node in the list less
///     than the next.
///     - 15 Size Classes (in bytes):
///
///          32,         40,          48,           56,           64,
///          72,         80,          128-255,      256-511,      512-1023,
///          1024-2047,    2048-4095, 4096-8191,    8192-16383,   16384-32767,
///          32768-65535,  65536+, last bucket carries the rest.
///
///     - A first fit search will yeild approximately the best fit.
///     - We will have one dummy node to serve as both the head and tail of all
///     lists.
///     - Be careful, last index is USHRT_MAX=65535!=65536. Mind the last index
///     size.
static struct fits
{
    seg_node *table;
    // One node can serve as the head and tail of all lists to allow some invariant code patterns.
    free_node *nil;
    size_t total;
} fits;

static struct heap
{
    void *client_start;
    void *client_end;
    size_t client_size;
} heap;

/// Count leading zeros of the input LEADING_ZEROS( x )
/// Credit https://github.com/pavel-kirienko/o1heap/tree/master.
#define LEADING_ZEROS __builtin_clzl

// NOLINTEND(*-non-const-global-variables)

///   Static Helper Functions  ///

/// @brief find_index  finds the index in the lookup table that a given block size is stored in.
/// @param block_size  the current block we are trying to find table index for.
/// @return            the index in the lookup table.
/// @citation          the bit manipulation is taken from Sean Anderson's Bit Twiddling Hacks.
///                    https://graphics.stanford.edu/~seander/bithacks.html
static inline size_t find_index( size_t any_block_size )
{
    // These are not powers of two so log2 tricks will not work and 0 is undefined! But they are
    // garuanteed to be rounded to the nearest HEADERSIZE/ALIGNMENT with a required min size.
    switch ( any_block_size ) {
    case INDEX_0_BYTES:
        return 0;
    case INDEX_1_BYTES:
        return 1;
    case INDEX_2_BYTES:
        return 2;
    case INDEX_3_BYTES:
        return 3;
    case INDEX_4_BYTES:
        return 4;
    case INDEX_5_BYTES:
        return 5;
    case INDEX_6_BYTES:
        return 6;
    default: {
        // Really cool way to get log2 from intrinsics. See https://github.com/pavel-kirienko/o1heap/tree/master.
        const size_t index_from_floored_log2 = ( (uint_fast8_t)( ( sizeof( any_block_size ) * CHAR_BIT ) - 1U )
                                                 - ( (uint_fast8_t)LEADING_ZEROS( any_block_size ) ) );
        return index_from_floored_log2 > NUM_BUCKETS - 1 ? NUM_BUCKETS - 1 : index_from_floored_log2;
    }
    }
}

/// @brief splice_free_node  removes a free node out of the free node list.
/// @param *to_splice        the heap node that we are either allocating or splitting.
/// @param *block_size       number of bytes that is used by the block we are splicing.
static void splice_free_node( free_node *to_splice, size_t block_size )
{
    // Catch if we are the first node pointed to by the lookup table.
    if ( fits.nil == to_splice->prev ) {
        fits.table[find_index( block_size )].start = to_splice->next;
        to_splice->next->prev = fits.nil;
    } else {
        // Because we have a sentinel we don't need to worry about middle or last node or NULL.
        to_splice->next->prev = to_splice->prev;
        to_splice->prev->next = to_splice->next;
    }
    fits.total--;
}

/// @brief init_free_node  initializes the header and footer of a free node, informs the right
///                        neighbor of its status and ads the node to the explicit free list.
/// @param to_add          the newly freed header for a heap_node to prepare for the free list.
/// @param block_size      the size of free memory that will now be added to the free list.
static void init_free_node( header *to_add, size_t block_size )
{
    *to_add = LEFT_ALLOCATED | block_size;
    header *footer = (header *)( (uint8_t *)to_add + block_size - ALIGNMENT );
    *footer = *to_add;
    header *neighbor = get_right_header( to_add, block_size );
    *neighbor &= LEFT_FREE;
    free_node *free_add = get_free_node( to_add );

    size_t index = 0;
    for ( ; index < NUM_BUCKETS - 1 && block_size >= fits.table[index + 1].size; ++index ) {}
    // For speed push nodes to front of the list. We are loosely sorted by at most powers of 2.
    free_node *cur = fits.table[index].start;
    fits.table[index].start = free_add;
    free_add->prev = fits.nil;
    free_add->next = cur;
    cur->prev = free_add;
    fits.total++;
}

/// @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
///                      split, it will add the newly freed split block to the free list.
/// @param *free_block   a pointer to the node for a free block in its entirety.
/// @param request       the user request for space.
/// @param block_space   the entire space that we have to work with.
/// @return              a void poiter to generic space that is now ready for the client.
static void *split_alloc( header *free_block, size_t request, size_t block_space )
{
    if ( block_space >= request + MIN_BLOCK_SIZE ) {
        // This takes care of the neighbor and ITS neighbor with appropriate updates.
        init_free_node( get_right_header( free_block, request ), block_space - request );
        init_header( free_block, request, ALLOCATED );
        return get_free_node( free_block );
    }
    *get_right_header( free_block, block_space ) |= LEFT_ALLOCATED;
    init_header( free_block, block_space, ALLOCATED );
    return get_free_node( free_block );
}

/// @brief *coalesce           performs an in place coalesce to the right and left on free and
///                            realloc. It will free any blocks that it coalesces, but it is the
///                            caller's responsibility to add a footer or add the returned block to
///                            the free list. This protects the caller from overwriting user data
///                            with a footer in the case of a minor reallocation in place.
/// @param *leftmost_header    the block which may move left. It may also gain space to the right.
/// @return                    a pointer to the leftmost node after coalescing. It may move. It is
///                            the caller's responsibility to ensure they do not overwrite user data
///                            or forget to add this node to free list, whichever they are doing.
static header *coalesce( header *leftmost_header )
{
    size_t coalesced_space = get_size( *leftmost_header );
    header *right_space = get_right_header( leftmost_header, coalesced_space );
    if ( right_space != heap.client_end && !is_block_allocated( *right_space ) ) {
        size_t block_size = get_size( *right_space );
        coalesced_space += block_size;
        splice_free_node( get_free_node( right_space ), block_size );
    }

    if ( is_left_space( leftmost_header ) ) {
        leftmost_header = get_left_header( leftmost_header );
        size_t block_size = get_size( *leftmost_header );
        coalesced_space += block_size;
        splice_free_node( get_free_node( leftmost_header ), block_size );
    }
    init_header( leftmost_header, coalesced_space, FREED );
    return leftmost_header;
}

///   Shared Heap Functions  ///

size_t get_free_total( void ) { return fits.total; }

bool myinit( void *heap_start, size_t heap_size )
{
    if ( heap_size < MIN_BLOCK_SIZE ) {
        return false;
    }

    heap.client_size = roundup( heap_size, ALIGNMENT );
    // This costs some memory in exchange for ease of use and low instruction counts.
    fits.nil = (free_node *)( (uint8_t *)heap_start + ( heap.client_size - FREE_NODE_WIDTH ) );
    fits.nil->prev = NULL;
    fits.nil->next = NULL;

    // Initialize array of free list sizes.
    heap_start = (seg_node( * )[NUM_BUCKETS])heap_start;
    fits.table = heap_start;
    // Small sizes go from 32 to 56 by increments of 8, and lists will only hold those sizes
    size_t size = MIN_BLOCK_SIZE;
    for ( size_t index = 0; index < NUM_SMALL_BUCKETS; index++, size += ALIGNMENT ) {
        fits.table[index].size = size;
        fits.table[index].start = fits.nil;
    }
    // Large sizes double until end of array except last index needs special attention.
    size = LARGE_TABLE_MIN_BYTES;
    for ( size_t index = NUM_SMALL_BUCKETS; index < NUM_BUCKETS - 1; index++, size *= 2 ) {
        fits.table[index].size = size;
        fits.table[index].start = fits.nil;
    }
    // Be careful here. We can't double to get 14th index. USHRT_MAX=65535 not 65536.
    fits.table[NUM_BUCKETS - 1].size = USHRT_MAX;
    fits.table[NUM_BUCKETS - 1].start = fits.nil;

    header *first_block = (header *)( (uint8_t *)heap_start + TABLE_BYTES );
    init_header( first_block, heap.client_size - TABLE_BYTES - FREE_NODE_WIDTH, FREED );
    init_footer( first_block, heap.client_size - TABLE_BYTES - FREE_NODE_WIDTH );

    free_node *first_free = (free_node *)( (uint8_t *)first_block + ALIGNMENT );
    first_free->next = fits.nil;
    first_free->prev = fits.nil;
    // Insert this first free into the appropriately sized list.
    init_free_node( first_block, heap.client_size - TABLE_BYTES - FREE_NODE_WIDTH );

    heap.client_start = first_block;
    heap.client_end = fits.nil;
    fits.total = 1;
    return true;
}

void *mymalloc( size_t requested_size )
{
    if ( requested_size == 0 || requested_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    size_t rounded_request = roundup( requested_size + HEADER_AND_FREE_NODE, ALIGNMENT );
    // We are starting with a pretty good guess thanks to log2 properties but we might not find anything.
    for ( size_t i = find_index( rounded_request ); i < NUM_BUCKETS; ++i ) {
        for ( free_node *node = fits.table[i].start; node != fits.nil; node = node->next ) {
            header *cur_header = get_block_header( node );
            size_t free_space = get_size( *cur_header );
            if ( free_space >= rounded_request ) {
                splice_free_node( node, free_space );
                return split_alloc( cur_header, rounded_request, free_space );
            }
        }
        // Whoops the best fitting estimate list was empty or didn't have our size! We'll try the next range up.
    }
    // This should be strange. The heap would be exhausted.
    return NULL;
}

void *myrealloc( void *old_ptr, size_t new_size )
{
    if ( new_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    if ( old_ptr == NULL ) {
        return mymalloc( new_size );
    }
    if ( new_size == 0 ) {
        myfree( old_ptr );
        return NULL;
    }
    size_t size_needed = roundup( new_size + HEADER_AND_FREE_NODE, ALIGNMENT );
    header *old_header = get_block_header( old_ptr );
    size_t old_space = get_size( *old_header );

    // Spec requires we coalesce as much as possible even if there were sufficient space in place.
    header *leftmost_header = coalesce( old_header );
    size_t coalesced_total = get_size( *leftmost_header );
    void *client_block = get_free_node( leftmost_header );

    if ( coalesced_total >= size_needed ) {
        // memmove seems bad but if we just coalesced right and did not find space, I would have to
        // search free list, update free list, split the block, memcpy, add a split block back
        // to the free list, update all headers, then come back to left coalesce the space we left
        // behind. This is fewer operations and I did not measure a significant time cost.
        if ( leftmost_header != old_header ) {
            memmove( client_block, old_ptr, old_space ); // NOLINT(*DeprecatedOrUnsafeBufferHandling)
        }
        return split_alloc( leftmost_header, size_needed, coalesced_total );
    }
    client_block = mymalloc( size_needed );
    if ( client_block ) {
        memcpy( client_block, old_ptr, old_space ); // NOLINT(*DeprecatedOrUnsafeBufferHandling)
        init_free_node( leftmost_header, coalesced_total );
    }
    // NULL or the space we found from in-place or malloc.
    return client_block;
}

void myfree( void *ptr )
{
    if ( ptr != NULL ) {
        header *to_free = get_block_header( ptr );
        to_free = coalesce( to_free );
        init_free_node( to_free, get_size( *to_free ) );
    }
}

///     Shared Debugger      ///

bool validate_heap( void )
{
    if ( !check_init( fits.table, fits.nil, heap.client_size ) ) {
        return false;
    }
    size_t total_free_mem = 0;
    if ( !is_memory_balanced( &total_free_mem, ( heap_range ){ heap.client_start, heap.client_end },
                              ( size_total ){ heap.client_size, fits.total } ) ) {
        return false;
    }
    if ( !are_fits_valid( total_free_mem, fits.table, fits.nil ) ) {
        return false;
    }
    return true;
}

///     Shared Printer       ///

void print_free_nodes( print_style style ) { print_fits( style, fits.table, fits.nil ); }

void dump_heap( void )
{
    print_all( ( heap_range ){ heap.client_start, heap.client_end }, heap.client_size, fits.table, fits.nil );
}
