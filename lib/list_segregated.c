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
#include "debug_break.h"
#include "print_utility.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////// Type Definitions  ///////////////////////////////////////

typedef size_t header;

struct free_node
{
    struct free_node *next;
    struct free_node *prev;
};

/// The segregated lists table we set up at instantiation takes up a decent chunk of space.
/// So to compromise we can ensure we take up less space per segment header with uint16_t.
/// This limits us however to a final bucket being the catch all for any request greater
/// than or equal to USHRT_MAX. If we want more powers of 2 segments we will need to change
/// this to the wider uint32_t.
struct seg_node
{
    uint16_t size;
    struct free_node *start;
};

struct heap_range
{
    void *start;
    void *end;
};

struct size_total
{
    size_t byte_size;
    size_t count_total;
};

struct header_size
{
    header header;
    size_t size;
};

struct bad_jump
{
    header *current;
    header *prev;
};

enum bucket_sizes
{
    NUM_BUCKETS = 17,
    NUM_SMALL_BUCKETS = 7,
};

enum bucket_bytes
{
    INDEX_0_BYTES = 32,
    INDEX_1_BYTES = 40,
    INDEX_2_BYTES = 48,
    INDEX_3_BYTES = 56,
    INDEX_4_BYTES = 64,
    INDEX_5_BYTES = 72,
    INDEX_6_BYTES = 80,
    I0 = 0,
    I1 = 1,
    I2 = 2,
    I3 = 3,
    I4 = 4,
    I5 = 5,
    I6 = 6,
    SMALL_TABLE_MAX_BYTES = INDEX_6_BYTES,
    /// This means our first log2 bucket index calculation yeilds 7 for the 0b1000_0000 bit.
    /// We then start doubling from here. 128, 256, 512, etc. Probably should profile to pick sizes.
    LARGE_TABLE_MIN_BYTES = 128,
    TOTAL_TABLE_BYTES = ( NUM_BUCKETS * sizeof( struct seg_node ) ),
};

/// Unsigned bitwise helpers we can't put into enums.
#define SIZE_MASK ~0x7UL
#define STATUS_CHECK 0x4UL
#define FREE_NODE_WIDTH sizeof( struct free_node )
#define HEADER_AND_FREE_NODE ( sizeof( header ) + sizeof( struct free_node ) )
#define HEADERSIZE sizeof( size_t )
#define MIN_BLOCK_SIZE (uint16_t)32
#define FREED 0x0UL
#define ALLOCATED 0x1UL
#define LEFT_ALLOCATED 0x2UL
#define LEFT_FREE ~0x2UL

///  Static Heap Tracking ///

// NOLINTBEGIN(*-non-const-global-variables)

/// Size Order Classes Maintained by an Array of segregated fits lists
///     - Our size classes stand for the minimum size of a node in the list less
///     than the next.
///     - 17 Size Classes (in bytes):
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
    struct seg_node table[NUM_BUCKETS];
    // One node can serve as the head and tail of all lists to allow some invariant code patterns.
    struct free_node nil;
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

/////////////////////////////  Forward Declarations  ///////////////////////////////////////

// This way you can read through the core implementation and helpers go to bottom.

static size_t roundup( size_t requested_size, size_t multiple );
static size_t get_size( header header_val );
static header *get_right_header( header *cur_header, size_t block_size );
static header *get_left_header( header *cur_header );
static bool is_block_allocated( header header_val );
static struct free_node *get_free_node( header *cur_header );
static header *get_block_header( struct free_node *user_mem_space );
static void init_header( header *cur_header, size_t block_size, header header_status );
static void init_footer( header *cur_header, size_t block_size );
static bool is_left_space( const header *cur_header );
static size_t find_index( size_t any_block_size );
static void splice_at_index( struct free_node *to_splice, size_t i );
static void init_free_node( header *to_add, size_t block_size );
static void *split_alloc( header *free_block, size_t request, size_t block_space );
static header *coalesce( header *leftmost_header );
static bool check_init( struct seg_node table[], size_t client_size );
static bool is_memory_balanced( size_t *total_free_mem, struct heap_range hr, struct size_total st );
static bool are_fits_valid( size_t total_free_mem, struct seg_node table[], struct free_node *nil );
static void print_all( struct heap_range hr, size_t client_size, struct seg_node table[], struct free_node *nil );
static void print_fits( enum print_style style, struct seg_node table[], struct free_node *nil );

