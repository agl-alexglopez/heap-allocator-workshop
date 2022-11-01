
#include "rbtree_utility.h"
#include "debug_break.h"


/* * * * * * * * * * * * * *   Generic Helpers For Any Node Type   * * * * * * * * * * * * */


/* @brief paint_node  flips the third least significant bit to reflect the color of the node.
 * @param *node       the node we need to paint.
 * @param color       the color the user wants to paint the node.
 */
void paint_node(void *node, rb_color color) {
    color == RED ? ((*(header *)node) |= RED_PAINT) : ((*(header *)node) &= BLK_PAINT);
}

/* @brief get_color   returns the color of a node from the value of its header.
 * @param header_val  the value of the node in question passed by value.
 * @return            RED or BLACK
 */
rb_color get_color(header header_val) {
    return (header_val & COLOR_MASK) == RED_PAINT;
}

/* @brief get_size    returns size in bytes as a size_t from the value of node's header.
 * @param header_val  the value of the node in question passed by value.
 * @return            the size in bytes as a size_t of the node.
 */
size_t get_size(header header_val) {
    return SIZE_MASK & header_val;
}

/* @brief is_block_allocated  determines if a node is allocated or free.
 * @param block_header        the header value of a node passed by value.
 * @return                    true if allocated false if not.
 */
static bool is_block_allocated(header block_header) {
    return block_header & ALLOCATED;
}

/* @brief is_left_space  determines if the left neighbor of a block is free or allocated.
 * @param *node          the node to check.
 * @return               true if left is free false if left is allocated.
 */
static bool is_left_space(const void *node) {
    return !((*(header *)node) & LEFT_ALLOCATED);
}

/* @brief init_header_size  initializes any node as the size and indicating left is allocated. Left
 *                          is allocated because we always coalesce left and right.
 * @param *node             the region of possibly uninitialized heap we must initialize.
 * @param payload           the payload in bytes as a size_t of the current block we initialize
 */
static void init_header_size(void *node, size_t payload) {
    *(header *)node = LEFT_ALLOCATED | payload;
}

/* @brief init_footer  initializes footer at end of the heap block to matcht the current header.
 * @param *node        the current node with a header field we will use to set the footer.
 * @param payload      the size of the current nodes free memory.
 */
static void init_footer(const void *node, size_t payload) {
    header *footer = (header *)((byte *)node + payload);
    *footer = *(header *)node;
}

/* @brief get_right_neighbor  gets the address of the next rb_node in the heap to the right.
 * @param *current            the rb_node we start at to then jump to the right.
 * @param payload             the size in bytes as a size_t of the current rb_node block.
 * @return                    the rb_node to the right of the current.
 */
static void *get_right_neighbor(const void *current, size_t payload) {
    return ((byte *)current + HEADERSIZE + payload);
}

/* @brief *get_left_neighbor  uses the left block size gained from the footer to move to the header.
 * @param *node               the current header at which we reside.
 * @param left_block_size     the space of the left block as reported by its footer.
 * @return                    a header pointer to the header for the block to the left.
 */
static void *get_left_neighbor(const void *node) {
    header *left_footer = (header *)((byte *)node - HEADERSIZE);
    return ((byte *)node - (*left_footer & SIZE_MASK) - HEADERSIZE);
}

/* @brief get_client_space  steps into the client space just after the header of a rb_node.
 * @param *node_header      the rb_node we start at before retreiving the client space.
 * @return                  the void* address of the client space they are now free to use.
 */
static void *get_client_space(const void *node) {
    return (byte *)node + HEADERSIZE;
}

/* @brief get_rb_node    steps to the rb_node header from the space the client was using.
 * @param *client_space  the void* the client was using for their type. We step to the left.
 * @return               a pointer to the rb_node of our heap block.
 */
static void *get_rb_node(const void *client_space) {
    return ((byte *) client_space - HEADERSIZE);
}


/* * * * * * * * * * * * * *   Tree Debuggers For Each Node Type   * * * * * * * * * * * * */


