/**
 * File: list_utility.h
 * -----------------
 * This file contains the struct types that make up the nodes behind our list and tree based
 * allocators. We also make all of our printing and debugging functions here. This arose from the
 * fact that the red black tree allocators had debugging and printing code upwards of 500 lines
 * following the heap allocator implementations. This excess was repeated across all allocators.
 * It was time to move as much as possible to an external library to cut down on needless
 * repition and make each file more manageable to read and understand.
 */

#ifndef _LIST_UTILITY_H_
#define _LIST_UTILITY_H_
#include <stdbool.h>
#include <stddef.h>

/* Text coloring macros (ANSI character escapes) for printing function colorful output. Consider
 * changing to a more portable library like ncurses.h. However, I don't want others to install
 * ncurses just to explore the project. They already must install gnuplot. Hope this works.
 */
#define COLOR_BLK "\033[34;1m"
#define COLOR_RED "\033[31;1m"
#define COLOR_CYN "\033[36;1m"
#define COLOR_GRN "\033[32;1m"
#define COLOR_NIL "\033[0m"
#define COLOR_ERR COLOR_RED "Error: " COLOR_NIL
#define PRINTER_INDENT (short)13
#define LIST_NODE_WIDTH (unsigned short)16
#define HEADERSIZE sizeof(size_t)
#define SIZE_MASK ~0x7UL
#define COLOR_MASK 0x4UL
#define MIN_LIST_BLOCK_SIZE (unsigned short)32
#define LIST_NODE_WIDTH (unsigned short)16
#define HEADER_AND_LIST_NODE (unsigned short)24
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

/* * * * * * * * * * * * * *  Type Definitions   * * * * * * * * * * * * * * * * */


typedef size_t header;
typedef unsigned char byte;

// PLAIN prints free block sizes, VERBOSE shows address in the heap and black height of tree.
typedef enum print_style {
    PLAIN = 0,
    VERBOSE = 1
}print_style;

/* All allocators use bits in the header to track information. The RED_PAINT BLK_PAINT status masks
 * are only used by the red black tree allocators, but other masks are identical across allocators.
 */
typedef enum header_status {
    FREE = 0x0UL,
    ALLOCATED = 0x1UL,
    LEFT_ALLOCATED = 0x2UL,
    RED_PAINT = 0x4UL,
    BLK_PAINT = ~0x4UL,
    LEFT_FREE = ~0x2UL
} header_status;

typedef enum rb_color {
    BLACK = 0,
    RED = 1
}rb_color;

// NOT(!) operator will flip this enum to the opposite field. !L == R and !R == L;
typedef enum tree_link {
    // (L == LEFT), (R == RIGHT)
    L = 0,
    R = 1
} tree_link;

// When you see these, know that we are working with a doubly linked list, not a tree.
typedef enum list_link {
    // (P == PREVIOUS), (N == NEXT)
    P = 0,
    N = 1
} list_link;

/* Doubly Linked List Node:
 *  - Use in a list organized by sizes.
 *  - Use in a list organized by address in memory.
 *  - Use in a segregated list as the node pointed to by the lookup table.
 */
typedef struct list_node {
    struct list_node *next;
    struct list_node *prev;
}list_node;

/* Size Order Classes Maintained by an Array of segregated fits lists
 *     - Our size classes stand for the minimum size of a node in the list less than the next.
 *     - 15 Size Classes (in bytes):
            32,        40,         48,          56,          64-127,
            128-255,   256-511,    512-1023,    1024-2047,   2048-4095,
            4096-8191, 8192-16383, 16384-32767, 32768-65535, 65536+,
 *     - A first fit search will yeild approximately the best fit.
 *     - We will have one dummy node to serve as both the head and tail of all lists.
 *     - Be careful, last index is USHRT_MAX=65535!=65536. Mind the last index size.
 */
typedef struct seg_node {
    unsigned short size;
    list_node *start;
}seg_node;




/* * * * * * * * * * * * * *  Minor Helper Functions   * * * * * * * * * * * * * * * * */

/* @brief get_size  given   a valid header find the total size of the header and block.
 * @param header_val        the current header block we are on.
 * @return                  the size in bytes of the header and memory block.
 */
size_t get_size(header header_val);

/* @brief is_block_allocated  will determine if a block is marked as allocated.
 * @param *header_location    the valid header we will determine status for.
 * @return                    true if the block is allocated, false if not.
 */
bool is_block_allocated(header header_val);

/* @brief is_left_space  checks the control bit in the second position to see if the left neighbor
 *                       is allocated or free to use for coalescing.
 * @param header         the current block header for which we are checking the left neighbor.
 * @return               true if there is space to the left, false if not.
 */
bool is_left_space(header header_val);

/* @brief *get_right_list_node_header  advances the header_t pointer to the next header in the heap.
 * @param *header                      the valid pointer to a heap header within the memory range.
 * @param block_size                   the size of the current block.
 * @return                             a header_t pointer to the next block in the heap.
 */
