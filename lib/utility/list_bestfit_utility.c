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


/* * * * * * * * * * * * * *     Debugging and Testing Functions   * * * * * * * * * * * * * * * */


/* @brief is_header_corrupted  will determine if a block has the 3rd bit on, which is invalid.
 * @param header_val           the valid header we will determine status for.
 * @return                     true if the block has the second or third bit on.
 */
bool is_header_corrupted(header header_val) {
    return header_val & STATUS_CHECK;
}

/* @breif check_init     checks the internal representation of our heap, especially the head and
 *                       tail nodes for any issues that would ruin our algorithms.
 * @param *client_start  the start address of the client heap segment.
 * @param *client_end    the end address of the client heap segment.
 * @param client_size    the size in bytes of the total space available for client.
 * @param *head          the free_node head of the linked list.
 * @param *tail          the free_node tail of the linked list.
 * @return               true if everything is in order otherwise false.
 */
bool check_init(void *client_start, void *client_end, size_t client_size,
                free_node *head, free_node *tail) {
    if (is_left_space(client_start)) {
        breakpoint();
        return false;
    }
    void *first_address = head;
    void *last_address = (byte *)tail + FREE_NODE_WIDTH;
    if ((size_t)((byte *)last_address - (byte *)first_address) != client_size) {
        breakpoint();
        return false;
    }
    /* There is one very rare edgecase that may affect the next field of the list tail. This
     * is acceptable because we never use that field and do not need it to remain NULL.
     */
    if (head->prev != NULL) {
        return false;
    }
    return true;
}

/* @brief is_valid_header  checks the header of a block of memory to make sure that is not an
 *                         unreasonable size or otherwise corrupted.
 * @param cur_header       the header to a block of memory
 * @param block_size       the reported size of this block of memory from its header.
 * @param client_size      the size of the space available for the user.
 * @return                 true if the header is valid, false otherwise.
 */
bool is_valid_header(header header_val, size_t block_size, size_t client_size) {
    // Most definitely impossible and our header is corrupted. Pointer arithmetic would fail.
    if (block_size > client_size) {
        return false;
    }
    // Some bits are overlapping into our lower three control bits in the headers.
    if (is_header_corrupted(header_val)) {
        return false;
    }
    if (block_size % HEADERSIZE != 0) {
        return false;
    }
    return true;
}

/* @brief get_size_used    loops through all blocks of memory to verify that the sizes
 *                         reported match the global bookeeping in our struct.
 * @param *total_free_mem  the output parameter of the total size used as another check.
 * @param *client_start    the start address of the client heap segment.
 * @param *client_end      the end address of the client heap segment.
 * @param client_size      the size in bytes of the total space available for client.
 * @param free_list_total  the total number of free nodes in our list.
 * @return                 true if our tallying is correct and our totals match.
 */
