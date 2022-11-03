/**
 * File: list_segregated_tests.h
 * -------------------------------
 * This file contains the testing interface for the list_segregated allocator. Add tests here as
 * needed. If a failing test is run in gdb you will find the exact stack frame in which it failed
 * which is helpful for debugging heap operations.
 */
#ifndef LIST_SEGREGATED_TESTS_H
#define LIST_SEGREGATED_TESTS_H

#include <stdbool.h>
#include <stddef.h>
#include "list_segregated_utility.h"


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


#endif
