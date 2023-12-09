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
    header header;
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
    struct free_node *current;
    struct free_node *prev;
};

struct coalesce_report
{
    struct free_node *left;
    struct free_node *current;
    struct free_node *right;
    size_t available;
};

enum bucket_sizes
{
    NUM_BUCKETS = 17,
    NUM_SMALL_BUCKETS = 7,
};

enum bucket_bytes
{
    INDEX_0_BYTES = 24,
    INDEX_1_BYTES = 32,
    INDEX_2_BYTES = 40,
    INDEX_3_BYTES = 48,
    INDEX_4_BYTES = 56,
    INDEX_5_BYTES = 64,
    INDEX_6_BYTES = 72,
    I0 = 0,
    I1 = 1,
    I2 = 2,
    I3 = 3,
    I4 = 4,
    I5 = 5,
    I6 = 6,
    SMALL_TABLE_MAX_BYTES = INDEX_6_BYTES,
    SMALL_TABLE_STEP = 8,
    /// This means our first log2 bucket index calculation yeilds 7 for the 0b1000_0000 bit.
    /// We then start doubling from here. 128, 256, 512, etc. Probably should profile to pick sizes.
    LARGE_TABLE_MIN_BYTES = 128,
    TOTAL_TABLE_BYTES = ( NUM_BUCKETS * sizeof( struct seg_node ) ),
};

/// Unsigned bitwise helpers we can't put into enums.
#define SIZE_MASK ~0x7UL
#define STATUS_CHECK 0x4UL
#define HEAP_NODE_WIDTH sizeof( struct free_node )
#define HEADERSIZE sizeof( size_t )
#define MIN_BLOCK_SIZE ( sizeof( struct free_node ) + HEADERSIZE )
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
static struct free_node *get_right_neighbor( struct free_node *cur_header, size_t block_size );
static struct free_node *get_left_neighbor( struct free_node *cur_header );
static bool is_block_allocated( header header_val );
static void *get_client_space( struct free_node *cur_header );
static struct free_node *get_free_node( const void *user_mem_space );
static inline void init_header( struct free_node *cur_header, size_t block_size, header header_status );
static void init_footer( struct free_node *cur_header, size_t block_size );
static bool is_left_space( header cur_header );
static size_t find_index( size_t any_block_size );
static void splice_at_index( struct free_node *to_splice, size_t i );
static void init_free_node( struct free_node *to_add, size_t block_size );
static void *split_alloc( struct free_node *free_block, size_t request, size_t block_space );
static struct coalesce_report check_neighbors( const void *old_ptr );
static void coalesce( struct coalesce_report *report );
static bool check_init( struct seg_node table[], size_t client_size );
static bool is_memory_balanced( size_t *total_free_mem, struct heap_range hr, struct size_total st );
static bool are_fits_valid( size_t total_free_mem, struct seg_node table[], struct free_node *nil );
static void print_all( struct heap_range hr, size_t client_size, struct seg_node table[], struct free_node *nil );
static void print_fits( enum print_style style, struct seg_node table[], struct free_node *nil );

///////////////////////////   Shared Heap Functions  ///////////////////////////////////////////

size_t wget_free_total( void )
{
    return fits.total;
}