bool is_memory_balanced(size_t *total_free_mem, void *client_start, void *client_end,
                        size_t client_size, size_t free_list_total) {
    header *cur_header = client_start;
    size_t size_used = FREE_NODE_WIDTH * 2;
    size_t total_free_nodes = 0;
    while (cur_header != client_end) {
        size_t block_size_check = get_size(*cur_header);
        if (block_size_check == 0) {
            breakpoint();
            return false;
        }
        if (!is_valid_header(*cur_header, block_size_check, client_size)) {
            breakpoint();
            return false;
        }
        if (is_block_allocated(*cur_header)) {
            size_used += block_size_check;
        } else {
            total_free_nodes++;
            *total_free_mem += block_size_check;
        }
        cur_header = get_right_header(cur_header, block_size_check);
    }
    if (size_used + *total_free_mem != client_size) {
        breakpoint();
        return false;
    }
    if (total_free_nodes != free_list_total) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief is_free_list_valid  loops through only the doubly linked list to make sure it matches
 *                            the loop we just completed by checking all blocks.
 * @param total_free_mem      the input from a previous loop that was completed by jumping block
 *                            by block over the entire heap.
 * @param *head               the free_node head of the linked list.
 * @param *tail               the free_node tail of the linked list.
 * @return                    true if the doubly linked list totals correctly, false if not.
 */
bool is_free_list_valid(size_t total_free_mem, free_node *head, free_node *tail) {
    size_t linked_free_mem = 0;
    size_t prev_size = 0;
    for (free_node *cur = head->next; cur != tail; cur = cur->next) {
        header *cur_header = get_block_header(cur);
        size_t cur_size = get_size(*cur_header);
        if (prev_size >  cur_size) {
            return false;
        }
        if (is_block_allocated(*cur_header)) {
            return false;
        }
        // This algorithm does not allow two free blocks to remain next to one another.
        if (is_left_space(get_block_header(cur))) {
            return false;
        }
        linked_free_mem += cur_size;
        prev_size = cur_size;
    }
    return total_free_mem == linked_free_mem;
}


/* * * * * * * * * * * * * *         Printing Functions            * * * * * * * * * * * * * * * */


/* @brief print_linked_free  prints the doubly linked free list in order to check if splicing and
 *                           adding is progressing correctly.
 * @param *head              the free_node head of the linked list.
 * @param *tail              the free_node tail of the linked list.
 */
void print_linked_free(print_style style, free_node *head, free_node *tail) {
    printf(COLOR_RED);
    printf("[");
    if (style == VERBOSE) {
        printf("%p:", head);
    }
    printf("(HEAD)]");
    for (free_node *cur = head->next; cur != tail; cur = cur->next) {
        if (cur) {
            header *cur_header = get_block_header(cur);
            printf("<=>[");
            if (style == VERBOSE) {
                printf("%p:", cur);
            }
            printf("(%zubytes)]", get_size(*cur_header) - HEADERSIZE);
        } else {
            printf("Something went wrong. NULL free free_list node.\n");
            break;
        }
    }
    printf("<=>[");
    if (style == VERBOSE) {
        printf("%p:", tail);
    }
    printf("(TAIL)]\n");
    printf(COLOR_NIL);
}


/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *cur_header        a valid header to a block of allocated memory.
 */
void print_alloc_block(header *cur_header) {
    size_t block_size = get_size(*cur_header) - HEADERSIZE;
    printf(COLOR_GRN);
    // We will see from what direction our header is messed up by printing 16 digits.
    printf("%p: HEADER->0x%016zX->[ALOC-%zubytes]\n", cur_header, *cur_header, block_size);
    printf(COLOR_NIL);
}

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *cur_header       a valid header to a block of allocated memory.
 */
void print_free_block(header *cur_header) {
    size_t full_size = get_size(*cur_header);
    size_t block_size = full_size - HEADERSIZE;
    header *footer = (header *)((byte *)cur_header + full_size - HEADERSIZE);
    /* We should be able to see the header is the same as the footer. If they are not the same
     * we will face subtle bugs that are very hard to notice.
     */
    if (*footer != *cur_header) {
        *footer = ULONG_MAX;
    }
    printf(COLOR_RED);
    printf("%p: HEADER->0x%016zX->[FREE-%zubytes->FOOTER->%016zX]\n",
            cur_header, *cur_header, block_size, *footer);
    printf(COLOR_NIL);
}

/* @brief print_error_block  prints a helpful error message if a block is corrupted.
 * @param *cur_header        a header to a block of memory.
 * @param full_size          the full size of a block of memory, not just the user block size.
 */
void print_error_block(header *cur_header, size_t full_size) {
    size_t block_size = full_size - HEADERSIZE;
    printf(COLOR_CYN);
    printf("\n%p: HEADER->0x%016zX->%zubytes\n",
           cur_header, *cur_header, block_size);
    printf("Block size is too large and header is corrupted.\n");
    printf(COLOR_NIL);
}

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 * @param *head           the free_node head of the linked list.
 * @param *tail           the free_node tail of the linked list.
 */
void print_bad_jump(header *current, header *prev, free_node *head, free_node *tail) {
    size_t prev_size = get_size(*prev);
    size_t cur_size = get_size(*current);
    printf(COLOR_CYN);
    printf("A bad jump from the value of a header has occured. Bad distance to next header.\n");
    printf("The previous address: %p:\n", prev);
    printf("\tHeader Hex Value: %016zX:\n", *prev);
    printf("\tBlock Byte Value: %zubytes:\n", prev_size);
    printf("\nJump by %zubytes...\n", prev_size);
    printf("The current address: %p:\n", current);
    printf("\tHeader Hex Value: %016zX:\n", *current);
    printf("\tBlock Byte Value: %zubytes:\n", cur_size);
    printf("\nJump by %zubytes...\n", cur_size);
    printf("Current state of the free list:\n");
    printf(COLOR_NIL);
    // The doubly linked free_list may be messed up as well.
    print_linked_free(VERBOSE, head, tail);
}

