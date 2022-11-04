/**
 * File: rbtree_unified_design.h
 * ---------------------------------
 * This file contains the interface that defines our custom types for the rbtree_unified
 * allocator. It also contains useful methods for these types.
 *
 * Across these heap utility libraries you may see code that appears almost identical to a utility
 * library for another allocator. While it may be tempting to think we could unite the common logic
 * of these methods to one utility library, I think this is a bad idea. There are subtle
 * differences between each allocator's types and block organization that makes keeping the logic
 * seperate easier and cleaner. This way, I can come back and adjust an existing allocator
 * exactly as needed. I can also add new allocators creatively, by simply adding a new utility
 * library to define its fundamentals, without worrying about fitting in to types and methods
 * previously established by other allocators.
 */
#ifndef RBTREE_UNIFIED_DESIGN_H
#define RBTREE_UNIFIED_DESIGN_H

#include <stddef.h>
#include <stdbool.h>
#include "print_utility.h"

#define TWO_NODE_ARRAY (unsigned short)2
#define SIZE_MASK ~0x7UL
#define COLOR_MASK 0x4UL
#define HEAP_NODE_WIDTH (unsigned short)32
#define MIN_BLOCK_SIZE (unsigned short)40
#define HEADERSIZE sizeof(size_t)

typedef size_t header;
typedef unsigned char byte;

/* Red Black Free Tree:
 *  - Maintain a red black tree of free nodes.
 *  - Root is black
 *  - No red node has a red child
 *  - New insertions are red
 *  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
 *  - Every path from root to tree.black_nil, root not included, has same number of black nodes.
 *  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
 *  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status for coalescing.
 */
typedef struct rb_node {
    // The header will store block size, allocation status, left neighbor status, and node color.
    header header;
    struct rb_node *parent;
    struct rb_node *links[TWO_NODE_ARRAY];
    // A footer goes at end of unused blocks. Need at least 8 bytes of user space to fit footer.
}rb_node;

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

typedef enum header_status {
    FREE = 0x0UL,
    ALLOCATED = 0x1UL,
    LEFT_ALLOCATED = 0x2UL,
    RED_PAINT = 0x4UL,
    BLK_PAINT = ~0x4UL,
    LEFT_FREE = ~0x2UL
} header_status;


/* * * * * * * * * * * * * *    Basic Block and Header Operations  * * * * * * * * * * * * * * * */


/* @brief roundup         rounds up size to the nearest multiple of two to be aligned in the heap.
 * @param requested_size  size given to us by the client.
 * @param multiple        the nearest multiple to raise our number to.
 * @return                rounded number.
 */
inline size_t roundup(size_t requested_size, size_t multiple) {
    return (requested_size + multiple - 1) & ~(multiple - 1);
}

/* @brief paint_node  flips the third least significant bit to reflect the color of the node.
 * @param *node       the node we need to paint.
 * @param color       the color the user wants to paint the node.
 */
inline void paint_node(rb_node *node, rb_color color) {
    color == RED ? (node->header |= RED_PAINT) : (node->header &= BLK_PAINT);
}

/* @brief get_color   returns the color of a node from the value of its header.
 * @param header_val  the value of the node in question passed by value.
 * @return            RED or BLACK
 */
inline rb_color get_color(header header_val) {
    return (header_val & COLOR_MASK) == RED_PAINT;
}

/* @brief get_size    returns size in bytes as a size_t from the value of node's header.
 * @param header_val  the value of the node in question passed by value.
 * @return            the size in bytes as a size_t of the node.
 */
inline size_t get_size(header header_val) {
    return SIZE_MASK & header_val;
}

/* @brief get_min     returns the smallest node in a valid binary search tree.
 * @param *root       the root of any valid binary search tree.
 * @param *black_nil  the sentinel node at the bottom of the tree that is always black.
 * @return            a pointer to the minimum node in a valid binary search tree.
 */
inline rb_node *get_min(rb_node *root, rb_node *black_nil) {
    for (; root->links[L] != black_nil; root = root->links[L]) {
    }
    return root;
}

/* @brief is_block_allocated  determines if a node is allocated or free.
 * @param block_header        the header value of a node passed by value.
 * @return                    true if allocated false if not.
 */
inline bool is_block_allocated(const header block_header) {
    return block_header & ALLOCATED;
}

/* @brief is_left_space  determines if the left neighbor of a block is free or allocated.
 * @param *node          the node to check.
 * @return               true if left is free false if left is allocated.
 */
inline bool is_left_space(const rb_node *node) {
    return !(node->header & LEFT_ALLOCATED);
}

/* @brief init_header_size  initializes any node as the size and indicating left is allocated. Left
 *                          is allocated because we always coalesce left and right.
 * @param *node             the region of possibly uninitialized heap we must initialize.
 * @param payload           the payload in bytes as a size_t of the current block we initialize
 */
inline void init_header_size(rb_node *node, size_t payload) {
    node->header = LEFT_ALLOCATED | payload;
}

/* @brief init_footer  initializes footer at end of the heap block to matcht the current header.
 * @param *node        the current node with a header field we will use to set the footer.
 * @param payload      the size of the current nodes free memory.
 */
inline void init_footer(rb_node *node, size_t payload) {
    header *footer = (header *)((byte *)node + payload);
    *footer = node->header;
}

/* @brief get_right_neighbor  gets the address of the next rb_node in the heap to the right.
 * @param *current            the rb_node we start at to then jump to the right.
 * @param payload             the size in bytes as a size_t of the current rb_node block.
 * @return                    the rb_node to the right of the current.
 */
inline rb_node *get_right_neighbor(const rb_node *current, size_t payload) {
    return (rb_node *)((byte *)current + HEADERSIZE + payload);
}

/* @brief *get_left_neighbor  uses the left block size gained from the footer to move to the header.
 * @param *node               the current header at which we reside.
 * @return                    a header pointer to the header for the block to the left.
 */
inline rb_node *get_left_neighbor(const rb_node *node) {
    header *left_footer = (header *)((byte *)node - HEADERSIZE);
    return (rb_node *)((byte *)node - (*left_footer & SIZE_MASK) - HEADERSIZE);
}


/* @brief get_client_space  steps into the client space just after the header of a rb_node.
 * @param *node_header      the rb_node we start at before retreiving the client space.
 * @return                  the void* address of the client space they are now free to use.
 */
inline void *get_client_space(const rb_node *node_header) {
    return (byte *) node_header + HEADERSIZE;
}

/* @brief get_rb_node    steps to the rb_node header from the space the client was using.
 * @param *client_space  the void* the client was using for their type. We step to the left.
 * @return               a pointer to the rb_node of our heap block.
 */
inline rb_node *get_rb_node(const void *client_space) {
    return (rb_node *)((byte *) client_space - HEADERSIZE);
}


#endif