bool winit( void *heap_start, size_t heap_size )
{
    if ( heap_size < MIN_BLOCK_SIZE ) {
        return false;
    }

    heap.client_start = heap_start;
    heap.client_size = roundup( heap_size, ALIGNMENT );
    heap.client_end = (uint8_t *)heap.client_start + heap.client_size - HEAP_NODE_WIDTH;
    fits.nil.prev = NULL;
    fits.nil.next = NULL;
    // Small sizes go from 32 to 56 by increments of 8, and lists will only hold those sizes
    size_t size = INDEX_0_BYTES;
    for ( size_t index = 0; index < NUM_SMALL_BUCKETS; index++, size += SMALL_TABLE_STEP ) {
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

    struct free_node *first_block = heap.client_start;
    // This makes it so we don't have to check for an edge case when coalescing right. It will always be allocated.
    struct free_node *dummy_block = heap.client_end;
    dummy_block->header = ALLOCATED;
    init_header( first_block, heap.client_size - HEAP_NODE_WIDTH - HEADERSIZE, FREED );
    init_footer( first_block, heap.client_size - HEAP_NODE_WIDTH - HEADERSIZE );

    first_block->next = &fits.nil;
    first_block->prev = &fits.nil;
    // Insert this first free into the appropriately sized list.
    init_free_node( first_block, heap.client_size - HEAP_NODE_WIDTH - HEADERSIZE );
    fits.total = 1;
    return true;
}

void *wmalloc( size_t requested_size )
{
    if ( requested_size == 0 || requested_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    const size_t rounded_request = roundup( requested_size, ALIGNMENT );
    // We are starting with a pretty good guess thanks to log2 properties but we might not find anything.
    for ( size_t i = find_index( rounded_request ); i < NUM_BUCKETS; ++i ) {
        for ( struct free_node *node = fits.table[i].start; node != &fits.nil; node = node->next ) {
            size_t free_space = get_size( node->header );
            if ( free_space >= rounded_request ) {
                splice_at_index( node, i );
                return split_alloc( node, rounded_request, free_space );
            }
        }
        // Whoops the best fitting estimate list was empty or didn't have our size! We'll try the next range up.
    }
    // This should be strange. The heap would be exhausted.
    return NULL;
}

void *wrealloc( void *old_ptr, size_t new_size )
{
    if ( new_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    if ( old_ptr == NULL ) {
        return wmalloc( new_size );
    }
    if ( new_size == 0 ) {
        wfree( old_ptr );
        return NULL;
    }
    size_t request = roundup( new_size, ALIGNMENT );
    struct coalesce_report report = check_neighbors( old_ptr );
    size_t old_size = get_size( report.current->header );
    if ( report.available >= request ) {
        coalesce( &report );
        if ( report.current == report.left ) {
            memmove( get_client_space( report.current ), old_ptr, old_size ); // NOLINT(*UnsafeBufferHandling)
        }
        return split_alloc( report.current, request, report.available );
    }
    void *elsewhere = wmalloc( request );
    // No data has moved or been modified at this point we will will just do nothing.
    if ( !elsewhere ) {
        return NULL;
    }
    memcpy( elsewhere, old_ptr, old_size ); // NOLINT(*UnsafeBufferHandling)
    coalesce( &report );
    init_free_node( report.current, report.available );
    return elsewhere;
}

void wfree( void *ptr )
{
    if ( ptr == NULL ) {
        return;
    }
    struct coalesce_report report = check_neighbors( ptr );
    coalesce( &report );
    init_free_node( report.current, get_size( report.current->header ) );
}

///////////////////////     Shared Debugger      ///////////////////////////////////////////

bool wvalidate_heap( void )
{
    if ( !check_init( fits.table, heap.client_size ) ) {
        return false;
    }
    size_t total_free_mem = 0;
    if ( !is_memory_balanced(
             &total_free_mem,
             ( struct heap_range ){ heap.client_start, heap.client_end },
             ( struct size_total ){ heap.client_size, fits.total }
         ) ) {
        return false;
    }
    if ( !are_fits_valid( total_free_mem, fits.table, &fits.nil ) ) {
        return false;
    }
    return true;
}

size_t wheap_align( size_t request )
{
    return roundup( request, ALIGNMENT );
}

size_t wheap_capacity( void )
{
    size_t total_free_mem = 0;
    size_t block_payload = 0;
    for ( struct free_node *cur = heap.client_start; cur != heap.client_end;
          cur = get_right_neighbor( cur, block_payload ) ) {
        block_payload = get_size( cur->header );
        if ( !is_block_allocated( cur->header ) ) {
            total_free_mem += block_payload;
        }
    }
    return total_free_mem;
}

void wheap_diff( const struct heap_block expected[], struct heap_block actual[], size_t len )
{
    struct free_node *cur_node = heap.client_start;
    size_t i = 0;
    for ( ; i < len && cur_node != heap.client_end; ++i ) {
        bool is_allocated = is_block_allocated( cur_node->header );
        const size_t next_jump = get_size( cur_node->header );
        size_t cur_payload = get_size( cur_node->header );
        void *client_addr = get_client_space( cur_node );
        if ( ( !expected[i].address && is_allocated )
             || ( expected[i].address && expected[i].address != client_addr ) ) {
            actual[i] = ( struct heap_block ){
                client_addr,
                cur_payload,
                ER,
            };
        } else if ( NA == expected[i].payload_bytes ) {
            actual[i] = ( struct heap_block ){
                is_allocated ? client_addr : NULL,
                NA,
                OK,
            };
        } else if ( expected[i].payload_bytes != cur_payload ) {
            actual[i] = ( struct heap_block ){
                is_allocated ? client_addr : NULL,
                cur_payload,
                ER,
            };
        } else {
            actual[i] = ( struct heap_block ){
                is_allocated ? client_addr : NULL,
                cur_payload,
                OK,
            };
        }
        cur_node = get_right_neighbor( cur_node, next_jump );
    }
    if ( i < len ) {
        for ( size_t fill = i; fill < len; ++fill ) {
            actual[fill].err = OUT_OF_BOUNDS;
        }
        return;
    }
    if ( cur_node != heap.client_end ) {
        actual[i].err = HEAP_CONTINUES;
    }
}

////////////////////////////     Shared Printer       ///////////////////////////////////////

void wprint_free_nodes( enum print_style style )
{
    print_fits( style, fits.table, &fits.nil );
}

void wdump_heap( void )
{
    print_all(
        ( struct heap_range ){ heap.client_start, heap.client_end }, heap.client_size, fits.table, &fits.nil
    );
}

//////////////////////////////   Static Helper Functions  ////////////////////////////////////

static void *split_alloc( struct free_node *free_block, size_t request, size_t block_space )
{
    if ( block_space >= request + MIN_BLOCK_SIZE ) {
        // This takes care of the neighbor and ITS neighbor with appropriate updates.
        init_free_node( get_right_neighbor( free_block, request ), block_space - request - HEADERSIZE );
        init_header( free_block, request, ALLOCATED );
        return get_client_space( free_block );
    }
    get_right_neighbor( free_block, block_space )->header |= LEFT_ALLOCATED;
    init_header( free_block, block_space, ALLOCATED );
    return get_client_space( free_block );
}

static void init_free_node( struct free_node *to_add, size_t block_size )
{
    to_add->header = LEFT_ALLOCATED | block_size;
    header *footer = (header *)( (uint8_t *)to_add + block_size );
    *footer = to_add->header;
    struct free_node *neighbor = get_right_neighbor( to_add, block_size );
    neighbor->header &= LEFT_FREE;

    size_t index = find_index( block_size );
    // For speed push nodes to front of the list. We are loosely sorted by at most powers of 2.
    struct free_node *cur = fits.table[index].start;
    fits.table[index].start = to_add;
    to_add->prev = &fits.nil;
    to_add->next = cur;
    cur->prev = to_add;
    fits.total++;
}

static struct coalesce_report check_neighbors( const void *old_ptr )
{
    struct free_node *current_node = get_free_node( old_ptr );
    const size_t original_space = get_size( current_node->header );
    struct coalesce_report result = { NULL, current_node, NULL, original_space };

    struct free_node *rightmost_node = get_right_neighbor( current_node, original_space );
    if ( !is_block_allocated( rightmost_node->header ) ) {
        result.available += get_size( rightmost_node->header ) + HEADERSIZE;
        result.right = rightmost_node;
    }

    if ( current_node != heap.client_start && is_left_space( current_node->header ) ) {
        result.left = get_left_neighbor( current_node );
        result.available += get_size( result.left->header ) + HEADERSIZE;
    }
    return result;
}

static inline void coalesce( struct coalesce_report *report )
{
    if ( report->left ) {
        report->current = report->left;
        splice_at_index( report->left, find_index( get_size( report->left->header ) ) );
    }
    if ( report->right ) {
        splice_at_index( report->right, find_index( get_size( report->right->header ) ) );
    }
    init_header( report->current, report->available, FREED );
}

/// @citation cool way to get log2 from intrinsics. https://github.com/pavel-kirienko/o1heap/tree/master.
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
        const size_t index_from_floored_log2
            = ( (uint_fast8_t)( ( sizeof( any_block_size ) * CHAR_BIT ) - 1U )
                - ( (uint_fast8_t)LEADING_ZEROS( any_block_size ) ) );
        return index_from_floored_log2 > NUM_BUCKETS - 1 ? NUM_BUCKETS - 1 : index_from_floored_log2;
    }
    }
}

