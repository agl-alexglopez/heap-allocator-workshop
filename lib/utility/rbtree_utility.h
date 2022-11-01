
#ifndef _RBTREE_UTILITY_H_
#define _RBTREE_UTILITY_H_
#include <stdbool.h>
#include <stddef.h>

/* Text coloring macros (ANSI character escapes) for printing function colorful output. Consider
 * changing to a more portable library like ncurses.h. However, I don't want others to install
 * ncurses just to explore the project. They already must install gnuplot. Hope this works.
 */
#define COLOR_BLK "\033[34;1m"
#define COLOR_RED "\033[31;1m"
#define COLOR_CYN "\033[36;1m"
#define COLOR_GRN "\033[32;1m"
#define COLOR_NIL "\033[0m"
#define COLOR_ERR COLOR_RED "Error: " COLOR_NIL
#define PRINTER_INDENT (short)13
#define TWO_NODE_ARRAY (unsigned short)2
#define COLOR_MASK 0x4UL
// Most implementations use this node size for the tree and blocks.
#define STD_NODE_WIDTH (unsigned short)32
#define STD_BLOCK_SIZE (unsigned short)40
// The rbtree_linked implementation is the only outlier with larger block size.
#define LRG_NODE_WIDTH (unsigned short)40
#define LRG_BLOCK_SIZE (unsigned short)48
#define HEADERSIZE sizeof(size_t)
// The rbtree_stack implementation needs a limit on the stack. Balanced tree, this can be small.
#define MAX_TREE_STACK_HEIGHT (unsigned short)50
#define SIZE_MASK ~0x7UL
#define COLOR_MASK 0x4UL


/* * * * * * * * * * * * * *  Type Definitions   * * * * * * * * * * * * * * * * */


typedef size_t header;
typedef unsigned char byte;

// PLAIN prints free block sizes, VERBOSE shows address in the heap and black height of tree.
typedef enum print_style {
    PLAIN = 0,
    VERBOSE = 1
}print_style;

// Printing enum for printing red black tree structure.
typedef enum print_link {
    BRANCH = 0, // ├──
    LEAF = 1    // └──
}print_link;

typedef enum rb_node_width {
    STD = 32,
    LRG = 40
}rb_node_width;

/* All allocators use bits in the header to track information. The RED_PAINT BLK_PAINT status masks
 * are only used by the red black tree allocators, but other masks are identical across allocators.
 */
typedef enum header_status {
    FREE = 0x0UL,
    ALLOCATED = 0x1UL,
    LEFT_ALLOCATED = 0x2UL,
    RED_PAINT = 0x4UL,
    BLK_PAINT = ~0x4UL,
    LEFT_FREE = ~0x2UL
} header_status;

typedef enum rb_color {
    BLACK = 0,
    RED = 1
}rb_color;

// NOT(!) operator will flip this enum to the opposite field. !L == R and !R == L;
typedef enum tree_link {
    // (L == LEFT), (R == RIGHT)
    L = 0,
    R = 1
} tree_link;

// When you see these, know that we are working with a doubly linked list, not a tree.
typedef enum list_link {
    // (P == PREVIOUS), (N == NEXT)
    P = 0,
    N = 1
} list_link;

/* All nodes below help maintain the rules of a Red Black Tree:
 *  - Maintain a red black tree of free nodes.
 *  - Root is black
 *  - No red node has a red child
 *  - New insertions are red
 *  - Every path to a non-branching node has same number of black nodes.
 *  - NULL is considered black. We use a black sentinel instead.
 *  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
 *  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status.
 */


// CLRS based implementation. Standard Red Black tree.
typedef struct rb_clrs_node {
    // Header stores block size, allocation status, left neighbor status, and color status.
    header header;
    struct rb_clrs_node *parent;
    struct rb_clrs_node *left;
    struct rb_clrs_node *right;
    // A footer goes at end of unused blocks. Need at least 8 bytes of user space to fit footer.
}rb_clrs_node;


// Unified implementation that unites left and right cases.
typedef struct rb_unif_node {
    // The header will store block size, allocation status, left neighbor status, and node color.
    header header;
    struct rb_unif_node *parent;
    struct rb_unif_node *links[TWO_NODE_ARRAY];
    // A footer goes at end of unused blocks. Need at least 8 bytes of user space to fit footer.
}rb_unif_node;


// Linked implementation focussed on speed. Tracks duplicates and parent.
typedef struct rb_link_node {
    // block size, allocation status, left neighbor status, and node color.
    header header;
    struct rb_link_node *parent;
    struct rb_link_node *links[TWO_NODE_ARRAY];
    // Points to a list which we will use P and N to manage to distinguish from the tree.
    struct duplicate_link_node *list_start;
}rb_link_node;

typedef struct duplicate_link_node {
    header header;
    struct rb_link_node *parent;
    struct duplicate_link_node *links[TWO_NODE_ARRAY];
    struct rb_link_node *list_start;
}duplicate_link_node;


/* My custom rb node to fit the needs of space and speed. Tracks duplicates and does not use a
 * parent field. This node is used by the stack and topdown based allocators.
 */
typedef struct rb_cstm_node {
    // The header will store block size, allocation status, left neighbor status, and node color.
    header header;
    struct rb_cstm_node *links[TWO_NODE_ARRAY];
    // Use list_start to maintain doubly linked duplicates, using the links[P]-links[N] fields
    struct duplicate_cstm_node *list_start;
}rb_cstm_node;

typedef struct duplicate_cstm_node {
    header header;
    struct duplicate_cstm_node *links[TWO_NODE_ARRAY];
    // We will always store the tree parent in first duplicate node in the list. O(1) coalescing.
    struct rb_cstm_node *parent;
} duplicate_cstm_node;


/* * * * * * * * * * * * * *  Minor Helper Functions   * * * * * * * * * * * * * * * * */

/* @brief paint_node  flips the third least significant bit to reflect the color of the node.
 * @param *node       the node we need to paint.
 * @param color       the color the user wants to paint the node.
 */
void paint_node(void *node, rb_color color);

/* @brief get_color   returns the color of a node from the value of its header.
 * @param header_val  the value of the node in question passed by value.
 * @return            RED or BLACK
 */
rb_color get_color(header header_val);

/* @brief get_size    returns size in bytes as a size_t from the value of node's header.
 * @param header_val  the value of the node in question passed by value.
 * @return            the size in bytes as a size_t of the node.
 */
size_t get_size(header header_val);


#endif
