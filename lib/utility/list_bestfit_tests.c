/**
 * File: list_bestfit_tests.c
 * ---------------------------
 * This file contains tests relevant the the list_bestfit allocator. Add more tests here as needed.
 * These are particularly useful to step through in the gdb debugger. We are oftem most concerned
 * with accurate management of our headers and how we track the space of the header itself and the
 * space it reports.
 */
#include "list_bestfit_tests.h"
#include "debug_break.h"


/* * * * * * * * * * * * * *     Debugging and Testing Functions   * * * * * * * * * * * * * * * */


/* @brief is_header_corrupted  will determine if a block has the 3rd bit on, which is invalid.
 * @param header_val           the valid header we will determine status for.
 * @return                     true if the block has the second or third bit on.
 */
static bool is_header_corrupted(header header_val) {
    return header_val & STATUS_CHECK;
}

/* @brief is_valid_header  checks the header of a block of memory to make sure that is not an
 *                         unreasonable size or otherwise corrupted.
 * @param cur_header       the header to a block of memory
 * @param block_size       the reported size of this block of memory from its header.
 * @param client_size      the size of the space available for the user.
 * @return                 true if the header is valid, false otherwise.
 */
static bool is_valid_header(header header_val, size_t block_size, size_t client_size) {
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

