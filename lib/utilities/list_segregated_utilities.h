/// File: list_segregated_utilities.h
/// -----------------------------------
/// This file contains the interface for defining the types, methods, tests and
/// printing functions for the list_segregated allocator. It is helpful to
/// seperate out these pieces of logic from the algorithmic portion of the
/// allocator because they can crowd the allocator file making it hard to
/// navigate. It is also convenient to refer to the design of the types for the
/// allocator in one place and use the testing and printing functions to debug
/// any issues.
#ifndef LIST_SEGREGATED_UTILITIES_H
#define LIST_SEGREGATED_UTILITIES_H

#include "print_utility.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///////////////////////////// Type Definitions  ///////////////////////////////////////

typedef size_t header;

typedef struct free_node
{
    struct free_node *next;
    struct free_node *prev;
} free_node;

/// The segregated lists table we set up at instantiation takes up a decent chunk of space.
/// So to compromise we can ensure we take up less space per segment header with uint16_t.
/// This limits us however to a final bucket being the catch all for any request greater
/// than or equal to USHRT_MAX. If we want more powers of 2 segments we will need to change
/// this to the wider uint32_t.
typedef struct seg_node
{
    uint16_t size;
    free_node *start;
} seg_node;

typedef struct heap_range
{
    void *start;
    void *end;
} heap_range;

typedef struct size_total
{
    size_t size;
    size_t total;
} size_total;

enum table_sizes
{
    NUM_BUCKETS = 17,
    NUM_SMALL_BUCKETS = 7,
};

enum table_bytes
{
    INDEX_0_BYTES = 32,
    INDEX_1_BYTES = 40,
    INDEX_2_BYTES = 48,
    INDEX_3_BYTES = 56,
    INDEX_4_BYTES = 64,
    INDEX_5_BYTES = 72,
    INDEX_6_BYTES = 80,
    SMALL_TABLE_MAX_BYTES = INDEX_6_BYTES,
    /// This means our first log2 bucket index calculation yeilds 7 for the 0b1000_0000 bit.
    /// We then start doubling from here. 128, 256, 512, etc. Probably should profile to pick sizes.
    LARGE_TABLE_MIN_BYTES = 128,
    TABLE_BYTES = ( NUM_BUCKETS * sizeof( seg_node ) ),
};

/// Unsigned bitwise helpers we can't put into enums.
#define SIZE_MASK ~0x7UL
#define STATUS_CHECK 0x4UL
#define FREE_NODE_WIDTH (uint16_t)16
#define HEADER_AND_FREE_NODE (uint16_t)24
#define HEADERSIZE sizeof( size_t )
#define MIN_BLOCK_SIZE (uint16_t)32
#define FREED 0x0UL
#define ALLOCATED 0x1UL
#define LEFT_ALLOCATED 0x2UL
#define LEFT_FREE ~0x2UL

/////////////////////////////   Basic Block and Header Operations   ////////////////////////////

/// @brief roundup         rounds up a size to the nearest multiple of two to be aligned in the heap.
/// @param requested_size  size given to us by the client.
/// @param multiple        the nearest multiple to raise our number to.
/// @return                rounded number.
static inline size_t roundup( size_t requested_size, size_t multiple )
{
    return ( requested_size + multiple - 1 ) & ~( multiple - 1 );
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
static inline free_node *get_free_node( header *cur_header )
{
    return (free_node *)( (uint8_t *)cur_header + HEADERSIZE );
}

/// @brief *get_block_header  steps to the left from the user-available space to
///                           get the pointer to the header header.
/// @param *user_mem_space    the void pointer to the space available for the user.
/// @return                   the header immediately to the left associated with memory block.
static inline header *get_block_header( free_node *user_mem_space )
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

/////////////////////////////    Debugging and Testing Functions

/// @brief check_init   checks the internal representation of our heap, especially the head
///                     and tail nodes for any issues that would ruin our algorithms.
/// @param table[]      the lookup table that holds the list size ranges
/// @param *nil         a special free_node that serves as a sentinel for logic and edgecases.
/// @param client_size  the total space available for client.
/// @return             true if everything is in order otherwise false.
bool check_init( seg_node table[], free_node *nil, size_t client_size );

/// @brief is_memory_balanced  loops through all blocks of memory to verify that the sizes
///                            reported match the global bookeeping in our struct.
/// @param *total_free_mem     the output parameter of the total size used as another check.
/// @param hr                  start and end of the heap
/// @param st                  size of the heap memory and total free nodes.
/// @return                    true if our tallying is correct and our totals match.
bool is_memory_balanced( size_t *total_free_mem, heap_range hr, size_total st );

/// @brief are_fits_valid  loops through only the segregated fits list to make sure it matches
///                        the loop we just completed by checking all blocks.
/// @param total_free_mem  the input from a previous loop that was completed by jumping block
///                        by block over the entire heap.
/// @param table[]         the lookup table that holds the list size ranges
/// @param *nil            a special free_node that serves as a sentinel for logic and edgecases.
/// @return                true if the segregated fits list totals correctly false if not.
bool are_fits_valid( size_t total_free_mem, seg_node table[], free_node *nil );

/////////////////////////////        Printing Functions

/// @brief print_all    prints our the complete status of the heap, all of its blocks, and the
///                     sizes the blocks occupy. Printing should be clean with
///                     no overlap of unique id's between heap blocks or corrupted headers.
/// @param hr           start and end of the heap
/// @param client_size  the size in bytes of the heap.
/// @param table[]      the lookup table of segregated sizes of nodes stored in each slot.
/// @param *nil         the free node that serves as a universal head and tail to all lists.
void print_all( heap_range hr, size_t client_size, seg_node table[], free_node *nil );

/// @brief print_fits  prints the segregated fits free list in order to check if splicing and
///                    adding is progressing correctly.
/// @param table[]     the lookup table that holds the list size ranges
/// @param *nil        a special free_node that serves as a sentinel for logic and edgecases.
void print_fits( print_style style, seg_node table[], free_node *nil );

#endif