///////////////////////////   Shared Heap Functions  ///////////////////////////////////////////

size_t get_free_total( void ) { return fits.total; }

bool myinit( void *heap_start, size_t heap_size )
{
    if ( heap_size < MIN_BLOCK_SIZE ) {
        return false;
    }

    heap.client_size = ( heap_size + HEADERSIZE - 1 ) & ~( HEADERSIZE - 1 );
    // Small sizes go from 32 to 56 by increments of 8, and lists will only hold those sizes
    size_t size = MIN_BLOCK_SIZE;
    for ( size_t index = 0; index < NUM_SMALL_BUCKETS; index++, size += ALIGNMENT ) {
        fits.table[index].size = size;
        fits.table[index].start = &fits.nil;
    }
    // Large sizes double until end of array except last index needs special attention.
    size = LARGE_TABLE_MIN_BYTES;
    for ( size_t index = NUM_SMALL_BUCKETS; index < NUM_BUCKETS - 1; index++, size *= 2 ) {
        fits.table[index].size = size;
        fits.table[index].start = &fits.nil;
    }
    // Be careful here. We can't double to get 14th index. USHRT_MAX=65535 not 65536.
    fits.table[NUM_BUCKETS - 1].size = USHRT_MAX;
    fits.table[NUM_BUCKETS - 1].start = &fits.nil;

    header *first_block = (header *)( heap_start );
    // This makes it so we don't have to check for an edge case when coalescing right. It will always be allocated.
    header *dummy_header = (header *)( (uint8_t *)( heap_start ) + heap.client_size - sizeof( header ) );
    *dummy_header = ALLOCATED;
    init_header( first_block, heap.client_size - sizeof( header ), FREED );
    init_footer( first_block, heap.client_size - sizeof( header ) );

    struct free_node *first_free = (struct free_node *)( (uint8_t *)first_block + ALIGNMENT );
    first_free->next = &fits.nil;
    first_free->prev = &fits.nil;
    // Insert this first free into the appropriately sized list.
    init_free_node( first_block, heap.client_size - sizeof( header ) );

    heap.client_start = first_block;
    heap.client_end = dummy_header;
    fits.total = 1;
    fits.nil.prev = NULL;
    fits.nil.next = NULL;
    return true;
}

