/**
 * File: list_segregated_utility.h
 * ---------------------------------
 * This file contains the interface that defines our custom types for the list_segregated
 * allocator. It also contains useful methods for these types, testing functions, and printer
 * functions that make development of the allocator much easier. I seperate them out here so that
 * they do not crowd the file that contains the core logic of the heap.
 *
 * Across these heap utility libraries you may see code that appears almost identical to a utility
 * library for another allocator. While it may be tempting to think we could unite the common logic
 * of these methods to one utility library, I think this is a bad idea. There are subtle
 * differences between each allocator's types and block organization that makes keeping the logic
 * seperate easier and cleaner. This way, I can come back and adjust an existing allocator
 * exactly as needed. I can also add new allocators creatively, by simply adding a new utility
 * library to define its fundamentals, without worrying about fitting in to types and methods
 * previously established by other allocators.
 */
#ifndef LIST_SEGREGATED_UTILITY_H
#define LIST_SEGREGATED_UTILITY_H

#include <stddef.h>
#include <stdbool.h>
#include "print_utility.h"

#define SIZE_MASK ~0x7UL
#define STATUS_CHECK 0x4UL
#define FREE_NODE_WIDTH (unsigned short)16
#define HEADER_AND_FREE_NODE (unsigned short)24
#define HEADERSIZE sizeof(size_t)
#define MIN_BLOCK_SIZE (unsigned short)32
#define TABLE_SIZE (unsigned short)15
#define SMALL_TABLE_SIZE (unsigned short)4
#define SMALL_TABLE_MAX (unsigned short)56
#define LARGE_TABLE_MIN (unsigned short)64
#define TABLE_BYTES (15 * sizeof(seg_node))
#define INDEX_0 (unsigned short)0
#define INDEX_0_SIZE (unsigned short)32
#define INDEX_1 (unsigned short)1
#define INDEX_1_SIZE (unsigned short)40
#define INDEX_2 (unsigned short)2
#define INDEX_2_SIZE (unsigned short)48
#define INDEX_3 (unsigned short)3
#define INDEX_3_SIZE (unsigned short)56
#define INDEX_OFFSET 2U

typedef size_t header;
typedef unsigned char byte;

typedef enum header_status_t {
    FREE = 0x0UL,
    ALLOCATED = 0x1UL,
    LEFT_ALLOCATED = 0x2UL,
    LEFT_FREE = ~0x2UL
} header_status_t;

typedef struct free_node {
    struct free_node *next;
    struct free_node *prev;
}free_node;

typedef struct seg_node {
    unsigned short size;
    free_node *start;
}seg_node;


/* * * * * * * * * * * * * *    Basic Block and Header Operations  * * * * * * * * * * * * * * * */


/* @brief get_size     given a valid header find the total size of the header and block.
 * @param *header_val  the pointer to the current header block we are on.
 * @return             the size in bytes of the header and memory block.
 */
size_t get_size(header header_val);

/* @brief *get_right_header  advances the header pointer to the next header in the heap.
 * @param *cur_header        the valid pointer to a heap header within the memory range.
 * @param block_size         the size of the current block.
 * @return                   a header pointer to the next block in the heap.
 */
header *get_right_header(header *cur_header, size_t block_size);

/* @brief *get_left_header  uses the left block size gained from the footer to move to the header.
 * @param *cur_header       the current header at which we reside.
 * @param left_block_size   the space of the left block as reported by its footer.
 * @return                  a header pointer to the header for the block to the left.
 */
header *get_left_header(header *cur_header);

/* @brief is_block_allocated  will determine if a block is marked as allocated.
 * @param *header_val         the valid header we will determine status for.
 * @return                    true if the block is allocated, false if not.
 */
bool is_block_allocated(header header_val);

/* @brief *get_client_space  get the pointer to the start of the client available memory.
 * @param *cur_header        the valid header to the current block of heap memory.
 * @return                   a pointer to the first available byte of client heap memory.
 */
free_node *get_free_node(header *cur_header);

/* @brief *get_block_header  steps to the left from the user-available space to get the pointer
 *                           to the header header.
 * @param *user_mem_space    the void pointer to the space available for the user.
 * @return                   the header immediately to the left associated with memory block.
 */
header *get_block_header(free_node *user_mem_space);

/* @brief init_header    initializes the header in the header_header field to reflect the
 *                       specified status and that the left neighbor is allocated or unavailable.
 * @param *cur_header    the header we will initialize.
 * @param block_size     the size, including the header, of the entire block.
 * @param header_status  FREE or ALLOCATED to reflect the status of the memory.
 */
