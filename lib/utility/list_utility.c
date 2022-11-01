
#include <limits.h>
#include "list_utility.h"
#include "debug_break.h"


/* * * * * * * * * * * * * *  Minor Helper Functions   * * * * * * * * * * * * * * * * */


/* @brief get_size  given   a valid header find the total size of the header and block.
 * @param header_val        the current header block we are on.
 * @return                  the size in bytes of the header and memory block.
 */
size_t get_size(header header_val) {
    return SIZE_MASK & header_val;
}

/* @brief is_block_allocated  will determine if a block is marked as allocated.
 * @param *header_location    the valid header we will determine status for.
 * @return                    true if the block is allocated, false if not.
 */
bool is_block_allocated(header header_val) {
    return header_val & ALLOCATED;
}

/* @brief is_left_space  checks the control bit in the second position to see if the left neighbor
 *                       is allocated or free to use for coalescing.
 * @param header         the current block header for which we are checking the left neighbor.
 * @return               true if there is space to the left, false if not.
 */
bool is_left_space(header header_val) {
    return !(header_val & LEFT_ALLOCATED);
}

/* @brief *get_right_list_node_header  advances the header_t pointer to the next header in the heap.
 * @param *header                      the valid pointer to a heap header within the memory range.
 * @param block_size                   the size of the current block.
 * @return                             a header_t pointer to the next block in the heap.
 */
header *get_right_header(header *cur_header, size_t block_size) {
    return (header *)((byte *)cur_header + block_size);
}

/* @brief *get_left_header  uses the left block size gained from the footer to move to the header.
 * @param *header           the current header at which we reside.
 * @param left_block_size   the space of the left block as reported by its footer.
 * @return                  a header_t pointer to the header for the block to the left.
 */
header *get_left_header(header *cur_header) {
    header *left_footer = (header *)((byte *)cur_header - HEADERSIZE);
    return (header *)((byte *)cur_header - (*left_footer & SIZE_MASK));
}

/* @brief *get_block_header  steps to the left from the user-available space to get the pointer
 *                               to the header_t header.
 * @param *user_mem_space        the void pointer to the space available for the user.
 * @return                       the header immediately to the left associated with memory block.
 */
header *get_block_header(list_node *user_mem_space) {
    return (header *)((byte *)user_mem_space - HEADERSIZE);
}

/* @brief *get_client_space  get the pointer to the start of the client available memory.
 * @param *header            the valid header to the current block of heap memory.
 * @return                   a pointer to the first available byte of client heap memory.
 */
list_node *get_list_node(header *cur_header) {
    return (list_node *)((byte *)cur_header + HEADERSIZE);
}

/* @brief init_header  initializes the header in the header_t_header field to reflect the
 *                     specified status and that the left neighbor is allocated or unavailable.
 * @param *header      the header we will initialize.
 * @param block_size   the size, including the header, of the entire block.
 * @param status       FREE or ALLOCATED to reflect the status of the memory.
 */
void init_header(header *cur_header, size_t block_size, header_status cur_status) {
    *cur_header = LEFT_ALLOCATED | block_size | cur_status;
}

/* @brief init_footer  initializes the footer to reflect that the associated block is now free. We
 *                     will only initialize footers on free blocks. We use the the control bits
 *                     in the right neighbor if the block is allocated and allow the user to have
 *                     the footer space.
 * @param *header      a pointer to the header_t that is now free and will have a footer.
 * @param block_size   the size to use to update the footer of the block.
 */
void init_footer(header *cur_header, size_t block_size) {
    header *footer = (header*)((byte *)cur_header + block_size - HEADERSIZE);
    *footer = LEFT_ALLOCATED | block_size | FREE;
}


/* * * * * * * * * * * * * *  Debugging Functions   * * * * * * * * * * * * * * * * */


/* @breif check_list_init  checks the internal representation of our heap, especially the head and tail
 *                         nodes for any issues that would ruin our algorithms.
 * @return                 true if everything is in order otherwise false.
 */
bool check_list_init(void *client_start, void *client_end, size_t client_size) {
    if ((size_t)((byte *)client_end - (byte *)client_start) != client_size) {
        breakpoint();
        return false;
    }
    return true;
}

/* @breif check_seg_list_init  checks the internals of our heap, especially the lookup table
 *                             nodes for any issues that would ruin our algorithms.
 * @return                     true if everything is in order otherwise false.
 */