/////////////////////////////   Basic Block and Header Operations   ////////////////////////////

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

static inline size_t roundup( size_t requested_size, size_t multiple )
{
    return requested_size <= HEAP_NODE_WIDTH ? HEAP_NODE_WIDTH
                                             : ( ( requested_size + multiple - 1 ) & ~( multiple - 1 ) );
}

static inline size_t get_size( header header_val )
{
    return header_val & SIZE_MASK;
}

static inline struct free_node *get_right_neighbor( struct free_node *cur_header, size_t block_size )
{
    return (struct free_node *)( (uint8_t *)cur_header + HEADERSIZE + block_size );
}

static inline struct free_node *get_left_neighbor( struct free_node *cur_header )
{
    header *left_footer = (header *)( (uint8_t *)cur_header - HEADERSIZE );
    return (struct free_node *)( (uint8_t *)cur_header - ( *left_footer & SIZE_MASK ) - HEADERSIZE );
}

static inline bool is_block_allocated( header header_val )
{
    return header_val & ALLOCATED;
}

static inline void *get_client_space( struct free_node *cur_header )
{
    return (uint8_t *)cur_header + HEADERSIZE;
}

static inline struct free_node *get_free_node( const void *user_mem_space )
{
    return (struct free_node *)( (uint8_t *)user_mem_space - HEADERSIZE );
}