void *mymalloc( size_t requested_size )
{
    if ( requested_size == 0 || requested_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    const size_t rounded_request = roundup( requested_size, ALIGNMENT );
    // We are starting with a pretty good guess thanks to log2 properties but we might not find anything.
    for ( size_t i = find_index( rounded_request ); i < NUM_BUCKETS; ++i ) {
        for ( struct free_node *node = fits.table[i].start; node != &fits.nil; node = node->next ) {
            header *cur_header = get_block_header( node );
            size_t free_space = get_size( *cur_header );
            if ( free_space >= rounded_request ) {
                splice_at_index( node, i );
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
    size_t size_needed = roundup( new_size, ALIGNMENT );
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

///////////////////////     Shared Debugger      ///////////////////////////////////////////

bool validate_heap( void )
{
    if ( !check_init( fits.table, heap.client_size ) ) {
        return false;
    }
    size_t total_free_mem = 0;
    if ( !is_memory_balanced( &total_free_mem, ( struct heap_range ){ heap.client_start, heap.client_end },
                              ( struct size_total ){ heap.client_size, fits.total } ) ) {
        return false;
    }
    if ( !are_fits_valid( total_free_mem, fits.table, &fits.nil ) ) {
        return false;
    }
    return true;
}

size_t align( size_t request ) { return roundup( request, ALIGNMENT ) - HEADERSIZE; }

size_t capacity( void )
{
    size_t total_free_mem = 0;
    size_t block_size_check = 0;
    for ( header *cur = heap.client_start; cur != heap.client_end;
          cur = get_right_header( cur, block_size_check ) ) {
        block_size_check = get_size( *cur );
        if ( !is_block_allocated( *cur ) ) {
            total_free_mem += block_size_check;
        }
    }
    return total_free_mem;
}

void validate_heap_state( const struct heap_block expected[], struct heap_block actual[], size_t len )
{
    (void)expected;
    (void)actual;
    (void)len;
    UNIMPLEMENTED();
}

////////////////////////////     Shared Printer       ///////////////////////////////////////

void print_free_nodes( enum print_style style ) { print_fits( style, fits.table, &fits.nil ); }

void dump_heap( void )
{
    print_all( ( struct heap_range ){ heap.client_start, heap.client_end }, heap.client_size, fits.table,
               &fits.nil );
}

//////////////////////////////   Static Helper Functions  ////////////////////////////////////

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
        splice_at_index( get_free_node( right_space ), find_index( block_size ) );
    }

    if ( is_left_space( leftmost_header ) ) {
        leftmost_header = get_left_header( leftmost_header );
        size_t block_size = get_size( *leftmost_header );
        coalesced_space += block_size;
        splice_at_index( get_free_node( leftmost_header ), find_index( block_size ) );
    }
    init_header( leftmost_header, coalesced_space, FREED );
    return leftmost_header;
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
    struct free_node *free_add = get_free_node( to_add );

    size_t index = find_index( block_size );
    // For speed push nodes to front of the list. We are loosely sorted by at most powers of 2.
    struct free_node *cur = fits.table[index].start;
    fits.table[index].start = free_add;
    free_add->prev = &fits.nil;
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

/// @brief find_index  finds the index in the lookup table that a given block size is stored in.
/// @param block_size  the current block we are trying to find table index for.
/// @return            the index in the lookup table.
/// @warning           assumes any_block_size is aligned to ALIGNMENT aka HEADERSIZE.
/// @citation          really cool way to get log2 from intrinsics.
///                    https://github.com/pavel-kirienko/o1heap/tree/master.
static inline size_t find_index( size_t any_block_size )
{
    switch ( any_block_size ) {
    case INDEX_0_BYTES:
        return I0;
    case INDEX_1_BYTES:
        return I1;
    case INDEX_2_BYTES:
        return I2;
    case INDEX_3_BYTES:
        return I3;
    case INDEX_4_BYTES:
        return I4;
    case INDEX_5_BYTES:
        return I5;
    case INDEX_6_BYTES:
        return I6;
    default: {
        const size_t index_from_floored_log2 = ( (uint_fast8_t)( ( sizeof( any_block_size ) * CHAR_BIT ) - 1U )
                                                 - ( (uint_fast8_t)LEADING_ZEROS( any_block_size ) ) );
        return index_from_floored_log2 > NUM_BUCKETS - 1 ? NUM_BUCKETS - 1 : index_from_floored_log2;
    }
    }
}

/////////////////////////////   Basic Block and Header Operations   ////////////////////////////

/// @brief splice_block_size removes a free node out of the free node list with known starting index.
/// @param *to_splice        the heap node that we are either allocating or splitting.
/// @param i                 table index for the power of two to which we know this block belongs.
static inline void splice_at_index( struct free_node *to_splice, size_t i )
{
    // Catch if we are the first node pointed to by the lookup table.
    if ( &fits.nil == to_splice->prev ) {
        fits.table[i].start = to_splice->next;
        to_splice->next->prev = &fits.nil;
    } else {
        // Because we have a sentinel we don't need to worry about middle or last node or NULL.
        to_splice->next->prev = to_splice->prev;
        to_splice->prev->next = to_splice->next;
    }
    fits.total--;
}

/// @brief roundup         rounds up a size to the nearest multiple of two to be aligned in the heap.
/// @param requested_size  size given to us by the client.
/// @param multiple        the nearest multiple to raise our number to.
/// @return                rounded number.
static inline size_t roundup( size_t requested_size, size_t multiple )
{
    return ( requested_size + HEADERSIZE ) <= ( HEADER_AND_FREE_NODE + HEADERSIZE )
               ? HEADER_AND_FREE_NODE + HEADERSIZE
               : ( ( requested_size + multiple - 1 ) & ~( multiple - 1 ) ) + HEADERSIZE;
}

/// @brief get_size     given a valid header find the total size of the header and block.
/// @param *header_val  the pointer to the current header block we are on.
/// @return             the size in bytes of the header and memory block.
static inline size_t get_size( header header_val ) { return header_val & SIZE_MASK; }

/// @brief *get_right_header  advances the header pointer to the next header in the heap.
/// @param *cur_header        the valid pointer to a heap header within the memory range.
/// @param block_size         the size of the current block.
/// @return                   a header pointer to the next block in the heap.
static inline header *get_right_header( header *cur_header, size_t block_size )
{
    return (header *)( (uint8_t *)cur_header + block_size );
}

/// @brief *get_left_header  uses the left block size gained from the footer to move to the header.
/// @param *cur_header       the current header at which we reside.
/// @param left_block_size   the space of the left block as reported by its footer.
/// @return                  a header pointer to the header for the block to the left.
static inline header *get_left_header( header *cur_header )
{
    header *left_footer = (header *)( (uint8_t *)cur_header - HEADERSIZE );
    return (header *)( (uint8_t *)cur_header - ( *left_footer & SIZE_MASK ) );
}

/// @brief is_block_allocated  will determine if a block is marked as allocated.
/// @param *header_val         the valid header we will determine status for.
/// @return                    true if the block is allocated, false if not.
static inline bool is_block_allocated( header header_val ) { return header_val & ALLOCATED; }

/// @brief *get_client_space  get the pointer to the start of the client available memory.
/// @param *cur_header        the valid header to the current block of heap memory.
/// @return                   a pointer to the first available byte of client heap memory.
static inline struct free_node *get_free_node( header *cur_header )
{
    return (struct free_node *)( (uint8_t *)cur_header + HEADERSIZE );
}

/// @brief *get_block_header  steps to the left from the user-available space to
///                           get the pointer to the header header.
/// @param *user_mem_space    the void pointer to the space available for the user.
/// @return                   the header immediately to the left associated with memory block.
static inline header *get_block_header( struct free_node *user_mem_space )
{
    return (header *)( (uint8_t *)user_mem_space - HEADERSIZE );
}

/// @brief init_header    initializes the header in the header_header field to reflect the
///                       specified status and that the left neighbor is allocated or unavailable.
/// @param *cur_header    the header we will initialize.
/// @param block_size     the size, including the header, of the entire block.
/// @param header_status  FREE or ALLOCATED to reflect the status of the memory.
static inline void init_header( header *cur_header, size_t block_size, header header_status )
{
    *cur_header = LEFT_ALLOCATED | block_size | header_status;
}

/// @brief init_footer  initializes the footer to reflect that the associated block is now free.
///                     We will only initialize footers on free blocks. We use
///                     the the control bits in the right neighbor if the block
///                     is allocated and allow the user to have the footer space.
/// @param *cur_header  a pointer to the header that is now free and will have a footer.
/// @param block_size   the size to use to update the footer of the block.
static inline void init_footer( header *cur_header, size_t block_size )
{
    header *footer = (header *)( (uint8_t *)cur_header + block_size - HEADERSIZE );
    *footer = LEFT_ALLOCATED | block_size | FREED;
}

/// @brief is_left_space  checks the control bit in the second position to see if the left
///                       neighbor is allocated or free to use for coalescing.
/// @param *cur_header    the current block for which we are checking the left/// neighbor.
/// @return               true if there is space to the left, false if not.
static inline bool is_left_space( const header *cur_header ) { return !( *cur_header & LEFT_ALLOCATED ); }

/////////////////////////////    Debugging and Testing Functions    /////////////////////////

static bool is_header_corrupted( header header_val ) { return header_val & STATUS_CHECK; }

static bool is_small_table_valid( struct seg_node table[] )
{
    // Check our lookup table. Sizes should never be altered and pointers should never be NULL.
    uint16_t size = MIN_BLOCK_SIZE;
    for ( size_t i = 0; i < NUM_SMALL_BUCKETS; i++, size += HEADERSIZE ) {
        if ( table[i].size != size ) {
            BREAKPOINT();
            return false;
        }
        // This should either be a valid node or the sentinel.
        if ( NULL == table[i].start ) {
            BREAKPOINT();
            return false;
        }
    }
    return true;
}

/// @brief check_init   checks the internal representation of our heap, especially the head
///                     and tail nodes for any issues that would ruin our algorithms.
/// @param table[]      the lookup table that holds the list size ranges
/// @param *nil         a special free_node that serves as a sentinel for logic and edgecases.
/// @param client_size  the total space available for client.
/// @return             true if everything is in order otherwise false.
static bool check_init( struct seg_node table[], size_t client_size )
{
    if ( (size_t)( ( (uint8_t *)heap.client_end + sizeof( header ) ) - (uint8_t *)heap.client_start )
         != client_size ) {
        BREAKPOINT();
        return false;
    }
    if ( !is_small_table_valid( table ) ) {
        BREAKPOINT();
        return false;
    }
    uint16_t size = LARGE_TABLE_MIN_BYTES;
    for ( size_t i = NUM_SMALL_BUCKETS; i < NUM_BUCKETS - 1; i++, size *= 2 ) {
        if ( table[i].size != size ) {
            BREAKPOINT();
            return false;
        }
        // This should either be a valid node or the nil.
        if ( NULL == table[i].start ) {
            BREAKPOINT();
            return false;
        }
    }
    if ( table[NUM_BUCKETS - 1].size != USHRT_MAX ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

static bool is_valid_header( struct header_size hs, size_t client_size )
{
    // Most definitely impossible and our header is corrupted. Pointer arithmetic would fail.
    if ( hs.size > client_size ) {
        BREAKPOINT();
        return false;
    }
    if ( is_header_corrupted( hs.header ) ) {
        BREAKPOINT();
        return false;
    }
    if ( hs.size % HEADERSIZE != 0 ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

/// @brief is_memory_balanced  loops through all blocks of memory to verify that the sizes
///                            reported match the global bookeeping in our struct.
/// @param *total_free_mem     the output parameter of the total size used as another check.
/// @param hr                  start and end of the heap
/// @param st                  size of the heap memory and total free nodes.
/// @return                    true if our tallying is correct and our totals match.
static bool is_memory_balanced( size_t *total_free_mem, struct heap_range hr, struct size_total st )
{
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    header *cur_header = hr.start;
    size_t size_used = sizeof( header );
    size_t total_free_nodes = 0;
    while ( cur_header != hr.end ) {
        size_t block_size_check = get_size( *cur_header );
        if ( block_size_check == 0 ) {
            BREAKPOINT();
            return false;
        }

        if ( !is_valid_header( ( struct header_size ){ *cur_header, block_size_check }, st.byte_size ) ) {
            BREAKPOINT();
            return false;
        }
        if ( is_block_allocated( *cur_header ) ) {
            size_used += block_size_check;
        } else {
            total_free_nodes++;
            *total_free_mem += block_size_check;
        }
        cur_header = get_right_header( cur_header, block_size_check );
    }
    if ( size_used + *total_free_mem != st.byte_size ) {
        BREAKPOINT();
        return false;
    }
    if ( total_free_nodes != st.count_total ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

static size_t are_links_valid( struct seg_node table[], size_t table_index, struct free_node *nil, size_t free_mem )
{
    for ( struct free_node *cur = table[table_index].start; cur != nil; cur = cur->next ) {
        header *cur_header = get_block_header( cur );
        size_t cur_size = get_size( *cur_header );
        if ( table_index != NUM_BUCKETS - 1 && cur_size >= table[table_index + 1].size ) {
            BREAKPOINT();
            return false;
        }
        if ( cur_size < table[table_index].size ) {
            BREAKPOINT();
            return false;
        }
        if ( is_block_allocated( *cur_header ) ) {
            BREAKPOINT();
            return false;
        }
        // This algorithm does not allow two free blocks to remain next to one another.
        if ( is_left_space( get_block_header( cur ) ) ) {
            BREAKPOINT();
            return false;
        }
        free_mem += cur_size;
    }
    return free_mem;
}

/// @brief are_fits_valid  loops through only the segregated fits list to make sure it matches
///                        the loop we just completed by checking all blocks.
/// @param total_free_mem  the input from a previous loop that was completed by jumping block
///                        by block over the entire heap.
/// @param table[]         the lookup table that holds the list size ranges
/// @param *nil            a special free_node that serves as a sentinel for logic and edgecases.
/// @return                true if the segregated fits list totals correctly false if not.
static bool are_fits_valid( size_t total_free_mem, struct seg_node table[], struct free_node *nil )
{
    size_t linked_free_mem = 0;
    for ( size_t i = 0; i < NUM_BUCKETS; i++ ) {
        linked_free_mem = are_links_valid( table, i, nil, linked_free_mem );
    }
    if ( total_free_mem != linked_free_mem ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

/////////////////////////////        Printing Functions     //////////////////////////////////

/// @brief print_fits  prints the segregated fits free list in order to check if splicing and
///                    adding is progressing correctly.
/// @param table[]     the lookup table that holds the list size ranges
/// @param *nil        a special free_node that serves as a sentinel for logic and edgecases.
static void print_fits( enum print_style style, struct seg_node table[], struct free_node *nil )
{
    bool alternate = false;
    for ( size_t i = 0; i < NUM_BUCKETS; i++, alternate = !alternate ) {
        printf( COLOR_GRN );
        if ( style == VERBOSE ) {
            printf( "%p: ", &table[i] );
        }
        if ( i == NUM_BUCKETS - 1 ) {
            printf( "[CLASS:%ubytes+]=>", table[i].size );
        } else if ( i >= NUM_SMALL_BUCKETS ) {
            printf( "[CLASS:%u-%ubytes]=>", table[i].size, table[i + 1].size - 1U );
        } else {
            printf( "[CLASS:%ubytes]=>", table[i].size );
        }
        printf( COLOR_NIL );
        if ( alternate ) {
            printf( COLOR_RED );
        } else {
            printf( COLOR_CYN );
        }

        for ( struct free_node *cur = table[i].start; cur != nil; cur = cur->next ) {
            if ( cur ) {
                header *cur_header = get_block_header( cur );
                printf( "<=>[" );
                if ( style == VERBOSE ) {
                    printf( "%p:", get_block_header( cur ) );
                }
                printf( "(%zubytes)]", get_size( *cur_header ) );
            } else {
                printf( "Something went wrong. NULL free fits node.\n" );
                break;
            }
        }
        printf( "<=>[%p]\n", nil );
        printf( COLOR_NIL );
    }
    printf( COLOR_RED );
    printf( "<-%p:SENTINEL->\n", nil );
    printf( COLOR_NIL );
}

/// @brief print_alloc_block  prints the contents of an allocated block of memory.
/// @param *cur_header        a valid header to a block of allocated memory.
static void print_alloc_block( header *cur_header )
{
    size_t block_size = get_size( *cur_header ) - HEADERSIZE;
    printf( COLOR_GRN );
    // We will see from what direction our header is messed up by printing 16 digits.
    printf( "%p: HEADER->0x%016zX->[ALOC-%zubytes]\n", cur_header, *cur_header, block_size );
    printf( COLOR_NIL );
}

/// @brief print_free_block  prints the contents of a free block of heap memory.
/// @param *cur_header       a valid header to a block of allocated memory.
static void print_free_block( header *cur_header )
{
    size_t full_size = get_size( *cur_header );
    size_t block_size = full_size - HEADERSIZE;
    header *footer = (header *)( (uint8_t *)cur_header + full_size - HEADERSIZE );
    // We should be able to see the header is the same as the footer. If they are
    // not the same we will face subtle bugs that are very hard to notice.
    if ( *footer != *cur_header ) {
        *footer = ULONG_MAX;
    }
    printf( COLOR_RED );
    printf( "%p: HEADER->0x%016zX->[FREE-%zubytes->FOOTER->%016zX]\n", cur_header, *cur_header, block_size,
            *footer );
    printf( COLOR_NIL );
}

/// @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
///                        notice where we went wrong and what the addresses were.
/// @param bad_jump        two nodes with a bad jump from one to the other
/// @param table[]         the lookup table that holds the list size ranges
/// @param *nil            a special free_node that serves as a sentinel for logic and edgecases.
static void print_bad_jump( struct bad_jump j, struct seg_node table[], struct free_node *nil )
{
    size_t prev_size = get_size( *j.prev );
    size_t cur_size = get_size( *j.current );
    printf( COLOR_CYN );
    printf( "A bad jump from the value of a header has occured. Bad distance to "
            "next header.\n" );
    printf( "The previous address: %p:\n", j.prev );
    printf( "\tHeader Hex Value: %016zX:\n", *j.prev );
    printf( "\tBlock Byte Value: %zubytes:\n", prev_size );
    printf( "\nJump by %zubytes...\n", prev_size );
    printf( "The current address: %p:\n", j.current );
    printf( "\tHeader Hex Value: %016zX:\n", *j.current );
    printf( "\tBlock Byte Value: %zubytes:\n", cur_size );
    printf( "\nJump by %zubytes...\n", cur_size );
    printf( "Current state of the free list:\n" );
    printf( COLOR_NIL );
    print_fits( VERBOSE, table, nil );
}

/// @brief print_all    prints our the complete status of the heap, all of its blocks, and the
///                     sizes the blocks occupy. Printing should be clean with
///                     no overlap of unique id's between heap blocks or corrupted headers.
/// @param hr           start and end of the heap
/// @param client_size  the size in bytes of the heap.
/// @param table[]      the lookup table of segregated sizes of nodes stored in each slot.
/// @param *nil         the free node that serves as a universal head and tail to all lists.
static void print_all( struct heap_range hr, size_t client_size, struct seg_node table[], struct free_node *nil )
{
    header *cur_header = hr.start;
    printf( "Heap client segment starts at address %p, ends %p. %zu total bytes "
            "currently used.\n",
            cur_header, hr.end, client_size );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n\n" );

    printf( "%p: FIRST ADDRESS\n", table );

    // This will create large amount of output but realistically table is before
    // the rest of heap.
    print_fits( VERBOSE, table, nil );
    printf( "--END OF LOOKUP TABLE, START OF HEAP--\n" );

    header *prev = cur_header;
    while ( cur_header != hr.end ) {
        size_t full_size = get_size( *cur_header );

        if ( full_size == 0 ) {
            print_bad_jump( ( struct bad_jump ){ cur_header, prev }, table, nil );
            printf( "Last known pointer before jump: %p", prev );
            return;
        }

        if ( is_block_allocated( *cur_header ) ) {
            print_alloc_block( cur_header );
        } else {
            print_free_block( cur_header );
        }
        prev = cur_header;
        cur_header = get_right_header( cur_header, full_size );
    }
    printf( "%p: END OF HEAP\n", hr.end );
    printf( "%p: LAST ADDRESS\n", (uint8_t *)nil + FREE_NODE_WIDTH );
    printf( "\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "\nSEGREGATED LIST OF FREE NODES AND BLOCK SIZES.\n" );
    printf( "HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n" );
    // For large heaps we wouldn't be able to scroll to the table location so
    // print again here.
    print_fits( VERBOSE, table, nil );
}