bool check_seg_list_init(seg_node table[], list_node *nil, size_t client_size) {
    void *first_address = table;
    void *last_address = (byte *)nil + LIST_NODE_WIDTH;
    if ((size_t)((byte *)last_address - (byte *)first_address) != client_size) {
        breakpoint();
        return false;
    }
    // Check our lookup table. Sizes should never be altered and pointers should never be NULL.
    unsigned short size = MIN_LIST_BLOCK_SIZE;
    for (int i = 0; i < SMALL_TABLE_SIZE; i++, size += HEADERSIZE) {
        if (table[i].size != size) {
            breakpoint();
            return false;
        }
        // This should either be a valid node or the sentinel.
        if (NULL == table[i].start) {
            breakpoint();
            return false;
        }
    }
    size = LARGE_TABLE_MIN;
    for (int i = SMALL_TABLE_SIZE; i < TABLE_SIZE - 1; i++, size *= 2) {
        if (table[i].size != size) {
            breakpoint();
            return false;
        }
        // This should either be a valid node or the nil.
        if (NULL == table[i].start) {
            breakpoint();
            return false;
        }
    }
    if (table[TABLE_SIZE - 1].size != USHRT_MAX) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief is_list_balanced  loops through all blocks of memory to verify that the sizes
 *                          reported match the global bookeeping in our struct.
 * @param *total_free_mem   the output parameter of the total size used as another check.
 * @return                  true if our tallying is correct and our totals match.
 */
bool is_list_balanced(size_t *total_free_mem, void *client_start, void *client_end,
                      size_t client_size, size_t free_list_total) {
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    header *cur_header = client_start;
    size_t size_used = LIST_NODE_WIDTH * 2;
    size_t total_free_nodes = 0;
    while (cur_header != client_end) {
        size_t block_size_check = get_size(*cur_header);
        if (block_size_check == 0) {
            // Bad jump check the previous node address compared to this one.
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

/* @brief is_list_addressorder_valid  loops through only the doubly linked list to make sure it
 *                                    matches the loop we just completed by checking all blocks.
 * @param total_free_mem              the input from a previous loop that was completed by jumping
 *                                    block by block over the entire heap.
 * @return                            true if the doubly linked list totals correctly, false if not.
 */
bool is_list_addressorder_valid(size_t total_free_mem,
                                list_node *free_list_head, list_node *free_list_tail) {
    size_t linked_free_mem = 0;
    list_node *prev = free_list_head;
    for (list_node *cur = free_list_head->next; cur != free_list_tail; cur = cur->next) {
        header *cur_header = get_block_header(cur);
        if (cur < prev) {
            return false;
        }
        if (is_block_allocated(*cur_header)) {
            return false;
        }
        // This algorithm does not allow two free blocks to remain next to one another.
        if (is_left_space(*get_block_header(cur))) {
            return false;
        }
        linked_free_mem += get_size(*cur_header);
        prev = cur;
    }
    return total_free_mem == linked_free_mem;
}

/* @brief is_list_bestfit_valid  loops through only the doubly linked list to make sure it matches
 *                               the loop we just completed by checking all blocks.
 * @param total_free_mem         the input from a previous loop that was completed by jumping block
 *                               by block over the entire heap.
 * @return                       true if the doubly linked list totals correctly, false if not.
 */
bool is_list_bestfit_valid(size_t total_free_mem,
                                  list_node *free_list_head, list_node *free_list_tail) {
    size_t linked_free_mem = 0;
    size_t prev_size = 0;
    for (list_node *cur = free_list_head->next; cur != free_list_tail; cur = cur->next) {
        header *cur_header = get_block_header(cur);
        size_t cur_size = get_size(*cur_header);
        if (prev_size >  cur_size) {
            return false;
        }
        if (is_block_allocated(*cur_header)) {
            breakpoint();
            return false;
        }
        // This algorithm does not allow two free blocks to remain next to one another.
        if (is_left_space(*get_block_header(cur))) {
            breakpoint();
            return false;
        }
        linked_free_mem += cur_size;
        prev_size = cur_size;
    }
    if (total_free_mem != linked_free_mem) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief are_seg_list_valid  loops through only the segregated fits list to make sure it matches
 *                            the loop we just completed by checking all blocks.
 * @param total_free_mem      the input from a previous loop that was completed by jumping block
 *                            by block over the entire heap.
 * @return                    true if the segregated fits list totals correctly, false if not.
 */
bool is_seg_list_valid(size_t total_free_mem, seg_node table[], list_node *nil) {
    size_t linked_free_mem = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        for (list_node *cur = table[i].start; cur != nil; cur = cur->next) {
            header *cur_header = get_block_header(cur);
            size_t cur_size = get_size(*cur_header);
            if (i != TABLE_SIZE - 1 && cur_size >= table[i + 1].size) {
                breakpoint();
                return false;
            }
            if (is_block_allocated(*cur_header)) {
                breakpoint();
                return false;
            }
            // This algorithm does not allow two free blocks to remain next to one another.
            if (is_left_space(*get_block_header(cur))) {
                breakpoint();
                return false;
            }
            linked_free_mem += cur_size;
        }
    }
    if (total_free_mem != linked_free_mem) {
        breakpoint();
        return false;
    }
    return true;
}


/* * * * * * * * * * * * * *  Printing Functions   * * * * * * * * * * * * * * * * */


/* @brief print_list_free  prints the doubly linked free list in order to check if splicing and
 *                           adding is progressing correctly.
 */
void print_list_free(print_style style,
                     list_node *free_list_head, list_node *free_list_tail) {
    printf(COLOR_RED);
    printf("[");
    if (style == VERBOSE) {
        printf("%p:", free_list_head);
    }
    printf("(HEAD)]");
    for (list_node *cur = free_list_head->next; cur != free_list_tail; cur = cur->next) {
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
        printf("%p:", free_list_tail);
    }
    printf("(TAIL)]\n");
    printf(COLOR_NIL);
}

/* @brief print_seg_list  prints the segregated fits free list in order to check if splicing and
 *                        adding is progressing correctly.
 */
void print_seg_list(print_style style, seg_node table[], list_node *nil) {
    bool alternate = false;
    for (int i = 0; i < TABLE_SIZE; i++) {
        printf(COLOR_GRN);
        if (i == TABLE_SIZE - 1) {
            printf("[CLASS:%ubytes+]=>", table[i].size);
        } else if (i >= SMALL_TABLE_SIZE) {
            printf("[CLASS:%u-%ubytes]=>", table[i].size, table[i + 1].size - 1U);
        } else {
            printf("[CLASS:%ubytes]=>", table[i].size);
        }
        printf(COLOR_NIL);
        if (alternate) {
            printf(COLOR_RED);
        } else {
            printf(COLOR_CYN);
        }
        alternate = !alternate;
        for (list_node *cur = table[i].start; cur != nil; cur = cur->next) {
            if (cur) {
                header *cur_header = get_block_header(cur);
                printf("<=>[");
                if (style == VERBOSE) {
                    printf("%p:", get_block_header(cur));
                }
                printf("(%zubytes)]", get_size(*cur_header));
            } else {
                printf("Something went wrong. NULL free fits node.\n");
                break;
            }
        }
        printf("<=>[%p]\n", nil);
        printf(COLOR_NIL);
    }
}

/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *header            a valid header to a block of allocated memory.
 */
void print_alloc_block(header *cur_header) {
    size_t block_size = get_size(*cur_header) - HEADERSIZE;
    // We will see from what direction our header is messed up by printing 16 digits.
    printf("%p: HEADER->0x%016zX->[ALOC-%zubytes]\n", cur_header, *cur_header, block_size);
}

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *header            a valid header to a block of allocated memory.
 */
void print_free_block(header *cur_header) {
    size_t full_size = get_size(*cur_header);
    size_t block_size = full_size - HEADERSIZE;
    header *footer = (header *)((void *)((byte *)cur_header + full_size - HEADERSIZE));
    /* We should be able to see the header is the same as the footer. If they are not the same
     * we will face subtle bugs that are very hard to notice.
     */
    if (*footer != *cur_header) {
        *footer = ULONG_MAX;
    }
    printf("%p: HEADER->0x%016zX->[FREE-%zubytes->FOOTER->%016zX]\n",
            cur_header, *cur_header, block_size, *footer);
}

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 */
void print_bad_jump(header *current, header *prev,
                           list_node *free_list_head, list_node *free_list_tail) {
    size_t prev_size = get_size(*prev);
    size_t cur_size = get_size(*current);
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
    // The doubly linked list may be messed up as well.
    print_list_free(VERBOSE, free_list_head, free_list_tail);
}
