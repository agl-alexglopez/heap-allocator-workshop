/**
 * File: list_addressorder_design.h
 * ---------------------------------
 * This file contains the interface that defines our custom types for the list_addressorder
 * allocator. It also contains useful methods for these types that are not key to understanding
 * the allocator algorithm, but helpful in its development.
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
#ifndef LIST_ADDRESSORDER_DESIGN_H
#define LIST_ADDRESSORDER_DESIGN_H
#include <stddef.h>
#include <stdbool.h>


/* * * * * * * * * * * * * *           Type Definitions            * * * * * * * * * * * * * * * */


#define SIZE_MASK ~0x7UL
#define STATUS_CHECK 0x4UL
#define FREE_NODE_WIDTH (unsigned short)16
#define HEADER_AND_FREE_NODE (unsigned short)24
#define MIN_BLOCK_SIZE (unsigned short)32
#define HEADERSIZE sizeof(size_t)

typedef size_t header;
typedef unsigned char byte;

typedef struct free_node {
    struct free_node *next;
    struct free_node *prev;
}free_node;

typedef enum header_status_t {
    FREE = 0x0UL,
    ALLOCATED = 0x1UL,
    LEFT_ALLOCATED = 0x2UL,
    LEFT_FREE = ~0x2UL
} header_status_t;


/* * * * * * * * * * * * * *    Basic Block and Header Operations  * * * * * * * * * * * * * * * */


/* @brief roundup         rounds up a size to the nearest multiple of two to be aligned in the heap.
 * @param requested_size  size given to us by the client.
 * @param multiple        the nearest multiple to raise our number to.
 * @return                rounded number.
 */
size_t roundup(size_t requested_size, size_t multiple);

/* @brief get_size    given a valid header find the total size of the header and block.
 * @param header_val  the pointer to the current header block we are on.
 * @return            the size in bytes of the header and memory block.
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
 * @param header_val          the valid header we will determine status for.
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

/* @brief init_header  initializes the header in the header_header field to reflect the
 *                     specified status and that the left neighbor is allocated or unavailable.
 * @param *cur_header  the header we will initialize.
 * @param block_size   the size, including the header, of the entire block.
 * @param status       FREE or ALLOCATED to reflect the status of the memory.
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


#endif
