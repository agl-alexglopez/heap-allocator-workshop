/**
 * File: list_segregated_tests.c
 * -----------------------------
 * This file contains tests for the list_segregated heap allocator. Run these in gdb to activate
 * the breakpoint() function that will stop program execution. Examine the stack frame of the
 * current test to see what is going wrong.
 */
#include <limits.h>
#include "list_segregated_utility.h"
#include "debug_break.h"


/* * * * * * * * * * * * * *     Debugging and Testing Functions   * * * * * * * * * * * * * * * */


/* @brief is_header_corrupted   will determine if a block has the 3rd bit on, which is invalid.
 * @param *cur_header_location  the valid header we will determine status for.
 * @return                      true if the block has the second or third bit on.
 */
bool is_header_corrupted(header header_val) {
    return header_val & STATUS_CHECK;
}

/* @breif check_init   checks the internal representation of our heap, especially the head and tail
 *                     nodes for any issues that would ruin our algorithms.
 * @param table[]      the lookup table that holds the list size ranges
 * @param *nil         a special free_node that serves as a sentinel for logic and edgecases.
 * @param client_size  the total space available for client.
 * @return             true if everything is in order otherwise false.
 */
bool check_init(seg_node table[], free_node *nil, size_t client_size) {
    void *first_address = table;
    void *last_address = (byte *)nil + FREE_NODE_WIDTH;
    if ((size_t)((byte *)last_address - (byte *)first_address) != client_size) {
        breakpoint();
        return false;
    }
    // Check our lookup table. Sizes should never be altered and pointers should never be NULL.
    unsigned short size = MIN_BLOCK_SIZE;
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

/* @brief is_valid_header  checks the header of a block of memory to make sure that is not an
 *                         unreasonable size or otherwise corrupted.
 * @param *cur_header      the header to a block of memory
 * @param block_size       the reported size of this block of memory from its header.
 * @client_size            the entire space available to the user.
 * @return                 true if the header is valid, false otherwise.
 */
bool is_valid_header(header cur_header, size_t block_size, size_t client_size) {
    // Most definitely impossible and our header is corrupted. Pointer arithmetic would fail.
    if (block_size > client_size) {
        return false;
    }
    if (is_header_corrupted(cur_header)) {
        return false;
    }
    if (block_size % HEADERSIZE != 0) {
        return false;
    }
    return true;
}

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
                        size_t client_size, size_t fits_total) {
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    header *cur_header = client_start;
    size_t size_used = FREE_NODE_WIDTH + TABLE_BYTES;
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
    if (total_free_nodes != fits_total) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief are_fits_valid  loops through only the segregated fits list to make sure it matches
 *                        the loop we just completed by checking all blocks.
 * @param total_free_mem  the input from a previous loop that was completed by jumping block
 *                        by block over the entire heap.
 * @param table[]         the lookup table that holds the list size ranges
 * @param *nil            a special free_node that serves as a sentinel for logic and edgecases.
 * @return                true if the segregated fits list totals correctly, false if not.
 */
bool are_fits_valid(size_t total_free_mem, seg_node table[], free_node *nil) {
    size_t linked_free_mem = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        for (free_node *cur = table[i].start; cur != nil; cur = cur->next) {
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
            if (is_left_space(get_block_header(cur))) {
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