/* @breif check_init  checks the internal representation of our heap, especially the head and tail
 *                    nodes for any issues that would ruin our algorithms.
 * @return            true if everything is in order otherwise false.
 */
static bool check_init(void *client_start, void *client_end, size_t heap_size,
                       rb_node_width width) {
    if (is_left_space(client_start)) {
        breakpoint();
        return false;
    }
    if ((byte *)client_end - (byte *)client_start
                           + (size_t)width
                           != heap_size) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief is_memory_balanced  loops through all blocks of memory to verify that the sizes
 *                            reported match the global bookeeping in our struct.
 * @param *total_free_mem     the output parameter of the total size used as another check.
 * @return                    true if our tallying is correct and our totals match.
 */
static bool is_memory_balanced(size_t *total_free_mem, void *client_start, void *client_end,
                               size_t heap_size, size_t tree_total, rb_node_width width) {
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    void *cur_node = client_start;
    size_t size_used = width;
    size_t total_free_nodes = 0;
    while (cur_node != client_end) {
        size_t block_size_check = get_size(*(header *)cur_node);
        if (block_size_check == 0) {
            breakpoint();
            return false;
        }

        if (is_block_allocated(*(header *)cur_node)) {
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

/* @brief get_std_black_height  gets the black node height of the tree excluding the current node.
 * @param *root                  the starting root to search from to find the height.
 * @return                       the black height from the current node as an integer.
 */
static int get_clrs_black_height(const rb_clrs_node *root, const rb_clrs_node *black_nil) {
    if (root == black_nil) {
        return 0;
    }
    if (get_color(root->left->header) == BLACK) {
        return 1 + get_clrs_black_height(root->left, black_nil);
    }
    return get_clrs_black_height(root->left, black_nil);
}

/* @brief get_unif_black_height  gets the black node height of the tree excluding the current node.
 * @param *root                  the starting root to search from to find the height.
 * @return                       the black height from the current node as an integer.
 */
static int get_unif_black_height(const rb_unif_node *root, const rb_unif_node *black_nil) {
    if (root == black_nil) {
        return 0;
    }
    if (get_color(root->links[L]->header) == BLACK) {
        return 1 + get_unif_black_height(root->links[L], black_nil);
    }
    return get_unif_black_height(root->links[L], black_nil);
}

/* @brief get_link_black_height  gets the black node height of the tree excluding the current node.
 * @param *root                  the starting root to search from to find the height.
 * @return                       the black height from the current node as an integer.
 */
static int get_link_black_height(const rb_link_node *root, const rb_link_node *black_nil) {
    if (root == black_nil) {
        return 0;
    }
    if (get_color(root->links[L]->header) == BLACK) {
        return 1 + get_link_black_height(root->links[L], black_nil);
    }
    return get_link_black_height(root->links[L], black_nil);
}

/* @brief get_cstm_black_height  gets the black node height of the tree excluding the current node.
 * @param *root                  the starting root to search from to find the height.
 * @return                       the black height from the current node as an integer.
 */
static int get_cstm_black_height(const rb_cstm_node *root, const rb_cstm_node *black_nil) {
    if (root == black_nil) {
        return 0;
    }
    if (get_color(root->links[L]->header) == BLACK) {
        return 1 + get_cstm_black_height(root->links[L], black_nil);
    }
    return get_cstm_black_height(root->links[L], black_nil);
}

/* @brief is_clrs_red_red  determines if a red red violation of a red black tree has occured.
 * @param *root            the current root of the tree to begin at for checking all subtrees.
 */
static bool is_clrs_red_red(const rb_clrs_node *root, const rb_clrs_node *black_nil) {
    if (root == black_nil ||
            (root->right == black_nil && root->left == black_nil)) {
        return false;
    }
    if (get_color(root->header) == RED) {
        if (get_color(root->left->header) == RED
                || get_color(root->right->header) == RED) {
            breakpoint();
            return true;
        }
    }
    return is_clrs_red_red(root->right, black_nil) || is_clrs_red_red(root->left, black_nil);
}

/* @brief is_unif_red_red  determines if a red red violation of a red black tree has occured.
 * @param *root            the current root of the tree to begin at for checking all subtrees.
 */
static bool is_unif_red_red(const rb_unif_node *root, const rb_unif_node *black_nil) {
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
    return is_unif_red_red(root->links[R], black_nil)
               || is_unif_red_red(root->links[L], black_nil);
}

/* @brief is_link_red_red  determines if a red red violation of a red black tree has occured.
 * @param *root            the current root of the tree to begin at for checking all subtrees.
 */
static bool is_link_red_red(const rb_link_node *root, const rb_link_node *black_nil) {
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
    return is_link_red_red(root->links[R], black_nil)
               || is_link_red_red(root->links[L], black_nil);
}

/* @brief is_cstm_red_red  determines if a red red violation of a red black tree has occured.
 * @param *root            the current root of the tree to begin at for checking all subtrees.
 */
static bool is_cstm_red_red(const rb_cstm_node *root, const rb_cstm_node *black_nil) {
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
    return is_cstm_red_red(root->links[R], black_nil)
               || is_cstm_red_red(root->links[L], black_nil);
}

/* @brief extract_clrs_mem  sums total memory in the red black tree to see if it matches the
 *                          total memory we got from traversing blocks of the heap.
 * @param *root             the root to start at for the summing recursive search.
 * @return                  the total memory in bytes as a size_t in the red black tree.
 */
static size_t extract_clrs_mem(const rb_clrs_node *root, const rb_clrs_node *black_nil) {
    if (root == black_nil) {
        return 0UL;
    }
    size_t total_mem = extract_clrs_mem(root->right, black_nil)
                           + extract_clrs_mem(root->left, black_nil);
    // We may have repeats so make sure to add the linked list values.
    total_mem += get_size(root->header) + HEADERSIZE;
    return total_mem;
}

/* @brief is_clrs_mem_valid  a wrapper for tree memory sum function used to check correctness.
 * @param *root              the root node to begin at for the recursive summing search.
 * @return                   true if the totals match false if they do not.
 */
static bool is_clrs_mem_valid(const rb_clrs_node *root, const rb_clrs_node *black_nil,
                              size_t total_free_mem) {
    if (extract_clrs_mem(root, black_nil) != total_free_mem) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief extract_unif_mem  sums total memory in the red black tree to see if it matches the
 *                          total memory we got from traversing blocks of the heap.
 * @param *root             the root to start at for the summing recursive search.
 * @return                  the total memory in bytes as a size_t in the red black tree.
 */
static size_t extract_unif_mem(const rb_unif_node *root, const rb_unif_node *black_nil) {
    if (root == black_nil) {
        return 0UL;
    }
    size_t total_mem = extract_unif_mem(root->links[R], black_nil)
                           + extract_unif_mem(root->links[L], black_nil);
    // We may have repeats so make sure to add the linked list values.
    total_mem += get_size(root->header) + HEADERSIZE;
    return total_mem;
}

/* @brief is_unif_mem_valid  a wrapper for tree memory sum function used to check correctness.
 * @param *root              the root node to begin at for the recursive summing search.
 * @return                   true if the totals match false if they do not.
 */
static bool is_unif_mem_valid(const rb_unif_node *root, const rb_unif_node *black_nil,
                              size_t total_free_mem) {
    if (extract_unif_mem(root, black_nil) != total_free_mem) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief extract_link_mem  sums total memory in the red black tree to see if it matches the
 *                          total memory we got from traversing blocks of the heap.
 * @param *root             the root to start at for the summing recursive search.
 * @return                  the total memory in bytes as a size_t in the red black tree.
 */
static size_t extract_link_mem(const rb_link_node *root, const rb_link_node *black_nil) {
    if (root == black_nil) {
        return 0UL;
    }
    size_t total_mem = extract_link_mem(root->links[R], black_nil)
                           + extract_link_mem(root->links[L], black_nil);
    // We may have repeats so make sure to add the linked list values.
    total_mem += get_size(root->header) + HEADERSIZE;
    return total_mem;
}

/* @brief is_link_mem_valid  a wrapper for tree memory sum function used to check correctness.
 * @param *root              the root node to begin at for the recursive summing search.
 * @return                   true if the totals match false if they do not.
 */
static bool is_link_mem_valid(const rb_link_node *root, const rb_link_node *black_nil,
                              size_t total_free_mem) {
    if (extract_link_mem(root, black_nil) != total_free_mem) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief extract_cstm_mem  sums total memory in the red black tree to see if it matches the
 *                          total memory we got from traversing blocks of the heap.
 * @param *root             the root to start at for the summing recursive search.
 * @return                  the total memory in bytes as a size_t in the red black tree.
 */
static size_t extract_cstm_mem(const rb_cstm_node *root, const rb_cstm_node *black_nil) {
    if (root == black_nil) {
        return 0UL;
    }
    size_t total_mem = extract_cstm_mem(root->links[R], black_nil)
                           + extract_cstm_mem(root->links[L], black_nil);
    // We may have repeats so make sure to add the linked list values.
    total_mem += get_size(root->header) + HEADERSIZE;
    return total_mem;
}

/* @brief is_cstm_mem_valid  a wrapper for tree memory sum function used to check correctness.
 * @param *root              the root node to begin at for the recursive summing search.
 * @return                   true if the totals match false if they do not.
 */
static bool is_cstm_mem_valid(const rb_cstm_node *root, const rb_cstm_node *black_nil,
                              size_t total_free_mem) {
    if (extract_cstm_mem(root, black_nil) != total_free_mem) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief calculate_clrs_bheight  verifies that the height of a red-black tree is valid. This is a
 *                                similar function to calculate_bheight but comes from a more
 *                                reliable source, because I saw results that made me doubt V1.
 * @citation                      Julienne Walker's writeup on topdown Red-Black trees has a helpful
 *                                function for verifying black heights.
 */
static int calculate_clrs_bheight(const rb_clrs_node *root, const rb_clrs_node *black_nil) {
    if (root == black_nil) {
        return 1;
    }
    int left_height = calculate_clrs_bheight(root->left, black_nil);
    int right_height = calculate_clrs_bheight(root->right, black_nil);
    if (left_height != 0 && right_height != 0 && left_height != right_height) {
        breakpoint();
        return 0;
    }
    if (left_height != 0 && right_height != 0) {
        return get_color(root->header) == RED ? left_height : left_height + 1;
    }
    return 0;
}

/* @brief is_clrs_bheight_valid  the wrapper for calculate_bheight_V2 that verifies that the black
 *                               height property is upheld.
 * @param *root                  the starting node of the red black tree to check.
 * @return                       true if the paths are valid, false if not.
 */
static bool is_clrs_bheight_valid(const rb_clrs_node *root, const rb_clrs_node *black_nil) {
    return calculate_clrs_bheight(root, black_nil) != 0;
}

/* @brief calculate_unif_bheight  verifies that the height of a red-black tree is valid. This is a
 *                                similar function to calculate_bheight but comes from a more
 *                                reliable source, because I saw results that made me doubt V1.
 * @citation                      Julienne Walker's writeup on topdown Red-Black trees has a helpful
 *                                function for verifying black heights.
 */
static int calculate_unif_bheight(const rb_unif_node *root, const rb_unif_node *black_nil) {
    if (root == black_nil) {
        return 1;
    }
    int left_height = calculate_unif_bheight(root->links[L], black_nil);
    int right_height = calculate_unif_bheight(root->links[R], black_nil);
    if (left_height != 0 && right_height != 0 && left_height != right_height) {
        breakpoint();
        return 0;
    }
    if (left_height != 0 && right_height != 0) {
        return get_color(root->header) == RED ? left_height : left_height + 1;
    }
    return 0;
}

/* @brief is_unif_bheight_valid  the wrapper for calculate_bheight_V2 that verifies that the black
 *                               height property is upheld.
 * @param *root                  the starting node of the red black tree to check.
 * @return                       true if the paths are valid, false if not.
 */
static bool is_unif_bheight_valid(const rb_unif_node *root, const rb_unif_node *black_nil) {
    return calculate_unif_bheight(root, black_nil) != 0;
}

/* @brief calculate_link_bheight  verifies that the height of a red-black tree is valid. This is a
 *                                similar function to calculate_bheight but comes from a more
 *                                reliable source, because I saw results that made me doubt V1.
 * @citation                      Julienne Walker's writeup on topdown Red-Black trees has a helpful
 *                                function for verifying black heights.
 */
static int calculate_link_bheight(const rb_link_node *root, const rb_link_node *black_nil) {
    if (root == black_nil) {
        return 1;
    }
    int left_height = calculate_link_bheight(root->links[L], black_nil);
    int right_height = calculate_link_bheight(root->links[R], black_nil);
    if (left_height != 0 && right_height != 0 && left_height != right_height) {
        breakpoint();
        return 0;
    }
    if (left_height != 0 && right_height != 0) {
        return get_color(root->header) == RED ? left_height : left_height + 1;
    }
    return 0;
}

/* @brief is_link_bheight_valid  the wrapper for calculate_bheight_V2 that verifies that the black
 *                               height property is upheld.
 * @param *root                  the starting node of the red black tree to check.
 * @return                       true if the paths are valid, false if not.
 */
static bool is_link_bheight_valid(const rb_link_node *root, const rb_link_node *black_nil) {
    return calculate_link_bheight(root, black_nil) != 0;
}

/* @brief calculate_cstm_bheight  verifies that the height of a red-black tree is valid. This is a
 *                                similar function to calculate_bheight but comes from a more
 *                                reliable source, because I saw results that made me doubt V1.
 * @citation                      Julienne Walker's writeup on topdown Red-Black trees has a helpful
 *                                function for verifying black heights.
 */
static int calculate_cstm_bheight(const rb_cstm_node *root, const rb_cstm_node *black_nil) {
    if (root == black_nil) {
        return 1;
    }
    int left_height = calculate_cstm_bheight(root->links[L], black_nil);
    int right_height = calculate_cstm_bheight(root->links[R], black_nil);
    if (left_height != 0 && right_height != 0 && left_height != right_height) {
        breakpoint();
        return 0;
    }
    if (left_height != 0 && right_height != 0) {
        return get_color(root->header) == RED ? left_height : left_height + 1;
    }
    return 0;
}

/* @brief is_cstm_bheight_valid  the wrapper for calculate_bheight_V2 that verifies that the black
 *                               height property is upheld.
 * @param *root                  the starting node of the red black tree to check.
 * @return                       true if the paths are valid, false if not.
 */
static bool is_cstm_bheight_valid(const rb_cstm_node *root, const rb_cstm_node *black_nil) {
    return calculate_cstm_bheight(root, black_nil) != 0;
}

/* @brief is_clrs_parent_valid  for duplicate node operations it is important to check the parents and
 *                              fields are updated corectly so we can continue using the tree.
 * @param *root                 the root to start at for the recursive search.
 */
static bool is_clrs_parent_valid(const rb_clrs_node *root, const rb_clrs_node *black_nil) {
    if (root == black_nil) {
        return true;
    }
    if (root->left != black_nil && root->left->parent != root) {
        breakpoint();
        return false;
    }
    if (root->right != black_nil && root->right->parent != root) {
        breakpoint();
        return false;
    }
    return is_clrs_parent_valid(root->left, black_nil)
               && is_clrs_parent_valid(root->right, black_nil);
}

/* @brief is_unif_parent_valid  for duplicate node operations it is important to check the parents and
 *                              fields are updated corectly so we can continue using the tree.
 * @param *root                 the root to start at for the recursive search.
 */
static bool is_unif_parent_valid(const rb_unif_node *root, const rb_unif_node *black_nil) {
    if (root == black_nil) {
        return true;
    }
    if (root->links[L] != black_nil && root->links[L]->parent != root) {
        breakpoint();
        return false;
    }
    if (root->links[R] != black_nil && root->links[R]->parent != root) {
        breakpoint();
        return false;
    }
    return is_unif_parent_valid(root->links[L], black_nil)
               && is_unif_parent_valid(root->links[R], black_nil);
}

/* @brief is_link_parent_valid  for duplicate node operations it is important to check the parents and
 *                              fields are updated corectly so we can continue using the tree.
 * @param *root                 the root to start at for the recursive search.
 */
static bool is_link_parent_valid(const rb_link_node *root, const rb_link_node *black_nil) {
    if (root == black_nil) {
        return true;
    }
    if (root->links[L] != black_nil && root->links[L]->parent != root) {
        breakpoint();
        return false;
    }
    if (root->links[R] != black_nil && root->links[R]->parent != root) {
        breakpoint();
        return false;
    }
    return is_link_parent_valid(root->links[L], black_nil)
               && is_link_parent_valid(root->links[R], black_nil);
}

/* @brief is_cstm_parent_valid  for duplicate node operations it is important to check the parents and
 *                              fields are updated corectly so we can continue using the tree.
 * @param *root                 the root to start at for the recursive search.
 */
static bool is_cstm_parent_valid(const rb_cstm_node *root,
                                 const rb_cstm_node *parent,
                                 const rb_cstm_node *black_nil,
                                 const duplicate_cstm_node *list_tail) {
    if (root == black_nil) {
        return true;
    }
    if (root->list_start != list_tail && root->list_start->parent != parent) {
        breakpoint();
        return false;
    }
    return is_cstm_parent_valid(root->links[L], root, black_nil, list_tail)
               && is_cstm_parent_valid(root->links[R], root, black_nil, list_tail);
}

/* @brief is_clrs_tree  confirms the validity of a binary search tree. Nodes to the left should
 *                      be less than the root and nodes to the right should be greater.
 * @param *root         the root of the tree from which we examine children.
 * @return              true if the tree is valid, false if not.
 */
static bool is_clrs_tree(const rb_clrs_node *root, const rb_clrs_node *black_nil) {
    if (root == black_nil) {
        return true;
    }
    size_t root_value = get_size(root->header);
    if (root->left != black_nil && root_value < get_size(root->left->header)) {
        breakpoint();
        return false;
    }
    if (root->right != black_nil && root_value > get_size(root->right->header)) {
        breakpoint();
        return false;
    }
    return is_clrs_tree(root->left, black_nil) && is_clrs_tree(root->right, black_nil);
}

/* @brief is_unif_tree  confirms the validity of a binary search tree. Nodes to the left should
 *                      be less than the root and nodes to the right should be greater.
 * @param *root         the root of the tree from which we examine children.
 * @return              true if the tree is valid, false if not.
 */
static bool is_unif_tree(const rb_unif_node *root, const rb_unif_node *black_nil) {
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
    return is_unif_tree(root->links[L], black_nil) && is_unif_tree(root->links[R], black_nil);
}

/* @brief is_link_tree  confirms the validity of a binary search tree. Nodes to the left should
 *                      be less than the root and nodes to the right should be greater.
 * @param *root         the root of the tree from which we examine children.
 * @return              true if the tree is valid, false if not.
 */
static bool is_link_tree(const rb_link_node *root, const rb_link_node *black_nil) {
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
    return is_link_tree(root->links[L], black_nil) && is_link_tree(root->links[R], black_nil);
}

/* @brief is_cstm_tree  confirms the validity of a binary search tree. Nodes to the left should
 *                      be less than the root and nodes to the right should be greater.
 * @param *root         the root of the tree from which we examine children.
 * @return              true if the tree is valid, false if not.
 */
static bool is_cstm_tree(const rb_cstm_node *root, const rb_cstm_node *black_nil) {
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
    return is_cstm_tree(root->links[L], black_nil) && is_cstm_tree(root->links[R], black_nil);
}