static inline void init_header( struct free_node *cur_header, size_t block_size, header header_status )
{
    cur_header->header = LEFT_ALLOCATED | block_size | header_status;
}

static inline void init_footer( struct free_node *cur_header, size_t block_size )
{
    header *footer = (header *)( (uint8_t *)cur_header + block_size );
    *footer = LEFT_ALLOCATED | block_size | FREED;
}

static inline bool is_left_space( const header cur_header )
{
    return !( cur_header & LEFT_ALLOCATED );
}

/////////////////////////////    Debugging and Testing Functions    /////////////////////////

static bool is_header_corrupted( header header_val )
{
    return header_val & STATUS_CHECK;
}

static bool is_small_table_valid( struct seg_node table[] )
{
    // Check our lookup table. Sizes should never be altered and pointers should never be NULL.
    uint16_t size = INDEX_0_BYTES;
    for ( size_t i = 0; i < NUM_SMALL_BUCKETS; i++, size += SMALL_TABLE_STEP ) {
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

static bool check_init( struct seg_node table[], size_t client_size )
{
    if ( (size_t)( ( (uint8_t *)heap.client_end + HEAP_NODE_WIDTH ) - (uint8_t *)heap.client_start )
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

static bool is_memory_balanced( size_t *total_free_mem, struct heap_range hr, struct size_total st )
{
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    struct free_node *cur = hr.start;
    size_t size_used = HEAP_NODE_WIDTH;
    size_t total_free_nodes = 0;
    while ( cur != hr.end ) {
        size_t block_size_check = get_size( cur->header );
        if ( block_size_check == 0 ) {
            BREAKPOINT();
            return false;
        }

        if ( !is_valid_header( ( struct header_size ){ cur->header, block_size_check }, st.byte_size ) ) {
            BREAKPOINT();
            return false;
        }
        if ( is_block_allocated( cur->header ) ) {
            size_used += block_size_check + HEADERSIZE;
        } else {
            ++total_free_nodes;
            *total_free_mem += block_size_check + HEADERSIZE;
        }
        cur = get_right_neighbor( cur, block_size_check );
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
        size_t cur_size = get_size( cur->header );
        if ( table_index != NUM_BUCKETS - 1 && cur_size >= table[table_index + 1].size ) {
            BREAKPOINT();
            return false;
        }
        if ( cur_size < table[table_index].size ) {
            BREAKPOINT();
            return false;
        }
        if ( is_block_allocated( cur->header ) ) {
            BREAKPOINT();
            return false;
        }
        // This algorithm does not allow two free blocks to remain next to one another.
        if ( is_left_space( cur->header ) ) {
            BREAKPOINT();
            return false;
        }
        free_mem += cur_size + HEADERSIZE;
    }
    return free_mem;
}

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

static void print_fits( enum print_style style, struct seg_node table[], struct free_node *nil )
{
    bool alternate = false;
    for ( size_t i = 0; i < NUM_BUCKETS; i++, alternate = !alternate ) {
        printf( COLOR_GRN );
        if ( style == VERBOSE ) {
            printf( "%p: ", &table[i] );
        }
        if ( i == NUM_BUCKETS - 1 ) {
            printf( "[FIT:%ubytes+]", table[i].size );
        } else if ( i >= NUM_SMALL_BUCKETS ) {
            printf( "[FIT:%u-%ubytes]", table[i].size, table[i + 1].size - 1U );
        } else {
            printf( "[FIT:%ubytes]", table[i].size );
        }
        printf( COLOR_NIL );
        if ( alternate ) {
            printf( COLOR_RED );
        } else {
            printf( COLOR_CYN );
        }

        for ( struct free_node *cur = table[i].start; cur != nil; cur = cur->next ) {
            if ( cur ) {
                printf( "⇄[" );
                if ( style == VERBOSE ) {
                    printf( "%p:", cur );
                }
                printf( "(%zubytes)]", get_size( cur->header ) );
            } else {
                printf( "Something went wrong. NULL free fits node.\n" );
                break;
            }
        }
        printf( "⇄[%p]\n", nil );
        printf( COLOR_NIL );
    }
    printf( COLOR_RED );
    printf( "←%p:SENTINEL→\n", nil );
    printf( COLOR_NIL );
}

static void print_alloc_block( struct free_node *cur_header )
{
    size_t block_size = get_size( cur_header->header );
    printf( COLOR_GRN );
    // We will see from what direction our header is messed up by printing 16 digits.
    printf( "%p: HEADER→0x%016zX→[ALOC-%zubytes]\n", cur_header, cur_header->header, block_size );
    printf( COLOR_NIL );
}

static void print_free_block( struct free_node *cur_header )
{
    size_t full_size = get_size( cur_header->header );
    header *footer = (header *)( (uint8_t *)cur_header + full_size );
    // We should be able to see the header is the same as the footer. If they are
    // not the same we will face subtle bugs that are very hard to notice.
    if ( *footer != cur_header->header ) {
        *footer = ULONG_MAX;
    }
    printf( COLOR_RED );
    printf(
        "%p: HEADER->0x%016zX->[FREE-%zubytes->FOOTER->%016zX]\n",
        cur_header,
        cur_header->header,
        full_size,
        *footer
    );
    printf( COLOR_NIL );
}

static void print_bad_jump( struct bad_jump j, struct seg_node table[], struct free_node *nil )
{
    size_t prev_size = get_size( j.prev->header );
    size_t cur_size = get_size( j.current->header );
    printf( COLOR_CYN );
    printf( "A bad jump from the value of a header has occured. Bad distance to "
            "next header.\n" );
    printf( "The previous address: %p:\n", j.prev );
    printf( "\tHeader Hex Value: %016zX:\n", j.prev->header );
    printf( "\tBlock Byte Value: %zubytes:\n", prev_size );
    printf( "\nJump by %zubytes...\n", prev_size );
    printf( "The current address: %p:\n", j.current );
    printf( "\tHeader Hex Value: %016zX:\n", j.current->header );
    printf( "\tBlock Byte Value: %zubytes:\n", cur_size );
    printf( "\nJump by %zubytes...\n", cur_size );
    printf( "Current state of the free list:\n" );
    printf( COLOR_NIL );
    print_fits( VERBOSE, table, nil );
}

static void print_all( struct heap_range hr, size_t client_size, struct seg_node table[], struct free_node *nil )
{
    struct free_node *cur = hr.start;
    printf(
        "Heap client segment starts at address %p, ends %p. %zu total bytes "
        "currently used.\n",
        cur,
        hr.end,
        client_size
    );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n\n" );

    printf( "%p: FIRST ADDRESS\n", table );

    // This will create large amount of output but realistically table is before the rest of heap.
    print_fits( VERBOSE, table, nil );
    printf( "--END OF LOOKUP TABLE, START OF HEAP--\n" );

    struct free_node *prev = cur;
    while ( cur != hr.end ) {
        size_t full_size = get_size( cur->header );
        if ( full_size == 0 ) {
            print_bad_jump( ( struct bad_jump ){ cur, prev }, table, nil );
            printf( "Last known pointer before jump: %p", prev );
            return;
        }

        if ( is_block_allocated( cur->header ) ) {
            print_alloc_block( cur );
        } else {
            print_free_block( cur );
        }
        prev = cur;
        cur = get_right_neighbor( cur, full_size );
    }
    printf( "%p: END OF HEAP\n", hr.end );
    printf( "%p: LAST ADDRESS\n", (uint8_t *)nil + HEAP_NODE_WIDTH );
    printf( "\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "\nSEGREGATED LIST OF FREE NODES AND BLOCK SIZES.\n" );
    printf( "HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n" );
    // For large heaps we wouldn't be able to scroll to the table location so print again here.
    print_fits( VERBOSE, table, nil );
}
