/**
 * File rbtree_unified_utility.c
 * ---------------------------------
 * This file contains the implementation of utility functions for the rbtree_unified heap
 * allocator. These functions serve as basic navigation for nodes and blocks, testing functions for
 * heap debugging, and printing functions for heap debugging. These functions can distract from the
 * algorithm implementations in the actual rbtree_unified.c file so we seperate them out here.
 */
#include <limits.h>
#include <stdlib.h>
#include "rbtree_unified_utility.h"
#include "debug_break.h"


/* * * * * * * * * * * * * *    Basic Block and Header Operations  * * * * * * * * * * * * * * * */


/* @brief paint_node  flips the third least significant bit to reflect the color of the node.
 * @param *node       the node we need to paint.
 * @param color       the color the user wants to paint the node.
 */
void paint_node(rb_node *node, rb_color color) {
    color == RED ? (node->header |= RED_PAINT) : (node->header &= BLK_PAINT);
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

/* @brief get_min     returns the smallest node in a valid binary search tree.
 * @param *root       the root of any valid binary search tree.
 * @param *black_nil  the sentinel node at the bottom of the tree that is always black.
 * @return            a pointer to the minimum node in a valid binary search tree.
 */
rb_node *get_min(rb_node *root, rb_node *black_nil) {
    for (; root->links[L] != black_nil; root = root->links[L]) {
    }
    return root;
}

/* @brief is_block_allocated  determines if a node is allocated or free.
 * @param block_header        the header value of a node passed by value.
 * @return                    true if allocated false if not.
 */
bool is_block_allocated(const header block_header) {
    return block_header & ALLOCATED;
}

/* @brief is_left_space  determines if the left neighbor of a block is free or allocated.
 * @param *node          the node to check.
 * @return               true if left is free false if left is allocated.
 */
bool is_left_space(const rb_node *node) {
    return !(node->header & LEFT_ALLOCATED);
}

/* @brief init_header_size  initializes any node as the size and indicating left is allocated. Left
 *                          is allocated because we always coalesce left and right.
 * @param *node             the region of possibly uninitialized heap we must initialize.
 * @param payload           the payload in bytes as a size_t of the current block we initialize
 */
void init_header_size(rb_node *node, size_t payload) {
    node->header = LEFT_ALLOCATED | payload;
}

/* @brief init_footer  initializes footer at end of the heap block to matcht the current header.
 * @param *node        the current node with a header field we will use to set the footer.
 * @param payload      the size of the current nodes free memory.
 */
void init_footer(rb_node *node, size_t payload) {
    header *footer = (header *)((byte *)node + payload);
    *footer = node->header;
}

/* @brief get_right_neighbor  gets the address of the next rb_node in the heap to the right.
 * @param *current            the rb_node we start at to then jump to the right.
 * @param payload             the size in bytes as a size_t of the current rb_node block.
 * @return                    the rb_node to the right of the current.
 */
rb_node *get_right_neighbor(const rb_node *current, size_t payload) {
    return (rb_node *)((byte *)current + HEADERSIZE + payload);
}

/* @brief *get_left_neighbor  uses the left block size gained from the footer to move to the header.
 * @param *node               the current header at which we reside.
 * @return                    a header pointer to the header for the block to the left.
 */
rb_node *get_left_neighbor(const rb_node *node) {
    header *left_footer = (header *)((byte *)node - HEADERSIZE);
    return (rb_node *)((byte *)node - (*left_footer & SIZE_MASK) - HEADERSIZE);
}


/* @brief get_client_space  steps into the client space just after the header of a rb_node.
 * @param *node_header      the rb_node we start at before retreiving the client space.
 * @return                  the void* address of the client space they are now free to use.
 */
void *get_client_space(const rb_node *node_header) {
    return (byte *) node_header + HEADERSIZE;
}

/* @brief get_rb_node    steps to the rb_node header from the space the client was using.
 * @param *client_space  the void* the client was using for their type. We step to the left.
 * @return               a pointer to the rb_node of our heap block.
 */
rb_node *get_rb_node(const void *client_space) {
    return (rb_node *)((byte *) client_space - HEADERSIZE);
}


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

/* @brief get_black_height  gets the black node height of the tree excluding the current node.
 * @param *root             the starting root to search from to find the height.
 * @param *black_nil        the sentinel node at the bottom of the tree that is always black.
 * @return                  the black height from the current node as an integer.
 */
int get_black_height(const rb_node *root, const rb_node *black_nil) {
    if (root == black_nil) {
        return 0;
    }
    if (get_color(root->links[L]->header) == BLACK) {
        return 1 + get_black_height(root->links[L], black_nil);
    }
    return get_black_height(root->links[L], black_nil);
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
int calculate_bheight(const rb_node *root, const rb_node *black_nil) {
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
 * @param *black_nil        the sentinel node at the bottom of the tree that is always black.
 * @return                  the total memory in bytes as a size_t in the red black tree.
 */
size_t extract_tree_mem(const rb_node *root, const rb_node *black_nil) {
    if (root == black_nil) {
        return 0UL;
    }
    size_t total_mem = extract_tree_mem(root->links[R], black_nil)
                           + extract_tree_mem(root->links[L], black_nil);
    // We may have repeats so make sure to add the linked list values.
    total_mem += get_size(root->header) + HEADERSIZE;
    return total_mem;
}

/* @brief is_rbtree_mem_valid  a wrapper for tree memory sum function used to check correctness.
 * @param *root                the root node to begin at for the recursive summing search.
 * @param *black_nil           the sentinel node at the bottom of the tree that is always black.
 * @param total_free_mem       the previously calculated free memory from a linear heap search.
 * @return                     true if the totals match false if they do not.
 */
bool is_rbtree_mem_valid(const rb_node *root, const rb_node *black_nil, size_t total_free_mem) {
    if (extract_tree_mem(root, black_nil) != total_free_mem) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief is_parent_valid  for duplicate node operations it is important to check the parents and
 *                         fields are updated corectly so we can continue using the tree.
 * @param *root            the root to start at for the recursive search.
 * @param *black_nil       the sentinel node at the bottom of the tree that is always black.
 * @return                 true if every parent child relationship is accurate.
 */
bool is_parent_valid(const rb_node *root, const rb_node *black_nil) {
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
    return is_parent_valid(root->links[L], black_nil) && is_parent_valid(root->links[R], black_nil);
}

/* @brief calculate_bheight_V2  verifies that the height of a red-black tree is valid. This is a
 *                              similar function to calculate_bheight but comes from a more
 *                              reliable source, because I saw results that made me doubt V1.
 * @param *root                 the root to start at for the recursive search.
 * @param *black_nil            the sentinel node at the bottom of the tree that is always black.
 * @citation                    Julienne Walker's writeup on topdown Red-Black trees has a helpful
 *                              function for verifying black heights.
 */
int calculate_bheight_V2(const rb_node *root, const rb_node *black_nil) {
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

/* @brief calculate_bheight_V2  verifies that the height of a red-black tree is valid. This is a
 *                              similar function to calculate_bheight but comes from a more
 *                              reliable source, because I saw results that made me doubt V1.
 * @param *root                 the root to start at for the recursive search.
 * @param *black_nil            the sentinel node at the bottom of the tree that is always black.
 * @citation                    Julienne Walker's writeup on topdown Red-Black trees has a helpful
 *                              function for verifying black heights.
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


/* * * * * * * * * * * * * *         Printing Functions            * * * * * * * * * * * * * * * */


/* @brief print_node  prints an individual node in its color and status as left or right child.
 * @param *root       the root we will print with the appropriate info.
 * @param *black_nil  the sentinel node at the bottom of the tree that is always black.
 * @param style       the print style: PLAIN or VERBOSE(displays memory addresses).
 */
void print_node(const rb_node *root, const rb_node *black_nil, print_style style) {
    size_t block_size = get_size(root->header);
    printf(COLOR_CYN);
    if (root->parent != black_nil) {
        root->parent->links[L] == root ? printf("L:") : printf("R:");
    }
    printf(COLOR_NIL);
    get_color(root->header) == BLACK ? printf(COLOR_BLK) : printf(COLOR_RED);
    if (style == VERBOSE) {
        printf("%p:", root);
    }
    printf("(%zubytes)", block_size);
    printf(COLOR_NIL);
    if (style == VERBOSE) {
        // print the black-height
        printf("(bh: %d)", get_black_height(root, black_nil));
    }
    printf("\n");
}

/* @brief print_inner_tree  recursively prints the contents of a red black tree with color and in
 *                          a style similar to a directory structure to be read from left to right.
 * @param *root             the root node to start at.
 * @param *black_nil        the sentinel node at the bottom of the tree that is always black.
 * @param *prefix           the string we print spacing and characters across recursive calls.
 * @param node_type         the node to print can either be a leaf or internal branch.
 * @param style             the print style: PLAIN or VERBOSE(displays memory addresses).
 */
void print_inner_tree(const rb_node *root, const rb_node *black_nil, const char *prefix,
                             const print_link node_type, print_style style) {
    if (root == black_nil) {
        return;
    }
    printf("%s", prefix);
    printf("%s", node_type == LEAF ? " └──" : " ├──");
    print_node(root, black_nil, style);

    char *str = NULL;
    int string_length = snprintf(NULL, 0, "%s%s", prefix, node_type == LEAF ? "     " : " │   ");
    if (string_length > 0) {
        str = malloc(string_length + 1);
        snprintf(str, string_length, "%s%s", prefix, node_type == LEAF ? "     " : " │   ");
    }
    if (str != NULL) {
        if (root->links[R] == black_nil) {
            print_inner_tree(root->links[L], black_nil, str, LEAF, style);
        } else if (root->links[L] == black_nil) {
            print_inner_tree(root->links[R], black_nil, str, LEAF, style);
        } else {
            print_inner_tree(root->links[R], black_nil, str, BRANCH, style);
            print_inner_tree(root->links[L], black_nil, str, LEAF, style);
        }
    } else {
        printf(COLOR_ERR "memory exceeded. Cannot display tree." COLOR_NIL);
    }
    free(str);
}

/* @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
 * @param *root          the root node to begin at for printing recursively.
 * @param *black_nil     the sentinel node at the bottom of the tree that is always black.
 * @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
 */
void print_rb_tree(const rb_node *root, const rb_node *black_nil, print_style style) {
    if (root == black_nil) {
        return;
    }
    printf(" ");
    print_node(root, black_nil, style);

    if (root->links[R] == black_nil) {
        print_inner_tree(root->links[L], black_nil, "", LEAF, style);
    } else if (root->links[L] == black_nil) {
        print_inner_tree(root->links[R], black_nil, "", LEAF, style);
    } else {
        print_inner_tree(root->links[R], black_nil, "", BRANCH, style);
        print_inner_tree(root->links[L], black_nil, "", LEAF, style);
    }
}

/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *node              a valid rb_node to a block of allocated memory.
 */
void print_alloc_block(const rb_node *node) {
    size_t block_size = get_size(node->header);
    // We will see from what direction our header is messed up by printing 16 digits.
    printf(COLOR_GRN "%p: HDR->0x%016zX(%zubytes)\n"
            COLOR_NIL, node, node->header, block_size);
}

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *header           a valid header to a block of allocated memory.
 */
void print_free_block(const rb_node *node) {
    size_t block_size = get_size(node->header);
    header *footer = (header *)((byte *)node + block_size);
    /* We should be able to see the header is the same as the footer. However, due to fixup
     * functions, the color may change for nodes and color is irrelevant to footers.
     */
    header to_print = *footer;
    if (get_size(*footer) != get_size(node->header)) {
        to_print = ULLONG_MAX;
    }
    // How far indented the Header field normally is for all blocks.
    short indent_struct_fields = PRINTER_INDENT;
    get_color(node->header) == BLACK ? printf(COLOR_BLK) : printf(COLOR_RED);
    printf("%p: HDR->0x%016zX(%zubytes)\n", node, node->header, block_size);
    printf("%*c", indent_struct_fields, ' ');

    if (node->parent) {
        printf(get_color(node->parent->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("PRN->%p\n", node->parent);
    } else {
        printf("PRN->%p\n", NULL);
    }
    printf(COLOR_NIL);
    printf("%*c", indent_struct_fields, ' ');
    if (node->links[L]) {
        printf(get_color(node->links[L]->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("LFT->%p\n", node->links[L]);
    } else {
        printf("LFT->%p\n", NULL);
    }
    printf(COLOR_NIL);
    printf("%*c", indent_struct_fields, ' ');
    if (node->links[R]) {
        printf(get_color(node->links[R]->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("RGT->%p\n", node->links[R]);
    } else {
        printf("RGT->%p\n", NULL);
    }

    /* The next and footer fields may not match the current node's color bit, and that is ok. we
     * will only worry about the next node's color when we delete a duplicate.
     */
    printf(COLOR_NIL);
    printf("%*c", indent_struct_fields, ' ');
    printf("FTR->0x%016zX\n", to_print);
}

/* @brief print_error_block  prints a helpful error message if a block is corrupted.
 * @param *header            a header to a block of memory.
 * @param block_size         the full size of a block of memory, not just the user block size.
 */
void print_error_block(const rb_node *node, size_t block_size) {
    printf("\n%p: HDR->0x%016zX->%zubyts\n",
            node, node->header, block_size);
    printf("Block size is too large and header is corrupted.\n");
}

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 * @param *root           the root node of the tree to start at for an overall heap check.
 * @param *black_nil      the sentinel node at the bottom of the tree that is always black.
 */
void print_bad_jump(const rb_node *current, const rb_node *prev,
                    const rb_node *root, const rb_node *black_nil) {
    size_t prev_size = get_size(prev->header);
    size_t cur_size = get_size(current->header);
    printf("A bad jump from the value of a header has occured. Bad distance to next header.\n");
    printf("The previous address: %p:\n", prev);
    printf("\tHeader Hex Value: %016zX:\n", prev->header);
    printf("\tBlock Byte Value: %zubytes:\n", prev_size);
    printf("\nJump by %zubytes...\n", prev_size);
    printf("The current address: %p:\n", current);
    printf("\tHeader Hex Value: 0x%016zX:\n", current->header);
    printf("\tBlock Byte Value: %zubytes:\n", cur_size);
    printf("\nJump by %zubytes...\n", cur_size);
    printf("Current state of the free tree:\n");
    print_rb_tree(root, black_nil, VERBOSE);
}

