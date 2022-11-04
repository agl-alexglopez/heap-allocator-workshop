/**
 * File: rbtree_stack_tests.c
 * --------------------------
 * This file contains the test implementation for the rbtree_stack allocator. These tests are most
 * helpful for the test_harness program and to use in gdb. If a failing test is run in gdb it will
 * automatically activate a breakpoint on the failing stackframe so we can see what went wrong.
 */

#include "../rbtree_stack_utilities.h"
#include "debug_break.h"


/* * * * * * * * * * * * * *     Debugging and Testing Functions   * * * * * * * * * * * * * * * */


/* @breif check_init    checks the internal representation of our heap, especially the head and tail
 *                      nodes for any issues that would ruin our algorithms.
 * @param client_start  the start of logically available space for user.
 * @param client_end    the end of logically available space for user.
 * @param heap_size     the total size in bytes of the heap.
 * @return              true if everything is in order otherwise false.
 */
bool check_init(void *client_start, void *client_end, size_t heap_size) {
    if (is_left_space(client_start)) {
        breakpoint();
        return false;
    }
    if ((byte *)client_end - (byte *)client_start
                           + (size_t)HEAP_NODE_WIDTH
                           != heap_size) {
        breakpoint();
        return false;
    }
    return true;
}

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
                        size_t heap_size, size_t tree_total) {
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    rb_node *cur_node = client_start;
    size_t size_used = HEAP_NODE_WIDTH;
    size_t total_free_nodes = 0;
    while (cur_node != client_end) {
        size_t block_size_check = get_size(cur_node->header);
        if (block_size_check == 0) {
            breakpoint();
            return false;
        }

        if (is_block_allocated(cur_node->header)) {
            size_used += block_size_check + HEADERSIZE;
        } else {
            total_free_nodes++;
            *total_free_mem += block_size_check + HEADERSIZE;
        }
        cur_node = get_right_neighbor(cur_node, block_size_check);
    }
    if (size_used + *total_free_mem != heap_size) {
        breakpoint();
        return false;
    }
    if (total_free_nodes != tree_total) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief is_red_red  determines if a red red violation of a red black tree has occured.
 * @param *root       the current root of the tree to begin at for checking all subtrees.
 * @param *black_nil  the sentinel node at the bottom of the tree that is always black.
 * @return            true if there is a red-red violation, false if we pass.
 */
bool is_red_red(const rb_node *root, const rb_node *black_nil) {
    if (root == black_nil ||
            (root->links[R] == black_nil && root->links[L] == black_nil)) {
        return false;
    }
    if (get_color(root->header) == RED) {
        if (get_color(root->links[L]->header) == RED
                || get_color(root->links[R]->header) == RED) {
            breakpoint();
            return true;
        }
    }
    return is_red_red(root->links[R], black_nil) || is_red_red(root->links[L], black_nil);
}

/* @brief calculate_bheight  determines if every path from a node to the tree.black_nil has the
 *                           same number of black nodes.
 * @param *root              the root of the tree to begin searching.
 * @param *black_nil         the sentinel node at the bottom of the tree that is always black.
 * @return                   -1 if the rule was not upheld, the black height if the rule is held.
 */
static int calculate_bheight(const rb_node *root, const rb_node *black_nil) {
    if (root == black_nil) {
        return 0;
    }
    int lf_bheight = calculate_bheight(root->links[L], black_nil);
    int rt_bheight = calculate_bheight(root->links[R], black_nil);
    int add = get_color(root->header) == BLACK;
    if (lf_bheight == -1 || rt_bheight == -1 || lf_bheight != rt_bheight) {
        breakpoint();
        return -1;
    }
    return lf_bheight + add;
}

/* @brief is_bheight_valid  the wrapper for calculate_bheight that verifies that the black height
 *                          property is upheld.
 * @param *root             the starting node of the red black tree to check.
 * @param *black_nil        the sentinel node at the bottom of the tree that is always black.
 * @return                  true if proper black height is consistently maintained throughout tree.
 */
bool is_bheight_valid(const rb_node *root, const rb_node *black_nil) {
    return calculate_bheight(root, black_nil) != -1;
}

/* @brief extract_tree_mem  sums the total memory in the red black tree to see if it matches the
 *                          total memory we got from traversing blocks of the heap.
 * @param *root             the root to start at for the summing recursive search.
 * @param *nil_and_tail     the address of a sentinel node serving as both list tail and black nil.
 * @return                  the total memory in bytes as a size_t in the red black tree.
 */
size_t extract_tree_mem(const rb_node *root, const void *nil_and_tail) {
    if (root == nil_and_tail) {
        return 0UL;
    }
    size_t total_mem = extract_tree_mem(root->links[R], nil_and_tail)
                       + extract_tree_mem(root->links[L], nil_and_tail);
    // We may have repeats so make sure to add the linked list values.
    size_t node_size = get_size(root->header) + HEADERSIZE;
    total_mem += node_size;
    duplicate_node *tally_list = root->list_start;
    if (tally_list != nil_and_tail) {
        // We have now entered a doubly linked list that uses left(prev) and right(next).
        while (tally_list != nil_and_tail) {
            total_mem += node_size;
            tally_list = tally_list->links[N];
        }
    }
    return total_mem;
}

/* @brief is_rbtree_mem_valid  a wrapper for tree memory sum function used to check correctness.
 * @param *root                the root node to begin at for the recursive summing search.
 * @param *nil_and_tail        address of a sentinel node serving as both list tail and black nil.
 * @param total_free_mem       the total free memory collected from a linear heap search.
 * @return                     true if the totals match false if they do not.
 */
bool is_rbtree_mem_valid(const rb_node *root, const void *nil_and_tail,
                                size_t total_free_mem) {
    return extract_tree_mem(root, nil_and_tail) == total_free_mem;
}

/* @brief calculate_bheight_V2  verifies that the height of a red-black tree is valid. This is a
 *                              similar function to calculate_bheight but comes from a more
 *                              reliable source, because I saw results that made me doubt V1.
 * @param *root                 the root to start at for the recursive search.
 * @param *black_nil            the sentinel node at the bottom of the tree that is always black.
 * @citation                    Julienne Walker's writeup on topdown Red-Black trees has a helpful
 *                              function for verifying black heights.
 */
static int calculate_bheight_V2(const rb_node *root, const rb_node *black_nil) {
    if (root == black_nil) {
        return 1;
    }
    int left_height = calculate_bheight_V2(root->links[L], black_nil);
    int right_height = calculate_bheight_V2(root->links[R], black_nil);
    if (left_height != 0 && right_height != 0 && left_height != right_height) {
        breakpoint();
        return 0;
    }
    if (left_height != 0 && right_height != 0) {
        return get_color(root->header) == RED ? left_height : left_height + 1;
    }
    return 0;
}

/* @brief is_bheight_valid_V2  the wrapper for calculate_bheight_V2 that verifies that the black
 *                             height property is upheld.
 * @param *root                the starting node of the red black tree to check.
 * @param *black_nil           the sentinel node at the bottom of the tree that is always black.
 * @return                     true if the paths are valid, false if not.
 */
bool is_bheight_valid_V2(const rb_node *root, const rb_node *black_nil) {
    return calculate_bheight_V2(root, black_nil) != 0;
}

/* @brief is_binary_tree  confirms the validity of a binary search tree. Nodes to the left should
 *                        be less than the root and nodes to the right should be greater.
 * @param *root           the root of the tree from which we examine children.
 * @param *black_nil      the sentinel node at the bottom of the tree that is always black.
 * @return                true if the tree is valid, false if not.
 */
bool is_binary_tree(const rb_node *root, const rb_node *black_nil) {
    if (root == black_nil) {
        return true;
    }
    size_t root_value = get_size(root->header);
    if (root->links[L] != black_nil && root_value < get_size(root->links[L]->header)) {
        breakpoint();
        return false;
    }
    if (root->links[R] != black_nil && root_value > get_size(root->links[R]->header)) {
        breakpoint();
        return false;
    }
    return is_binary_tree(root->links[L], black_nil) && is_binary_tree(root->links[R], black_nil);
}

/* @brief is_parent_valid  for duplicate node operations it is important to check the parents and
 *                         fields are updated corectly so we can continue using the tree.
 * @param *parent          the parent of the current root.
 * @param *root            the root to start at for the recursive search.
 * @param *nil_and_tail    address of a sentinel node serving as both list tail and black nil.
 * @return                 true if all parent child relationships are correct.
 */
bool is_duplicate_storing_parent(const rb_node *parent, const rb_node *root,
                                 const void *nil_and_tail) {
    if (root == nil_and_tail) {
        return true;
    }
    if (root->list_start != nil_and_tail && root->list_start->parent != parent) {
        breakpoint();
        return false;
    }
    return is_duplicate_storing_parent(root, root->links[L], nil_and_tail)
             && is_duplicate_storing_parent(root, root->links[R], nil_and_tail);
}

