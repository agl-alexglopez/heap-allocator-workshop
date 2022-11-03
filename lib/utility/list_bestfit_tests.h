/**
 * File: list_bestfit_tests.h
 * ---------------------------
 * This file contains tests relevant the the list_bestfit allocator. Add more tests here as needed.
 * These are particularly useful to step through in the gdb debugger.
 */
#ifndef LIST_BESTFIT_TESTS_H
#define LIST_BESTFIT_TESTS_H

#include <stdbool.h>
#include <stddef.h>
#include "list_bestfit_design.h"


/* * * * * * * * * * * * * *     Debugging and Testing Functions   * * * * * * * * * * * * * * * */


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
                free_node *head, free_node *tail);

/* @brief get_size_used    loops through all blocks of memory to verify that the sizes
 *                         reported match the global bookeeping in our struct.
 * @param *total_free_mem  the output parameter of the total size used as another check.
 * @param *client_start    the start address of the client heap segment.
 * @param *client_end      the end address of the client heap segment.
 * @param client_size      the size in bytes of the total space available for client.
 * @param free_list_total  the total number of free nodes in our list.
 * @return                 true if our tallying is correct and our totals match.
 */
bool is_memory_balanced(size_t *total_free_mem, void *client_start,
                        void *client_end, size_t client_size, size_t free_list_total);

/* @brief is_free_list_valid  loops through only the doubly linked list to make sure it matches
 *                            the loop we just completed by checking all blocks.
 * @param total_free_mem      the input from a previous loop that was completed by jumping block
 *                            by block over the entire heap.
 * @param *head               the free_node head of the linked list.
 * @param *tail               the free_node tail of the linked list.
 * @return                    true if the doubly linked list totals correctly, false if not.
 */
bool is_free_list_valid(size_t total_free_mem, free_node *head, free_node *tail);


#endif