void init_header(header *cur_header, size_t block_size, header_status_t header_status);

/* @brief init_footer  initializes the footer to reflect that the associated block is now free. We
 *                     will only initialize footers on free blocks. We use the the control bits
 *                     in the right neighbor if the block is allocated and allow the user to have
 *                     the footer space.
 * @param *cur_header  a pointer to the header that is now free and will have a footer.
 * @param block_size   the size to use to update the footer of the block.
 */
void init_footer(header *cur_header, size_t block_size);

/* @brief is_left_space  checks the control bit in the second position to see if the left neighbor
 *                       is allocated or free to use for coalescing.
 * @param *cur_header    the current block for which we are checking the left neighbor.
 * @return               true if there is space to the left, false if not.
 */
bool is_left_space(header *cur_header);


/* * * * * * * * * * * * * *     Debugging and Testing Functions   * * * * * * * * * * * * * * * */


/* @brief is_header_corrupted   will determine if a block has the 3rd bit on, which is invalid.
 * @param *cur_header_location  the valid header we will determine status for.
 * @return                      true if the block has the second or third bit on.
 */
bool is_header_corrupted(header header_val);

/* @breif check_init   checks the internal representation of our heap, especially the head and tail
 *                     nodes for any issues that would ruin our algorithms.
 * @param table[]      the lookup table that holds the list size ranges
 * @param *nil         a special free_node that serves as a sentinel for logic and edgecases.
 * @param client_size  the total space available for client.
 * @return             true if everything is in order otherwise false.
 */
bool check_init(seg_node table[], free_node *nil, size_t client_size);

/* @brief is_valid_header  checks the header of a block of memory to make sure that is not an
 *                         unreasonable size or otherwise corrupted.
 * @param *cur_header      the header to a block of memory
 * @param block_size       the reported size of this block of memory from its header.
 * @client_size            the entire space available to the user.
 * @return                 true if the header is valid, false otherwise.
 */
bool is_valid_header(header cur_header, size_t block_size, size_t client_size);

/* @brief is_memory_balanced  loops through all blocks of memory to verify that the sizes
 *                            reported match the global bookeeping in our struct.
 * @param *total_free_mem     the output parameter of the total size used as another check.
 * @param *client_start       the start address of the client heap segment.
 * @param *client_end         the end address of the client heap segment.
 * @param client_size         the size in bytes of the total space available for client.
 * @param fits_total          the total number of free nodes in our table of lists.
 * @return                    true if our tallying is correct and our totals match.
 */
bool is_memory_balanced(size_t *total_free_mem, void *client_start, void *client_end,
                        size_t client_size, size_t fits_total);

/* @brief are_fits_valid  loops through only the segregated fits list to make sure it matches
 *                        the loop we just completed by checking all blocks.
 * @param total_free_mem  the input from a previous loop that was completed by jumping block
 *                        by block over the entire heap.
 * @param table[]         the lookup table that holds the list size ranges
 * @param *nil            a special free_node that serves as a sentinel for logic and edgecases.
 * @return                true if the segregated fits list totals correctly, false if not.
 */
bool are_fits_valid(size_t total_free_mem, seg_node table[], free_node *nil);


/* * * * * * * * * * * * * *         Printing Functions            * * * * * * * * * * * * * * * */


/* @brief print_fits  prints the segregated fits free list in order to check if splicing and
 *                    adding is progressing correctly.
 * @param table[]     the lookup table that holds the list size ranges
 * @param *nil        a special free_node that serves as a sentinel for logic and edgecases.
 */
void print_fits(print_style style, seg_node table[], free_node *nil);

/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *cur_header        a valid header to a block of allocated memory.
 */
void print_alloc_block(header *cur_header);

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *cur_header       a valid header to a block of allocated memory.
 */
void print_free_block(header *cur_header);

/* @brief print_error_block  prints a helpful error message if a block is corrupted.
 * @param *cur_header        a header to a block of memory.
 * @param full_size          the full size of a block of memory, not just the user block size.
 */
void print_error_block(header *cur_header, size_t full_size);

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 * @param table[]         the lookup table that holds the list size ranges
 * @param *nil            a special free_node that serves as a sentinel for logic and edgecases.
 */
void print_bad_jump(header *current, header *prev, seg_node table[], free_node *nil);


#endif