header *get_right_header(header *cur_header, size_t block_size);

/* @brief *get_left_header  uses the left block size gained from the footer to move to the header.
 * @param *header           the current header at which we reside.
 * @param left_block_size   the space of the left block as reported by its footer.
 * @return                  a header_t pointer to the header for the block to the left.
 */
header *get_left_header(header *cur_header);

/* @brief *get_block_header  steps to the left from the user-available space to get the pointer
 *                               to the header_t header.
 * @param *user_mem_space        the void pointer to the space available for the user.
 * @return                       the header immediately to the left associated with memory block.
 */
header *get_block_header(list_node *user_mem_space);

/* @brief *get_client_space  get the pointer to the start of the client available memory.
 * @param *header            the valid header to the current block of heap memory.
 * @return                   a pointer to the first available byte of client heap memory.
 */
list_node *get_list_node(header *header);

/* @brief init_header  initializes the header in the header_t_header field to reflect the
 *                     specified status and that the left neighbor is allocated or unavailable.
 * @param *header      the header we will initialize.
 * @param block_size   the size, including the header, of the entire block.
 * @param status       FREE or ALLOCATED to reflect the status of the memory.
 */
void init_header(header *cur_header, size_t block_size, header_status cur_status);

/* @brief init_footer  initializes the footer to reflect that the associated block is now free. We
 *                     will only initialize footers on free blocks. We use the the control bits
 *                     in the right neighbor if the block is allocated and allow the user to have
 *                     the footer space.
 * @param *header      a pointer to the header_t that is now free and will have a footer.
 * @param block_size   the size to use to update the footer of the block.
 */
void init_footer(header *cur_header, size_t block_size);


/* * * * * * * * * * * * * *  Debugging Functions   * * * * * * * * * * * * * * * * */


/* @breif check_list_init  checks the internal representation of our heap, especially the head and tail
 *                         nodes for any issues that would ruin our algorithms.
 * @return                 true if everything is in order otherwise false.
 */
bool check_list_init(void *client_start, void *client_end, size_t client_size);

/* @breif check_seg_list_init  checks the internals of our heap, especially the lookup table
 *                             nodes for any issues that would ruin our algorithms.
 * @return                     true if everything is in order otherwise false.
 */
bool check_seg_list_init(seg_node table[], list_node *nil, size_t client_size);

/* @brief is_list_balanced  loops through all blocks of memory to verify that the sizes
 *                          reported match the global bookeeping in our struct.
 * @param *total_free_mem   the output parameter of the total size used as another check.
 * @return                  true if our tallying is correct and our totals match.
 */
bool is_list_balanced(size_t *total_free_mem, void *client_start, void *client_end,
                      size_t client_size, size_t free_list_total);

/* @brief is_list_addressorder_valid  loops through only the doubly linked list to make sure it
 *                                    matches the loop we just completed by checking all blocks.
 * @param total_free_mem              the input from a previous loop that was completed by jumping
 *                                    block by block over the entire heap.
 * @return                            true if the doubly linked list totals correctly, false if not.
 */
bool is_list_addressorder_valid(size_t total_free_mem,
                                list_node *free_list_head, list_node *free_list_tail);

/* @brief is_list_bestfit_valid  loops through only the doubly linked list to make sure it matches
 *                               the loop we just completed by checking all blocks.
 * @param total_free_mem         the input from a previous loop that was completed by jumping block
 *                               by block over the entire heap.
 * @return                       true if the doubly linked list totals correctly, false if not.
 */
bool is_list_bestfit_valid(size_t total_free_mem,
                           list_node *free_list_head, list_node *free_list_tail);

/* @brief are_seg_list_valid  loops through only the segregated fits list to make sure it matches
 *                            the loop we just completed by checking all blocks.
 * @param total_free_mem      the input from a previous loop that was completed by jumping block
 *                            by block over the entire heap.
 * @return                    true if the segregated fits list totals correctly, false if not.
 */
bool is_seg_list_valid(size_t total_free_mem, seg_node table[], list_node *nil);


/* * * * * * * * * * * * * *  Debugging Functions   * * * * * * * * * * * * * * * * */


/* @brief print_list_free  prints the doubly linked free list in order to check if splicing and
 *                           adding is progressing correctly.
 */
void print_list_free(print_style style,
                     list_node *free_list_head, list_node *free_list_tail);

/* @brief print_seg_list  prints the segregated fits free list in order to check if splicing and
 *                        adding is progressing correctly.
 */
void print_seg_list(print_style style, seg_node table[], list_node *nil);

/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *header            a valid header to a block of allocated memory.
 */
void print_alloc_block(header *cur_header);

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *header            a valid header to a block of allocated memory.
 */
void print_free_block(header *cur_header);

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 */
void print_bad_jump(header *current, header *prev,
                    list_node *free_list_head, list_node *free_list_tail);

#endif
