/// File: rbtree_clrs_utilities.h
/// -----------------------------------
/// This file contains the interface for defining the types, methods, tests and
/// printing functions for the rbtree_clrs allocator. It is helpful to seperate
/// out these pieces of logic from the algorithmic portion of the allocator
/// because they can crowd the allocator file making it hard to navigate. It is
/// also convenient to refer to the design of the types for the allocator in one
/// place and use the testing and printing functions to debug any issues.
///
/// Citations:
/// -----------------------------------
///
///  1. I took much of the ideas for the pretty printing of the tree and the
/// checks for a valid tree from Seth Furman's red black tree implementation.
/// Specifically, the tree print structure and colors came from Furman's
/// implementation. https://github.com/sfurman3/red-black-tree-c
///
///  2. I took my function to verify black node paths of a red black tree from
/// kraskevich on stackoverflow in the following answer:
///          https://stackoverflow.com/questions/27731072/check-whether-a-tree-satisfies-the-black-height-property-of-red-black-tree
///
/// The header stays as the first field of the rb_node and must remain accessible
/// at all times. The size of the block is a multiple of eight to leave the
/// bottom three bits accessible for info.
///
///
///   v--Most Significnat Bit        v--Least Significnat Bit
///   0...00000    0         0       0
///   +--------+--------+--------+--------+--------+--------+--------+--------+--------+
///   |        |        |        |        |        |        |        |        | |
///   |        |red     |left    |free    |        |        |        |        | |
///   |size_t  |or      |neighbor|or      |*parent |*left   |*right  |... |footer
/// | |bytes   |black   |status  |alloc   |        |        |        |        | |
///   |        |        |        |        |        |        |        |        | |
///   +--------+--------+--------+--------+--------+--------+--------+--------+--------+
///   |___________________________________|____________________________________________|
///                     |                                     |
///               64-bit header              space available for user if
/// allocated
///
///
/// The rest of the rb_node remains accessible for the user, even the footer. We
/// only need the information in the rest of the struct when it is free in our
/// tree.
#ifndef RBTREE_CLRS_UTILITIES_H
#define RBTREE_CLRS_UTILITIES_H

#include "print_utility.h"
#include <stdbool.h>
#include <stddef.h>

/////////////////////////////          Type Definitions            //////////////////////////////////

typedef size_t header;
typedef unsigned char byte;

/// Red Black Free Tree:
///  - Maintain a red black tree of free nodes.
///  - Root is black
///  - No red node has a red child
///  - New insertions are red
///  - Every path to a non-branching node has same number of black nodes.
///  - NULL is considered black. We use a black sentinel instead.
///  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
///  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor
/// status.
typedef struct rb_node
{
    // Header stores block size, allocation status, left neighbor status, and
    // color status.
    header header;
    struct rb_node *parent;
    struct rb_node *left;
    struct rb_node *right;
    // A footer goes at end of unused blocks. Need at least 8 bytes of user space
    // to fit footer.
} rb_node;

typedef struct heap_range
{
    void *start;
    void *end;
} heap_range;

typedef struct bad_jump
{
    rb_node *prev;
    rb_node *root;
} bad_jump;

typedef struct size_total
{
    size_t size;
    size_t total;
} size_total;

typedef enum rb_color
{
    BLACK = 0,
    RED = 1
} rb_color;

#define SIZE_MASK ~0x7UL
#define MIN_BLOCK_SIZE (unsigned short)40
#define HEADERSIZE sizeof( size_t )
#define FREED 0x0UL
#define ALLOCATED 0x1UL
#define LEFT_ALLOCATED 0x2UL
#define LEFT_FREE ~0x2UL
#define COLOR_MASK 0x4UL
#define HEAP_NODE_WIDTH 32UL
#define RED_PAINT 0x4UL
#define BLK_PAINT ~0x4UL

/////////////////////////////   Basic Block and Header Operations  //////////////////////////////////

/// @brief roundup         rounds up size to the nearest multiple of two to be aligned in the heap.
/// @param requested_size  size given to us by the client.
/// @param multiple        the nearest multiple to raise our number to.
/// @return                rounded number.
static inline size_t roundup( const size_t requested_size, size_t multiple )
{
    return ( requested_size + multiple - 1 ) & ~( multiple - 1 );
}

