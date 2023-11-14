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

///////////////////////////// Type Definitions  ///////////////////////////////////////

/// Size Order Classes Maintained by an Array of segregated fits lists
///     - Our size classes stand for the minimum size of a node in the list less
///     than the next.
///     - 15 Size Classes (in bytes):
///
///          32,         40,          48,           56,           64-127,
///          128-255,    256-511,     512-1023,     1024-2047,    2048-4095,
///          4096-8191,  8192-16383,  16384-32767,  32768-65535,  65536+,
///
///     - A first fit search will yeild approximately the best fit.
///     - We will have one dummy node to serve as both the head and tail of all
///     lists.
///     - Be careful, last index is USHRT_MAX=65535!=65536. Mind the last index
///     size.
///

typedef size_t header;
typedef unsigned char byte;

typedef struct free_node
{
    struct free_node *next;
    struct free_node *prev;
} free_node;

typedef struct seg_node
{
    unsigned short size;
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

#define INDEX_0 0UL
#define INDEX_0_SIZE 32UL
#define INDEX_1 1UL
#define INDEX_1_SIZE 40UL
#define INDEX_2 2UL
#define INDEX_2_SIZE 48UL
#define INDEX_3 3UL
#define INDEX_3_SIZE 56UL
#define INDEX_OFFSET 2UL
#define SIZE_MASK ~0x7UL
#define STATUS_CHECK 0x4UL
#define FREE_NODE_WIDTH (unsigned short)16
#define HEADER_AND_FREE_NODE (unsigned short)24
#define HEADERSIZE sizeof( size_t )
#define MIN_BLOCK_SIZE (unsigned short)32
#define TABLE_SIZE (unsigned short)15
#define SMALL_TABLE_SIZE (unsigned short)4
#define SMALL_TABLE_MAX (unsigned short)56
#define LARGE_TABLE_MIN (unsigned short)64
#define TABLE_BYTES ( TABLE_SIZE * sizeof( seg_node ) )
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
    return (header *)( (byte *)cur_header + block_size );
}

/// @brief *get_left_header  uses the left block size gained from the footer to move to the header.
/// @param *cur_header       the current header at which we reside.
/// @param left_block_size   the space of the left block as reported by its footer.
/// @return                  a header pointer to the header for the block to the left.
static inline header *get_left_header( header *cur_header )
{
    header *left_footer = (header *)( (byte *)cur_header - HEADERSIZE );
    return (header *)( (byte *)cur_header - ( *left_footer & SIZE_MASK ) );
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
    return (free_node *)( (byte *)cur_header + HEADERSIZE );
}

/// @brief *get_block_header  steps to the left from the user-available space to
///                           get the pointer to the header header.
/// @param *user_mem_space    the void pointer to the space available for the user.
/// @return                   the header immediately to the left associated with memory block.
static inline header *get_block_header( free_node *user_mem_space )
{
    return (header *)( (byte *)user_mem_space - HEADERSIZE );
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
    header *footer = (header *)( (byte *)cur_header + block_size - HEADERSIZE );
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
