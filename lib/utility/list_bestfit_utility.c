/**
 * File list_bestfit_utility.c
 * ---------------------------------
 * This file contains the implementation of utility functions for the list_bestfit heap
 * allocator. These functions serve as basic navigation for nodes and blocks, testing functions for
 * heap debugging, and printing functions for heap debugging. These functions can distract from the
 * algorithm implementations in the actual list_bestfit.c file so we seperate them out here.
 */
#include <limits.h>
#include "list_bestfit_utility.h"
#include "debug_break.h"


/* * * * * * * * * * * * * *    Basic Block and Header Operations  * * * * * * * * * * * * * * * */


/* @brief roundup         rounds up a size to the nearest multiple of two to be aligned in the heap.
 * @param requested_size  size given to us by the client.
 * @param multiple        the nearest multiple to raise our number to.
 * @return                rounded number.
 */
size_t roundup(size_t requested_size, size_t multiple) {
    return (requested_size + multiple - 1) & ~(multiple - 1);
}

/* @brief get_size  given a valid header find the total size of the header and block.
 * @param *cur_header_location    the pointer to the current header block we are on.
 * @return                    the size in bytes of the header and memory block.
 */
size_t get_size(header header_val) {
    return header_val & SIZE_MASK;
}

/* @brief *get_right_header  advances the header pointer to the next header in the heap.
 * @param *cur_header        the valid pointer to a heap header within the memory range.
 * @param block_size         the size of the current block.
 * @return                   a header pointer to the next block in the heap.
 */
header *get_right_header(header *cur_header, size_t block_size) {
    return (header *)((byte *)cur_header + block_size);
}

/* @brief *get_left_header  uses the left block size gained from the footer to move to the header.
 * @param *cur_header       the current header at which we reside.
 * @param left_block_size   the space of the left block as reported by its footer.
 * @return                  a header pointer to the header for the block to the left.
 */
header *get_left_header(header *cur_header) {
    header *left_footer = (header *)((byte *)cur_header - HEADERSIZE);
    return (header *)((byte *)cur_header - (*left_footer & SIZE_MASK));
}

/* @brief is_block_allocated  will determine if a block is marked as allocated.
 * @param header_val          the valid header we will determine status for.
 * @return                    true if the block is allocated, false if not.
 */
bool is_block_allocated(header header_val) {
    return header_val & ALLOCATED;
}

/* @brief *get_client_space  get the pointer to the start of the client available memory.
 * @param *cur_header        the valid header to the current block of heap memory.
 * @return                   a pointer to the first available byte of client heap memory.
 */
free_node *get_free_node(header *cur_header) {
    return (free_node *)((byte *)cur_header + HEADERSIZE);
}

/* @brief *get_block_header  steps to the left from the user-available space to get the pointer
 *                           to the header header.
 * @param *user_mem_space    the void pointer to the space available for the user.
 * @return                   the header immediately to the left associated with memory block.
 */
header *get_block_header(free_node *user_mem_space) {
    return (header *)((byte *)user_mem_space - HEADERSIZE);
}

/* @brief init_header  initializes the header in the header_header field to reflect the
 *                     specified status and that the left neighbor is allocated or unavailable.
 * @param *cur_header  the header we will initialize.
 * @param block_size   the size, including the header, of the entire block.
 * @param status       FREE or ALLOCATED to reflect the status of the memory.
 */
void init_header(header *cur_header, size_t block_size, header_status_t header_status) {
    *cur_header = LEFT_ALLOCATED | block_size | header_status;
}

/* @brief init_footer  initializes the footer to reflect that the associated block is now free. We
 *                     will only initialize footers on free blocks. We use the the control bits
 *                     in the right neighbor if the block is allocated and allow the user to have
 *                     the footer space.
 * @param *cur_header  a pointer to the header that is now free and will have a footer.
 * @param block_size   the size to use to update the footer of the block.
 */
void init_footer(header *cur_header, size_t block_size) {
    header *footer = (header*)((byte *)cur_header + block_size - HEADERSIZE);
    *footer = LEFT_ALLOCATED | block_size | FREE;
}

/* @brief is_left_space  checks the control bit in the second position to see if the left neighbor
 *                       is allocated or free to use for coalescing.
 * @param *cur_header    the current block for which we are checking the left neighbor.
 * @return               true if there is space to the left, false if not.
 */
bool is_left_space(header *cur_header) {
    return !(*cur_header & LEFT_ALLOCATED);
}