/// @brief paint_node  flips the third least significant bit to reflect the color of the node.
/// @param *node       the node we need to paint.
/// @param color       the color the user wants to paint the node.
static inline void paint_node( rb_node *node, rb_color color )
{
    color == RED ? ( node->header |= RED_PAINT ) : ( node->header &= BLK_PAINT );
}

/// @brief get_color   returns the color of a node from the value of its header.
/// @param header_val  the value of the node in question passed by value.
/// @return            RED or BLACK
static inline rb_color get_color( header header_val ) { return ( header_val & COLOR_MASK ) == RED_PAINT; }

/// @brief get_size    returns size in bytes as a size_t from the value of node's header.
/// @param header_val  the value of the node in question passed by value.
/// @return            the size in bytes as a size_t of the node.
static inline size_t get_size( header header_val ) { return SIZE_MASK & header_val; }

/// @brief get_min     returns the smallest node in a valid binary search tree.
/// @param *root       the root of any valid binary search tree.
/// @param *black_nil  the sentinel node at the bottom of the tree that is always black.
/// @return            a pointer to the minimum node in a valid binary search tree.
static inline rb_node *get_min( rb_node *root, rb_node *black_nil )
{
    for ( ; root->left != black_nil; root = root->left ) {}
    return root;
}

/// @brief is_block_allocated  determines if a node is allocated or free.
/// @param block_header        the header value of a node passed by value.
/// @return                    true if allocated false if not.
static inline bool is_block_allocated( const header block_header ) { return block_header & ALLOCATED; }

/// @brief is_left_space  determines if the left neighbor of a block is free or allocated.
/// @param *node          the node to check.
/// @return               true if left is free false if left is allocated.
static inline bool is_left_space( const rb_node *node ) { return !( node->header & LEFT_ALLOCATED ); }

/// @brief init_header_size  initializes any node as the size and indicating left is allocated.
///                          Left is allocated because we always coalesce left and right.
/// @param *node             the region of possibly uninitialized heap we must initialize.
/// @param payload           the payload in bytes as a size_t of the current block we initialize
static inline void init_header_size( rb_node *node, size_t payload ) { node->header = LEFT_ALLOCATED | payload; }

/// @brief init_footer  initializes footer at end of the heap block to matcht the current header.
/// @param *node        the current node with a header field we will use to set the footer.
/// @param payload      the size of the current nodes free memory.
static inline void init_footer( rb_node *node, size_t payload )
{
    header *footer = (header *)( (byte *)node + payload );
    *footer = node->header;
}

/// @brief get_right_neighbor  gets the address of the next rb_node in the heap to the right.
/// @param *current            the rb_node we start at to then jump to the right.
/// @param payload             the size in bytes as a size_t of the current rb_node block.
/// @return                    the rb_node to the right of the current.
static inline rb_node *get_right_neighbor( const rb_node *current, size_t payload )
{
    return (rb_node *)( (byte *)current + HEADERSIZE + payload );
}

/// @brief *get_left_neighbor  uses the left block size gained from the footer to move to the header.
/// @param *node               the current header at which we reside.
/// @return                    a header pointer to the header for the block to the left.
static inline rb_node *get_left_neighbor( const rb_node *node )
{
    header *left_footer = (header *)( (byte *)node - HEADERSIZE );
    return (rb_node *)( (byte *)node - ( *left_footer & SIZE_MASK ) - HEADERSIZE );
}

/// @brief get_client_space  steps into the client space just after the header of a rb_node.
/// @param *node_header      the rb_node we start at before retreiving the client space.
/// @return                  the voi/// address of the client space they are now free to use.
static inline void *get_client_space( const rb_node *node_header ) { return (byte *)node_header + HEADERSIZE; }

/// @brief get_rb_node    steps to the rb_node header from the space the client was using.
/// @param *client_space  the voi/// the client was using for their type. We step to the left.
/// @return               a pointer to the rb_node of our heap block.
static inline rb_node *get_rb_node( const void *client_space )
{
    return (rb_node *)( (byte *)client_space - HEADERSIZE );
}

/////////////////////////////    Debugging and Testing Functions   //////////////////////////////////

