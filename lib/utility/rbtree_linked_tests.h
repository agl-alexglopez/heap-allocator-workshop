/**
 * File: rbtree_linked_tests.h
 * ---------------------------
 * This file contains the interface for testing the rbtree_linked allocator. We introduce the new
 * complexity of storing duplicate nodes in a linked list attached to the tree nodes. If run in
 * gdb these functions will stop execution during the failing test in the stack frame that failed.
 */
#ifndef RBTREE_LINKED_TESTS_H
#define RBTREE_LINKED_TESTS_H

#include <stddef.h>
#include <stdbool.h>
#include "rbtree_linked_design.h"


/* * * * * * * * * * * * * *     Debugging and Testing Functions   * * * * * * * * * * * * * * * */


/* @breif check_init    checks the internal representation of our heap, especially the head and tail
 *                      nodes for any issues that would ruin our algorithms.
 * @param client_start  the start of logically available space for user.
 * @param client_end    the end of logically available space for user.
 * @param heap_size     the total size in bytes of the heap.
 * @return              true if everything is in order otherwise false.
 */
bool check_init(void *client_start, void *client_end, size_t heap_size);

/* @brief is_memory_balanced  loops through all blocks of memory to verify that the sizes
 *                            reported match the global bookeeping in our struct.
 * @param *total_free_mem     the output parameter of the total size used as another check.
 * @param client_start        the start of logically available space for user.
 * @param client_end          the end of logically available space for user.
 * @param heap_size           the total size in bytes of the heap.
 * @param tree_total          the total nodes in the red-black tree.
 * @return                    true if our tallying is correct and our totals match.
 */
bool is_memory_balanced(size_t *total_free_mem, void *client_start, void *client_end,
                        size_t heap_size, size_t tree_total);

/* @brief is_red_red  determines if a red red violation of a red black tree has occured.
 * @param *root       the current root of the tree to begin at for checking all subtrees.
 * @param *black_nil  the sentinel node at the bottom of the tree that is always black.
 * @return            true if there is a red-red violation, false if we pass.
 */
bool is_red_red(const rb_node *root, const rb_node *black_nil);

/* @brief calculate_bheight  determines if every path from a node to the tree.black_nil has the
 *                           same number of black nodes.
 * @param *root              the root of the tree to begin searching.
 * @param *black_nil         the sentinel node at the bottom of the tree that is always black.
 * @return                   -1 if the rule was not upheld, the black height if the rule is held.
 */
int calculate_bheight(const rb_node *root, const rb_node *black_nil);

/* @brief is_bheight_valid  the wrapper for calculate_bheight that verifies that the black height
 *                          property is upheld.
 * @param *root             the starting node of the red black tree to check.
 * @param *black_nil        the sentinel node at the bottom of the tree that is always black.
 * @return                  true if proper black height is consistently maintained throughout tree.
 */
bool is_bheight_valid(const rb_node *root, const rb_node *black_nil);

/* @brief extract_tree_mem  sums the total memory in the red black tree to see if it matches the
 *                          total memory we got from traversing blocks of the heap.
 * @param *root             the root to start at for the summing recursive search.
 * @param *nil_and_tail     the address of a sentinel node serving as both list tail and black nil.
 * @return                  the total memory in bytes as a size_t in the red black tree.
 */
size_t extract_tree_mem(const rb_node *root, const void *nil_and_tail);

/* @brief is_rbtree_mem_valid  a wrapper for tree memory sum function used to check correctness.
 * @param *root                the root node to begin at for the recursive summing search.
 * @param *nil_and_tail        address of a sentinel node serving as both list tail and black nil.
 * @param total_free_mem       the total free memory collected from a linear heap search.
 * @return                     true if the totals match false if they do not.
 */
bool is_rbtree_mem_valid(const rb_node *root, const void *nil_and_tail, size_t total_free_mem);

/* @brief is_parent_valid  for duplicate node operations it is important to check the parents and
 *                         fields are updated corectly so we can continue using the tree.
 * @param *root            the root to start at for the recursive search.
 * @param *black_nil       the sentinel node at the bottom of the tree that is always black.
 * @return                 true if all parent child relationships are correct.
 */
bool is_parent_valid(const rb_node *root, const rb_node *black_nil);

/* @brief calculate_bheight_V2  verifies that the height of a red-black tree is valid. This is a
 *                              similar function to calculate_bheight but comes from a more
 *                              reliable source, because I saw results that made me doubt V1.
 * @param *root                 the root to start at for the recursive search.
 * @param *black_nil            the sentinel node at the bottom of the tree that is always black.
 * @citation                    Julienne Walker's writeup on topdown Red-Black trees has a helpful
 *                              function for verifying black heights.
 */
int calculate_bheight_V2(const rb_node *root, const rb_node *black_nil);

/* @brief is_bheight_valid_V2  the wrapper for calculate_bheight_V2 that verifies that the black
 *                             height property is upheld.
 * @param *root                the starting node of the red black tree to check.
 * @param *black_nil           the sentinel node at the bottom of the tree that is always black.
 * @return                     true if the paths are valid, false if not.
 */
bool is_bheight_valid_V2(const rb_node *root, const rb_node *black_nil);

/* @brief is_binary_tree  confirms the validity of a binary search tree. Nodes to the left should
 *                        be less than the root and nodes to the right should be greater.
 * @param *root           the root of the tree from which we examine children.
 * @param *black_nil      the sentinel node at the bottom of the tree that is always black.
 * @return                true if the tree is valid, false if not.
 */
bool is_binary_tree(const rb_node *root, const rb_node *black_nil);


#endif