/// @breif check_init    checks the internal representation of our heap, especially the head and
///                      tail nodes for any issues that would ruin our algorithms.
/// @param hr            start and end of the heap
/// @param heap_size     the total size in bytes of the heap.
/// @return              true if everything is in order otherwise false.
bool check_init( heap_range r, size_t heap_size );

/// @brief is_memory_balanced  loops through all blocks of memory to verify that the sizes
///                            reported match the global bookeeping in our struct.
/// @param *total_free_mem     the output parameter of the total size used as another check.
/// @param hr                  start and end of the heap
/// @param s                   size of the heap memory and total free nodes.
/// @return                    true if our tallying is correct and our totals match.
bool is_memory_balanced( size_t *total_free_mem, heap_range r, size_total s );

/// @brief is_red_red  determines if a red red violation of a red black tree has occured.
/// @param *root       the current root of the tree to begin at for checking all subtrees.
/// @param *black_nil  the sentinel node at the bottom of the tree that is always black.
/// @return            true if there is a red-red violation, false if we pass.
bool is_red_red( const rb_node *root, const rb_node *black_nil );

/// @brief is_bheight_valid  the wrapper for calculate_bheight that verifies that the black
///                          height property is upheld.
/// @param *root             the starting node of the red black tree to check.
/// @param *black_nil        the sentinel node at the bottom of the tree that is always black.
/// @return                  true if proper black height is consistently maintained throughout tree.
bool is_bheight_valid( const rb_node *root, const rb_node *black_nil );

/// @brief extract_tree_mem  sums the total memory in the red black tree to see if it matches the
///                          total memory we got from traversing blocks of the heap.
/// @param *root             the root to start at for the summing recursive search.
/// @param *black_nil        the sentinel node at the bottom of the tree that is always black.
/// @return                  the total memory in bytes as a size_t in the red black tree.
size_t extract_tree_mem( const rb_node *root, const rb_node *black_nil );

/// @brief is_rbtree_mem_valid  a wrapper for tree memory sum function used to check correctness.
/// @param *root                the root node to begin at for the recursive summing search.
/// @param *black_nil           the sentinel node at the bottom of the tree that is always black.
/// @param total_free_mem       the previously calculated free memory from a linear heap search.
/// @return                     true if the totals match false if they do not.
bool is_rbtree_mem_valid( const rb_node *root, const rb_node *black_nil, size_t total_free_mem );

/// @brief is_parent_valid  for duplicate node operations it is important to check the parents
///                         and fields are updated corectly so we can continue using the tree.
/// @param *root            the root to start at for the recursive search.
/// @param *black_nil       the sentinel node at the bottom of the tree that is always black.
/// @return                 true if every parent child relationship is accurate.
bool is_parent_valid( const rb_node *root, const rb_node *black_nil );

/// @brief is_bheight_valid_V2  the wrapper for calculate_bheight_V2 that verifies that the
///                             black height property is upheld.
/// @param *root                the starting node of the red black tree to check.
/// @param *black_nil            the sentinel node at the bottom of the tree that is always black.
/// @return                     true if the paths are valid, false if not.
bool is_bheight_valid_v2( const rb_node *root, const rb_node *black_nil );

/// @brief is_binary_tree  confirms the validity of a binary search tree. Nodes to the left
///                        should be less than the root and nodes to the right should be greater.
/// @param *root           the root of the tree from which we examine children.
/// @param *black_nil      the sentinel node at the bottom of the tree that is always black.
/// @return                true if the tree is valid, false if not.
bool is_binary_tree( const rb_node *root, const rb_node *black_nil );

/////////////////////////////        Printing Functions            //////////////////////////////////

/// @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
/// @param *root          the root node to begin at for printing recursively.
/// @param *black_nil     the sentinel node at the bottom of the tree that is always black.
/// @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
void print_rb_tree( const rb_node *root, const rb_node *black_nil, print_style style );

/// @brief print_all    prints our the complete status of the heap, all of its blocks, and
///                     the sizes the blocks occupy. Printing should be clean with no overlap of
///                     unique id's between heap blocks or corrupted headers.
/// @param hr           start and end of the heap
/// @param heap_size    the size in bytes of the heap.
/// @param *root        the root of the tree we start at for printing.
/// @param *black_nil   the sentinel node that waits at the bottom of the tree for all leaves.
void print_all( heap_range r, size_t heap_size, rb_node *root, rb_node *black_nil );

#endif