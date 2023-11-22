/// Author: Alexander Griffin Lopez
/// File: rbtree_clrs.c
/// ---------------------
///  This file contains my implementation of an explicit heap allocator. This allocator uses a tree
///  implementation to track the free space in the heap, relying on the properties of a red-black
///  tree to remain balanced.
///  Citations:
///  -------------------
///  1. Bryant and O'Hallaron, Computer Systems: A Programmer's Perspective, Chapter 9.
///     I used the explicit free list outline from the textbook, specifically
///     regarding how to implement left and right coalescing. I even used their suggested
///     optimization of an extra control bit so that the footers to the left can be overwritten
///     if the block is allocated so the user can have more space.
///  2. The text Introduction to Algorithms, by Cormen, Leiserson, Rivest, and Stein was central to
///     my implementation of the red black tree portion of my allocator. Specifically, I took the
///     the implementation from chapter 13. The placeholder black null node that always sits at the
///     bottom of the tree proved useful for simplicity.
#include "allocator.h"
#include "debug_break.h"
#include "print_utility.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NOLINTBEGIN(*-avoid-non-const-global-variables)

/////////////////////////////          Type Definitions            //////////////////////////////////

typedef size_t header;

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
struct rb_node
{
    // Header stores block size, allocation status, left neighbor status, and color status.
    header header;
    struct rb_node *parent;
    struct rb_node *left;
    struct rb_node *right;
    // A footer goes at end of unused blocks. Need at least 8 bytes of user space
    // to fit footer.
};

struct heap_range
{
    void *start;
    void *end;
};

struct bad_jump
{
    struct rb_node *prev;
    struct rb_node *root;
};

struct size_total
{
    size_t byte_size;
    size_t count_total;
};

enum rb_color
{
    BLACK = 0,
    RED = 1
};

#define SIZE_MASK ~0x7UL
#define MIN_BLOCK_SIZE (uint8_t)40
#define HEADERSIZE sizeof( size_t )
#define FREED 0x0UL
#define ALLOCATED 0x1UL
#define LEFT_ALLOCATED 0x2UL
#define LEFT_FREE ~0x2UL
#define COLOR_MASK 0x4UL
#define HEAP_NODE_WIDTH 32UL
#define RED_PAINT 0x4UL
#define BLK_PAINT ~0x4UL

//////////////////////////////  Static Heap Tracking  ////////////////////////////////

/// Red Black Free Tree:
///  - Maintain a red black tree of free nodes.
///  - Root is black
///  - No red node has a red child
///  - New insertions are red
///  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
///  - Every path from root to tree.black_nil, root not included, has same number of black nodes.
///  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
///  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status for coalescing.
///  - For more details, see the _utilities.h file.
static struct tree
{
    struct rb_node *root;
    struct rb_node *black_nil;
    size_t total;
} tree;

static struct heap
{
    void *client_start;
    void *client_end;
    size_t heap_size;
} heap;

// NOLINTEND(*-avoid-non-const-global-variables)

/////////////////////////////   Forward Declarations  //////////////////////////////////

// There are a ton of helper function so forward declare so we can put them at the end of file.

static struct rb_node *coalesce( struct rb_node *leftmost_node );
static void init_free_node( struct rb_node *to_free, size_t block_size );
static void *split_alloc( struct rb_node *free_block, size_t request, size_t block_space );
static void left_rotate( struct rb_node *current );
static void right_rotate( struct rb_node *current );
static void fix_rb_insert( struct rb_node *current );
static void insert_rb_node( struct rb_node *current );
static void rb_transplant( const struct rb_node *remove, struct rb_node *replacement );
static void fix_rb_delete( struct rb_node *extra_black );
static struct rb_node *delete_rb_node( struct rb_node *remove );
static struct rb_node *find_best_fit( size_t key );
static size_t roundup( size_t requested_size, size_t multiple );
static void paint_node( struct rb_node *node, enum rb_color color );
static enum rb_color get_color( header header_val );
static size_t get_size( header header_val );
static struct rb_node *get_min( struct rb_node *root, struct rb_node *black_nil );
static bool is_block_allocated( header block_header );
static bool is_left_space( const struct rb_node *node );
static void init_header_size( struct rb_node *node, size_t payload );
static void init_footer( struct rb_node *node, size_t payload );
static struct rb_node *get_right_neighbor( const struct rb_node *current, size_t payload );
static struct rb_node *get_left_neighbor( const struct rb_node *node );
static void *get_client_space( const struct rb_node *node_header );
static struct rb_node *get_rb_node( const void *client_space );
static bool check_init( struct heap_range r, size_t heap_size );
static bool is_memory_balanced( size_t *total_free_mem, struct heap_range r, struct size_total s );
static bool is_red_red( const struct rb_node *root, const struct rb_node *black_nil );
static bool is_bheight_valid( const struct rb_node *root, const struct rb_node *black_nil );
static size_t extract_tree_mem( const struct rb_node *root, const struct rb_node *black_nil );
static bool is_rbtree_mem_valid( const struct rb_node *root, const struct rb_node *black_nil,
                                 size_t total_free_mem );
static bool is_parent_valid( const struct rb_node *root, const struct rb_node *black_nil );
static bool is_bheight_valid_v2( const struct rb_node *root, const struct rb_node *black_nil );
static bool is_binary_tree( const struct rb_node *root, const struct rb_node *black_nil );
static void print_rb_tree( const struct rb_node *root, const struct rb_node *black_nil, enum print_style style );
static void print_all( struct heap_range r, size_t heap_size, struct rb_node *root, struct rb_node *black_nil );

/////////////////////////////   Shared Heap Functions    ///////////////////////////////////////

size_t get_free_total( void ) { return tree.total; }

bool myinit( void *heap_start, size_t heap_size )
{
    // Initialize the root of the tree and heap addresses.
    size_t client_request = roundup( heap_size, ALIGNMENT );
    if ( client_request < MIN_BLOCK_SIZE ) {
        return false;
    }
    heap.client_start = heap_start;
    heap.heap_size = client_request;
    heap.client_end = (uint8_t *)heap.client_start + heap.heap_size - HEAP_NODE_WIDTH;
    // Set up the dummy base of the tree to which all leaves will point.
    tree.black_nil = heap.client_end;
    tree.black_nil->header = 1UL;
    tree.black_nil->parent = NULL;
    tree.black_nil->left = NULL;
    tree.black_nil->right = NULL;
    paint_node( tree.black_nil, BLACK );
    // Set up the root of the tree (top) that starts as our largest free block.
    tree.root = heap.client_start;
    init_header_size( tree.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    paint_node( tree.root, BLACK );
    init_footer( tree.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    tree.root->parent = tree.black_nil;
    tree.root->left = tree.black_nil;
    tree.root->right = tree.black_nil;
    tree.total = 1;
    return true;
}

void *mymalloc( size_t requested_size )
{
    if ( requested_size == 0 || requested_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    size_t client_request = roundup( requested_size + HEAP_NODE_WIDTH, ALIGNMENT );
    struct rb_node *found_node = find_best_fit( client_request );
    return split_alloc( found_node, client_request, get_size( found_node->header ) );
}

void *myrealloc( void *old_ptr, size_t new_size )
{
    if ( new_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    if ( old_ptr == NULL ) {
        return mymalloc( new_size );
    }
    if ( new_size == 0 ) {
        myfree( old_ptr );
        return NULL;
    }
    size_t request = roundup( new_size + HEAP_NODE_WIDTH, ALIGNMENT );
    struct rb_node *old_node = get_rb_node( old_ptr );
    size_t old_size = get_size( old_node->header );

    struct rb_node *leftmost_node = coalesce( old_node );
    size_t coalesced_space = get_size( leftmost_node->header );
    void *client_space = get_client_space( leftmost_node );

    if ( coalesced_space >= request ) {
        if ( leftmost_node != old_node ) {
            memmove( client_space, old_ptr, old_size ); // NOLINT(*DeprecatedOrUnsafeBufferHandling)
        }
        return split_alloc( leftmost_node, request, coalesced_space );
    }
    client_space = mymalloc( request );
    if ( client_space ) {
        memcpy( client_space, old_ptr, old_size ); // NOLINT(*DeprecatedOrUnsafeBufferHandling)
        init_free_node( leftmost_node, coalesced_space );
    }
    return client_space;
}

void myfree( void *ptr )
{
    if ( NULL == ptr ) {
        return;
    }
    struct rb_node *to_insert = get_rb_node( ptr );
    to_insert = coalesce( to_insert );
    init_free_node( to_insert, get_size( to_insert->header ) );
}

/////////////////////////     Shared Debugger       //////////////////////////////////////////

bool validate_heap( void )
{
    if ( !check_init( ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size ) ) {
        return false;
    }
    size_t total_free_mem = 0;
    if ( !is_memory_balanced( &total_free_mem, ( struct heap_range ){ heap.client_start, heap.client_end },
                              ( struct size_total ){ heap.heap_size, tree.total } ) ) {
        return false;
    }
    // Does a tree search for all memory match the linear heap search for totals?
    if ( !is_rbtree_mem_valid( tree.root, tree.black_nil, total_free_mem ) ) {
        return false;
    }
    // Two red nodes in a row are invalid for the tree.
    if ( is_red_red( tree.root, tree.black_nil ) ) {
        return false;
    }
    // Does every path from a node to the black sentinel contain the same number of black nodes.
    if ( !is_bheight_valid( tree.root, tree.black_nil ) ) {
        return false;
    }
    // Check that the parents and children are updated correctly if duplicates are deleted.
    if ( !is_parent_valid( tree.root, tree.black_nil ) ) {
        return false;
    }
    // This comes from a more official write up on red black trees so I included it.
    if ( !is_bheight_valid_v2( tree.root, tree.black_nil ) ) {
        return false;
    }
    if ( !is_binary_tree( tree.root, tree.black_nil ) ) {
        return false;
    }
    return true;
}

/////////////////////////     Shared Printer    /////////////////////////////////////////

void print_free_nodes( enum print_style style )
{
    printf( "\n" );
    print_rb_tree( tree.root, tree.black_nil, style );
}

void dump_heap( void )
{
    print_all( ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size, tree.root,
               tree.black_nil );
}

///////////////////////    Static Heap Helper Functions   /////////////////////////////////

/// @brief coalesce        attempts to coalesce left and right if the left and right rb_node
///                        are free. Runs the search to free the specific free node in O(logN) + d
///                        where d is the number of duplicate nodes of the same size.
/// @param *leftmost_node  the current node that will move left if left is free to coalesce.
/// @return                the leftmost node from attempts to coalesce left and right. The leftmost
///                        node is initialized to reflect the correct size for the space it now has.
/// @warning               this function does not overwrite the data that may be in the middle if we
///                        expand left and write. The user may wish to move elsewhere if reallocing.
static struct rb_node *coalesce( struct rb_node *leftmost_node )
{
    size_t coalesced_space = get_size( leftmost_node->header );
    struct rb_node *rightmost_node = get_right_neighbor( leftmost_node, coalesced_space );

    // The black_nil is the right boundary. We set it to always be allocated with size 0.
    if ( !is_block_allocated( rightmost_node->header ) ) {
        coalesced_space += get_size( rightmost_node->header ) + HEADERSIZE;
        (void)delete_rb_node( rightmost_node );
    }
    // We use our static struct for convenience here to tell where our segment start is.
    if ( leftmost_node != heap.client_start && is_left_space( leftmost_node ) ) {
        leftmost_node = get_left_neighbor( leftmost_node );
        coalesced_space += get_size( leftmost_node->header ) + HEADERSIZE;
        leftmost_node = delete_rb_node( leftmost_node );
    }

    // Do not initialize footer here because we may coalesce during realloc. Preserve user data.
    init_header_size( leftmost_node, coalesced_space );
    return leftmost_node;
}

/// @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
/// @param *to_free        the heap_node to add to the red black tree.
/// @param block_size      the size we use to initialize the node and find the right place in tree.
static void init_free_node( struct rb_node *to_free, size_t block_size )
{
    to_free->header = block_size | LEFT_ALLOCATED | RED_PAINT;
    init_footer( to_free, block_size );
    get_right_neighbor( to_free, block_size )->header &= LEFT_FREE;
    insert_rb_node( to_free );
}

/// @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
///                      split, it will add the newly freed split block to the free red black tree.
/// @param *free_block   a pointer to the node for a free block in its entirety.
/// @param request       the user request for space.
/// @param block_space   the entire space that we have to work with.
/// @return              a void pointer to generic space that is now ready for the client.
static void *split_alloc( struct rb_node *free_block, size_t request, size_t block_space )
{
    if ( block_space >= request + MIN_BLOCK_SIZE ) {
        // This takes care of the neighbor and ITS neighbor with appropriate updates.
        init_free_node( get_right_neighbor( free_block, request ), block_space - request - HEADERSIZE );
        init_header_size( free_block, request );
        free_block->header |= ALLOCATED;
        return get_client_space( free_block );
    }
    get_right_neighbor( free_block, block_space )->header |= LEFT_ALLOCATED;
    init_header_size( free_block, block_space );
    free_block->header |= ALLOCATED;
    return get_client_space( free_block );
}

////////////////////////   Red Black Tree Implementation   /////////////////////////////

/// @brief left_rotate  complete a left rotation to help repair a red-black tree. Assumes current is
///                     not the tree.black_nil and that the right child is not black sentinel.
/// @param *current     current will move down the tree, it's right child will move up to replace.
/// @warning            this function assumes current and current->right are not tree.black_nil.
static void left_rotate( struct rb_node *current )
{
    struct rb_node *right_child = current->right;
    current->right = right_child->left;
    if ( right_child->left != tree.black_nil ) {
        right_child->left->parent = current;
    }
    right_child->parent = current->parent;
    if ( current->parent == tree.black_nil ) {
        tree.root = right_child;
    } else if ( current == current->parent->left ) {
        current->parent->left = right_child;
    } else {
        current->parent->right = right_child;
    }
    right_child->left = current;
    current->parent = right_child;
}

/// @brief right_rotate  completes a right rotation to help repair a red-black tree. Assumes current
///                      is not the tree.black_nil and the right child is not tree.black_nil.
/// @param *current      the current node moves down the tree and the left child moves up.
/// @warning             this function assumes current and current->right are not tree.black_nil.
static void right_rotate( struct rb_node *current )
{
    struct rb_node *left_child = current->left;
    current->left = left_child->right;
    if ( left_child->right != tree.black_nil ) {
        left_child->right->parent = current;
    }
    left_child->parent = current->parent;
    if ( current->parent == tree.black_nil ) {
        tree.root = left_child;
    } else if ( current == current->parent->right ) {
        current->parent->right = left_child;
    } else {
        current->parent->left = left_child;
    }
    left_child->right = current;
    current->parent = left_child;
}

///     Static Red-Black Tree Insertion Logic

/// @brief fix_rb_insert  implements Cormen et.al. red black fixup after the insertion of a node.
///                       Ensures that the rules of a red-black tree are upheld after insertion.
/// @param *current       the current node that has just been added to the red black tree.
static void fix_rb_insert( struct rb_node *current )
{
    while ( get_color( current->parent->header ) == RED ) {
        if ( current->parent == current->parent->parent->left ) {
            struct rb_node *uncle = current->parent->parent->right;
            if ( get_color( uncle->header ) == RED ) {
                paint_node( current->parent, BLACK );
                paint_node( uncle, BLACK );
                paint_node( current->parent->parent, RED );
                current = current->parent->parent;
            } else { // uncle is BLACK
                if ( current == current->parent->right ) {
                    current = current->parent;
                    left_rotate( current );
                }
                paint_node( current->parent, BLACK );
                paint_node( current->parent->parent, RED );
                right_rotate( current->parent->parent );
            }
        } else {
            struct rb_node *uncle = current->parent->parent->left;
            if ( get_color( uncle->header ) == RED ) {
                paint_node( current->parent, BLACK );
                paint_node( uncle, BLACK );
                paint_node( current->parent->parent, RED );
                current = current->parent->parent;
            } else { // uncle is BLACK
                if ( current == current->parent->left ) {
                    current = current->parent;
                    right_rotate( current );
                }
                paint_node( current->parent, BLACK );
                paint_node( current->parent->parent, RED );
                left_rotate( current->parent->parent );
            }
        }
    }
    paint_node( tree.root, BLACK );
}

/// @brief insert_rb_node  a simple binary tree insert with additional red black fixup logic.
/// @param *current        we must insert to tree or add to a list as duplicate.
static void insert_rb_node( struct rb_node *current )
{
    struct rb_node *child = tree.root;
    struct rb_node *parent = tree.black_nil;
    size_t current_key = get_size( current->header );
    while ( child != tree.black_nil ) {
        parent = child;
        size_t child_size = get_size( child->header );
        if ( current_key < child_size ) {
            child = child->left;
        } else {
            child = child->right;
        }
    }
    current->parent = parent;
    if ( parent == tree.black_nil ) {
        tree.root = current;
    } else if ( current_key < get_size( parent->header ) ) {
        parent->left = current;
    } else {
        parent->right = current;
    }
    current->left = tree.black_nil;
    current->right = tree.black_nil;
    paint_node( current, RED );
    fix_rb_insert( current );
    tree.total++;
}

/// @brief rb_transplant  replaces node with the appropriate node to start balancing the tree.
/// @param *remove        the node we are removing from the tree.
/// @param *replacement   the node that will fill the remove position. It can be tree.black_nil.
static void rb_transplant( const struct rb_node *remove, struct rb_node *replacement )
{
    if ( remove->parent == tree.black_nil ) {
        tree.root = replacement;
    } else if ( remove == remove->parent->left ) {
        remove->parent->left = replacement;
    } else {
        remove->parent->right = replacement;
    }
    replacement->parent = remove->parent;
}

/// @brief fix_rb_delete  completes repairs on a red black tree to ensure rules hold and balance.
/// @param *extra_black   the current node that was moved into place from the previous delete. It
///                       holds an extra "black" it must get rid of by either pointing to a red node
///                       or reaching the root. In either case it is then painted singly black.
static void fix_rb_delete( struct rb_node *extra_black )
{
    // If we enter the loop extra_black points to a black node making it "doubly black".
    while ( extra_black != tree.root && get_color( extra_black->header ) == BLACK ) {
        if ( extra_black == extra_black->parent->left ) {
            struct rb_node *right_sibling = extra_black->parent->right;
            if ( get_color( right_sibling->header ) == RED ) {
                paint_node( right_sibling, BLACK );
                paint_node( extra_black->parent, RED );
                left_rotate( extra_black->parent );
                right_sibling = extra_black->parent->right;
            }
            // The previous left rotation may have made the right sibling black null.
            if ( get_color( right_sibling->left->header ) == BLACK
                 && get_color( right_sibling->right->header ) == BLACK ) {
                paint_node( right_sibling, RED );
                extra_black = extra_black->parent;
            } else {
                if ( get_color( right_sibling->right->header ) == BLACK ) {
                    paint_node( right_sibling->left, BLACK );
                    paint_node( right_sibling, RED );
                    right_rotate( right_sibling );
                    right_sibling = extra_black->parent->right;
                }
                paint_node( right_sibling, get_color( extra_black->parent->header ) );
                paint_node( extra_black->parent, BLACK );
                paint_node( right_sibling->right, BLACK );
                left_rotate( extra_black->parent );
                extra_black = tree.root;
            }
            continue;
        }
        // This is a symmetric case, so it is identical with left and right switched.
        struct rb_node *left_sibling = extra_black->parent->left;
        if ( get_color( left_sibling->header ) == RED ) {
            paint_node( left_sibling, BLACK );
            paint_node( extra_black->parent, RED );
            right_rotate( extra_black->parent );
            left_sibling = extra_black->parent->left;
        }
        // The previous left rotation may have made the right sibling black null.
        if ( get_color( left_sibling->right->header ) == BLACK
             && get_color( left_sibling->left->header ) == BLACK ) {
            paint_node( left_sibling, RED );
            extra_black = extra_black->parent;
            continue;
        }
        if ( get_color( left_sibling->left->header ) == BLACK ) {
            paint_node( left_sibling->right, BLACK );
            paint_node( left_sibling, RED );
            left_rotate( left_sibling );
            left_sibling = extra_black->parent->left;
        }
        paint_node( left_sibling, get_color( extra_black->parent->header ) );
        paint_node( extra_black->parent, BLACK );
        paint_node( left_sibling->left, BLACK );
        right_rotate( extra_black->parent );
        extra_black = tree.root;
    }
    // Node has either become "red-and-black" by pointing to a red node or is root. Paint black.
    paint_node( extra_black, BLACK );
}

/// @brief delete_rb_node  performs the necessary steps to have a functional, balanced tree after
///                        deletion of any node in the tree.
/// @param *remove         the node to remove from the tree from a call to malloc or coalesce.
static struct rb_node *delete_rb_node( struct rb_node *remove )
{
    enum rb_color fixup_color_check = get_color( remove->header );

    // We will give the replacement of the replacement an "extra" black color.
    struct rb_node *extra_black = NULL;
    if ( remove->left == tree.black_nil ) {
        rb_transplant( remove, ( extra_black = remove->right ) );
    } else if ( remove->right == tree.black_nil ) {
        rb_transplant( remove, ( extra_black = remove->left ) );
    } else {
        // The node to remove is internal with two children of unkown size subtrees.
        struct rb_node *right_min = get_min( remove->right, tree.black_nil );
        fixup_color_check = get_color( right_min->header );

        // Possible this is black_nil and that's ok.
        extra_black = right_min->right;
        if ( right_min != remove->right ) {
            rb_transplant( right_min, right_min->right );
            right_min->right = remove->right;
            right_min->right->parent = right_min;
        } else {
            extra_black->parent = right_min;
        }
        rb_transplant( remove, right_min );
        right_min->left = remove->left;
        right_min->left->parent = right_min;
        paint_node( right_min, get_color( remove->header ) );
    }
    // Nodes can only be red or black, so we need to get rid of "extra" black by fixing tree.
    if ( fixup_color_check == BLACK ) {
        fix_rb_delete( extra_black );
    }
    tree.total--;
    return remove;
}

/// @brief find_best_fit  a red black tree is well suited to best fit search in O(logN) time. We
///                       will find the best fitting node possible given the options in our tree.
/// @param key            the size_t number of bytes we are searching for in our tree.
/// @return               the pointer to the rb_node that is the best fit for our need.
static struct rb_node *find_best_fit( size_t key )
{
    struct rb_node *seeker = tree.root;
    size_t best_fit_size = ULLONG_MAX;
    struct rb_node *remove = seeker;
    while ( seeker != tree.black_nil ) {
        size_t seeker_size = get_size( seeker->header );
        if ( key == seeker_size ) {
            remove = seeker;
            break;
        }
        if ( key < seeker_size ) {
            if ( seeker_size < best_fit_size ) {
                remove = seeker;
                best_fit_size = seeker_size;
            }
            seeker = seeker->left;
        } else {
            seeker = seeker->right;
        }
    }
    // We can decompose the Cormen et.al. deletion logic because coalesce and malloc use it.
    return delete_rb_node( remove );
}

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
static inline void paint_node( struct rb_node *node, enum rb_color color )
{
    color == RED ? ( node->header |= RED_PAINT ) : ( node->header &= BLK_PAINT );
}

/// @brief get_color   returns the color of a node from the value of its header.
/// @param header_val  the value of the node in question passed by value.
/// @return            RED or BLACK
static inline enum rb_color get_color( header header_val ) { return ( header_val & COLOR_MASK ) == RED_PAINT; }

/// @brief get_size    returns size in bytes as a size_t from the value of node's header.
/// @param header_val  the value of the node in question passed by value.
/// @return            the size in bytes as a size_t of the node.
static inline size_t get_size( header header_val ) { return SIZE_MASK & header_val; }

/// @brief get_min     returns the smallest node in a valid binary search tree.
/// @param *root       the root of any valid binary search tree.
/// @param *black_nil  the sentinel node at the bottom of the tree that is always black.
/// @return            a pointer to the minimum node in a valid binary search tree.
static inline struct rb_node *get_min( struct rb_node *root, struct rb_node *black_nil )
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
static inline bool is_left_space( const struct rb_node *node ) { return !( node->header & LEFT_ALLOCATED ); }

/// @brief init_header_size  initializes any node as the size and indicating left is allocated.
///                          Left is allocated because we always coalesce left and right.
/// @param *node             the region of possibly uninitialized heap we must initialize.
/// @param payload           the payload in bytes as a size_t of the current block we initialize
static inline void init_header_size( struct rb_node *node, size_t payload )
{
    node->header = LEFT_ALLOCATED | payload;
}

/// @brief init_footer  initializes footer at end of the heap block to matcht the current header.
/// @param *node        the current node with a header field we will use to set the footer.
/// @param payload      the size of the current nodes free memory.
static inline void init_footer( struct rb_node *node, size_t payload )
{
    header *footer = (header *)( (uint8_t *)node + payload );
    *footer = node->header;
}

/// @brief get_right_neighbor  gets the address of the next rb_node in the heap to the right.
/// @param *current            the rb_node we start at to then jump to the right.
/// @param payload             the size in bytes as a size_t of the current rb_node block.
/// @return                    the rb_node to the right of the current.
static inline struct rb_node *get_right_neighbor( const struct rb_node *current, size_t payload )
{
    return (struct rb_node *)( (uint8_t *)current + HEADERSIZE + payload );
}

/// @brief *get_left_neighbor  uses the left block size gained from the footer to move to the header.
/// @param *node               the current header at which we reside.
/// @return                    a header pointer to the header for the block to the left.
static inline struct rb_node *get_left_neighbor( const struct rb_node *node )
{
    header *left_footer = (header *)( (uint8_t *)node - HEADERSIZE );
    return (struct rb_node *)( (uint8_t *)node - ( *left_footer & SIZE_MASK ) - HEADERSIZE );
}

/// @brief get_client_space  steps into the client space just after the header of a rb_node.
/// @param *node_header      the rb_node we start at before retreiving the client space.
/// @return                  the voi/// address of the client space they are now free to use.
static inline void *get_client_space( const struct rb_node *node_header )
{
    return (uint8_t *)node_header + HEADERSIZE;
}

/// @brief get_rb_node    steps to the rb_node header from the space the client was using.
/// @param *client_space  the voi/// the client was using for their type. We step to the left.
/// @return               a pointer to the rb_node of our heap block.
static inline struct rb_node *get_rb_node( const void *client_space )
{
    return (struct rb_node *)( (uint8_t *)client_space - HEADERSIZE );
}

/////////////////////////////    Debugging and Testing Functions   //////////////////////////////////

// NOLINTBEGIN(misc-no-recursion)

/// @breif check_init    checks the internal representation of our heap, especially the head and
///                      tail nodes for any issues that would ruin our algorithms.
/// @param hr            start and end of the heap
/// @param heap_size     the total size in bytes of the heap.
/// @return              true if everything is in order otherwise false.
static bool check_init( struct heap_range r, size_t heap_size )
{
    if ( is_left_space( r.start ) ) {
        BREAKPOINT();
        return false;
    }
    if ( (uint8_t *)r.end - (uint8_t *)r.start + HEAP_NODE_WIDTH != heap_size ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

/// @brief is_memory_balanced  loops through all blocks of memory to verify that the sizes
///                            reported match the global bookeeping in our struct.
/// @param *total_free_mem     the output parameter of the total size used as another check.
/// @param hr                  start and end of the heap
/// @param s                   size of the heap memory and total free nodes.
/// @return                    true if our tallying is correct and our totals match.
static bool is_memory_balanced( size_t *total_free_mem, struct heap_range r, struct size_total s )
{
    // Check that after checking all headers we end on size 0 tail and then end of
    // address space.
    struct rb_node *cur_node = r.start;
    size_t size_used = HEAP_NODE_WIDTH;
    size_t total_free_nodes = 0;
    while ( cur_node != r.end ) {
        size_t block_size_check = get_size( cur_node->header );
        if ( block_size_check == 0 ) {
            BREAKPOINT();
            return false;
        }

        if ( is_block_allocated( cur_node->header ) ) {
            size_used += block_size_check + HEADERSIZE;
        } else {
            total_free_nodes++;
            *total_free_mem += block_size_check + HEADERSIZE;
        }
        cur_node = get_right_neighbor( cur_node, block_size_check );
    }
    if ( size_used + *total_free_mem != s.byte_size ) {
        BREAKPOINT();
        return false;
    }
    if ( total_free_nodes != s.count_total ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

/// @brief is_red_red  determines if a red red violation of a red black tree has occured.
/// @param *root       the current root of the tree to begin at for checking all subtrees.
/// @param *black_nil  the sentinel node at the bottom of the tree that is always black.
/// @return            true if there is a red-red violation, false if we pass.
static bool is_red_red( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil || ( root->right == black_nil && root->left == black_nil ) ) {
        return false;
    }
    if ( get_color( root->header ) == RED ) {
        if ( get_color( root->left->header ) == RED || get_color( root->right->header ) == RED ) {
            BREAKPOINT();
            return true;
        }
    }
    return is_red_red( root->right, black_nil ) || is_red_red( root->left, black_nil );
}

/// @brief calculate_bheight  determines if every path from a node to the tree.black_nil
///                           has the same number of black nodes.
/// @param *root              the root of the tree to begin searching.
/// @param *black_nil         the sentinel node at the bottom of the tree that is always black.
/// @return                   -1 if the rule was not upheld, the black height if the rule is held.
static int calculate_bheight( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 0;
    }
    int lf_bheight = calculate_bheight( root->left, black_nil );
    int rt_bheight = calculate_bheight( root->right, black_nil );
    int add = get_color( root->header ) == BLACK;
    if ( lf_bheight == -1 || rt_bheight == -1 || lf_bheight != rt_bheight ) {
        BREAKPOINT();
        return -1;
    }
    return lf_bheight + add;
}

/// @brief is_bheight_valid  the wrapper for calculate_bheight that verifies that the black
///                          height property is upheld.
/// @param *root             the starting node of the red black tree to check.
/// @param *black_nil        the sentinel node at the bottom of the tree that is always black.
/// @return                  true if proper black height is consistently maintained throughout tree.
static bool is_bheight_valid( const struct rb_node *root, const struct rb_node *black_nil )
{
    return calculate_bheight( root, black_nil ) != -1;
}

/// @brief extract_tree_mem  sums the total memory in the red black tree to see if it matches the
///                          total memory we got from traversing blocks of the heap.
/// @param *root             the root to start at for the summing recursive search.
/// @param *black_nil        the sentinel node at the bottom of the tree that is always black.
/// @return                  the total memory in bytes as a size_t in the red black tree.
static size_t extract_tree_mem( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 0UL;
    }
    return get_size( root->header ) + HEADERSIZE + extract_tree_mem( root->right, black_nil )
           + extract_tree_mem( root->left, black_nil );
}

/// @brief is_rbtree_mem_valid  a wrapper for tree memory sum function used to check correctness.
/// @param *root                the root node to begin at for the recursive summing search.
/// @param *black_nil           the sentinel node at the bottom of the tree that is always black.
/// @param total_free_mem       the previously calculated free memory from a linear heap search.
/// @return                     true if the totals match false if they do not.
static bool is_rbtree_mem_valid( const struct rb_node *root, const struct rb_node *black_nil,
                                 size_t total_free_mem )
{
    if ( total_free_mem != extract_tree_mem( root, black_nil ) ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

/// @brief is_parent_valid  for duplicate node operations it is important to check the parents
///                         and fields are updated corectly so we can continue using the tree.
/// @param *root            the root to start at for the recursive search.
/// @param *black_nil       the sentinel node at the bottom of the tree that is always black.
/// @return                 true if every parent child relationship is accurate.
static bool is_parent_valid( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil ) {
        return true;
    }
    if ( root->left != black_nil && root->left->parent != root ) {
        BREAKPOINT();
        return false;
    }
    if ( root->right != black_nil && root->right->parent != root ) {
        BREAKPOINT();
        return false;
    }
    return is_parent_valid( root->left, black_nil ) && is_parent_valid( root->right, black_nil );
}

static int calculate_bheight_v2( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 1;
    }
    int left_height = calculate_bheight_v2( root->left, black_nil );
    int right_height = calculate_bheight_v2( root->right, black_nil );
    if ( left_height != 0 && right_height != 0 && left_height != right_height ) {
        BREAKPOINT();
        return 0;
    }
    if ( left_height != 0 && right_height != 0 ) {
        return get_color( root->header ) == RED ? left_height : left_height + 1;
    }
    return 0;
}

/// @brief is_bheight_valid_V2  the wrapper for calculate_bheight_V2 that verifies that the
///                             black height property is upheld.
/// @param *root                the starting node of the red black tree to check.
/// @param *black_nil           the sentinel node at the bottom of the tree that is always black.
/// @return                     true if the paths are valid, false if not.
static bool is_bheight_valid_v2( const struct rb_node *root, const struct rb_node *black_nil )
{
    return calculate_bheight_v2( root, black_nil ) != 0;
}

/// @brief is_binary_tree  confirms the validity of a binary search tree. Nodes to the left
///                        should be less than the root and nodes to the right should be greater.
/// @param *root           the root of the tree from which we examine children.
/// @param *black_nil      the sentinel node at the bottom of the tree that is always black.
/// @return                true if the tree is valid, false if not.
static bool is_binary_tree( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil ) {
        return true;
    }
    size_t root_value = get_size( root->header );
    if ( root->left != black_nil && root_value < get_size( root->left->header ) ) {
        BREAKPOINT();
        return false;
    }
    if ( root->right != black_nil && root_value > get_size( root->right->header ) ) {
        BREAKPOINT();
        return false;
    }
    return is_binary_tree( root->left, black_nil ) && is_binary_tree( root->right, black_nil );
}

/////////////////////////////        Printing Functions            //////////////////////////////////

static int get_black_height( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 0;
    }
    if ( get_color( root->left->header ) == BLACK ) {
        return 1 + get_black_height( root->left, black_nil );
    }
    return get_black_height( root->left, black_nil );
}

static void print_node( const struct rb_node *root, const struct rb_node *black_nil, enum print_style style )
{
    size_t block_size = get_size( root->header );
    printf( COLOR_CYN );
    if ( root->parent != black_nil ) {
        root->parent->left == root ? printf( "L:" ) : printf( "R:" );
    }
    printf( COLOR_NIL );
    get_color( root->header ) == BLACK ? printf( COLOR_BLK ) : printf( COLOR_RED );
    if ( style == VERBOSE ) {
        printf( "%p:", root );
    }
    printf( "(%zubytes)", block_size );
    printf( COLOR_NIL );
    if ( style == VERBOSE ) {
        // print the black-height
        printf( "(bh: %d)", get_black_height( root, black_nil ) );
    }
    printf( "\n" );
}

/// @brief print_inner_tree  recursively prints the contents of a red black tree with color and in
///                          a style similar to a directory structure to be read from left to right.
/// @param *root             the root node to start at.
/// @param *black_nil        the sentinel node at the bottom of the tree that is always black.
/// @param *prefix           the string we print spacing and characters across recursive calls.
/// @param node_type         the node to print can either be a leaf or internal branch.
/// @param style             the print style: PLAIN or VERBOSE(displays memory addresses).
static void print_inner_tree( const struct rb_node *root, const struct rb_node *black_nil, const char *prefix,
                              const enum print_link node_type, enum print_style style )
{
    if ( root == black_nil ) {
        return;
    }
    printf( "%s", prefix );
    printf( "%s", node_type == LEAF ? " └──" : " ├──" );
    print_node( root, black_nil, style );

    char *str = NULL;
    int string_length = snprintf( NULL, 0, "%s%s", prefix, node_type == LEAF ? "     " : " │   " ); // NOLINT
    if ( string_length > 0 ) {
        str = malloc( string_length + 1 );
        (void)snprintf( str, string_length, "%s%s", prefix, node_type == LEAF ? "     " : " │   " ); // NOLINT
    }
    if ( str != NULL ) {
        if ( root->right == black_nil ) {
            print_inner_tree( root->left, black_nil, str, LEAF, style );
        } else if ( root->left == black_nil ) {
            print_inner_tree( root->right, black_nil, str, LEAF, style );
        } else {
            print_inner_tree( root->right, black_nil, str, BRANCH, style );
            print_inner_tree( root->left, black_nil, str, LEAF, style );
        }
    } else {
        printf( COLOR_ERR "memory exceeded. Cannot display tree." COLOR_NIL );
    }
    free( str );
}

/// @brief print_alloc_block  prints the contents of an allocated block of memory.
/// @param *node              a valid rb_node to a block of allocated memory.
static void print_alloc_block( const struct rb_node *node )
{
    size_t block_size = get_size( node->header );
    // We will see from what direction our header is messed up by printing 16
    // digits.
    printf( COLOR_GRN "%p: HDR->0x%016zX(%zubytes)\n" COLOR_NIL, node, node->header, block_size );
}

/// @brief print_free_block  prints the contents of a free block of heap memory.
/// @param *header           a valid header to a block of allocated memory.
static void print_free_block( const struct rb_node *node )
{
    size_t block_size = get_size( node->header );
    header *footer = (header *)( (uint8_t *)node + block_size );
    /* We should be able to see the header is the same as the footer. However, due
     * to fixup functions, the color may change for nodes and color is irrelevant
     * to footers.
     */
    header to_print = *footer;
    if ( get_size( *footer ) != get_size( node->header ) ) {
        to_print = ULLONG_MAX;
    }
    // How far indented the Header field normally is for all blocks.
    short indent_struct_fields = PRINTER_INDENT;
    get_color( node->header ) == BLACK ? printf( COLOR_BLK ) : printf( COLOR_RED );
    printf( "%p: HDR->0x%016zX(%zubytes)\n", node, node->header, block_size );
    printf( "%*c", indent_struct_fields, ' ' );

    if ( node->parent ) {
        printf( get_color( node->parent->header ) == BLACK ? COLOR_BLK : COLOR_RED );
        printf( "PRN->%p\n", node->parent );
    } else {
        printf( "PRN->%p\n", NULL );
    }
    printf( COLOR_NIL );
    printf( "%*c", indent_struct_fields, ' ' );
    if ( node->left ) {
        printf( get_color( node->left->header ) == BLACK ? COLOR_BLK : COLOR_RED );
        printf( "LFT->%p\n", node->left );
    } else {
        printf( "LFT->%p\n", NULL );
    }
    printf( COLOR_NIL );
    printf( "%*c", indent_struct_fields, ' ' );
    if ( node->right ) {
        printf( get_color( node->right->header ) == BLACK ? COLOR_BLK : COLOR_RED );
        printf( "RGT->%p\n", node->right );
    } else {
        printf( "RGT->%p\n", NULL );
    }

    // The next and footer fields may not match the current node's color bit, and
    // that is ok. we will only worry about the next node's color when we delete a
    // duplicate.
    printf( COLOR_NIL );
    printf( "%*c", indent_struct_fields, ' ' );
    printf( "FTR->0x%016zX\n", to_print );
}

/// @brief print_error_block  prints a helpful error message if a block is corrupted.
/// @param *header            a header to a block of memory.
/// @param block_size         the full size of a block of memory, not just the user block size.
static void print_error_block( const struct rb_node *node, size_t block_size )
{
    printf( "\n%p: HDR->0x%016zX->%zubyts\n", node, node->header, block_size );
    printf( "Block size is too large and header is corrupted.\n" );
}

/// @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
///                        notice where we went wrong and what the addresses were.
/// @param bad_jump        two nodes with a bad jump from one to the other
/// @param *root           the root node of the tree to start at for an overall heap check.
/// @param *black_nil      the sentinel node at the bottom of the tree that is always black.
static void print_bad_jump( const struct rb_node *current, const struct bad_jump j,
                            const struct rb_node *black_nil )
{
    size_t prev_size = get_size( j.prev->header );
    size_t cur_size = get_size( current->header );
    printf( "A bad jump from the value of a header has occured. Bad distance to "
            "next header.\n" );
    printf( "The previous address: %p:\n", j.prev );
    printf( "\tHeader Hex Value: %016zX:\n", j.prev->header );
    printf( "\tBlock Byte Value: %zubytes:\n", prev_size );
    printf( "\nJump by %zubytes...\n", prev_size );
    printf( "The current address: %p:\n", current );
    printf( "\tHeader Hex Value: 0x%016zX:\n", current->header );
    printf( "\tBlock Byte Value: %zubytes:\n", cur_size );
    printf( "\nJump by %zubytes...\n", cur_size );
    printf( "Current state of the free tree:\n" );
    print_rb_tree( j.root, black_nil, VERBOSE );
}

/// @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
/// @param *root          the root node to begin at for printing recursively.
/// @param *black_nil     the sentinel node at the bottom of the tree that is always black.
/// @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
static void print_rb_tree( const struct rb_node *root, const struct rb_node *black_nil, enum print_style style )
{
    if ( root == black_nil ) {
        return;
    }
    printf( " " );
    print_node( root, black_nil, style );

    if ( root->right == black_nil ) {
        print_inner_tree( root->left, black_nil, "", LEAF, style );
    } else if ( root->left == black_nil ) {
        print_inner_tree( root->right, black_nil, "", LEAF, style );
    } else {
        print_inner_tree( root->right, black_nil, "", BRANCH, style );
        print_inner_tree( root->left, black_nil, "", LEAF, style );
    }
}

/// @brief print_all    prints our the complete status of the heap, all of its blocks, and
///                     the sizes the blocks occupy. Printing should be clean with no overlap of
///                     unique id's between heap blocks or corrupted headers.
/// @param hr           start and end of the heap
/// @param heap_size    the size in bytes of the heap.
/// @param *root        the root of the tree we start at for printing.
/// @param *black_nil   the sentinel node that waits at the bottom of the tree for all leaves.
static void print_all( struct heap_range r, size_t heap_size, struct rb_node *root, struct rb_node *black_nil )
{
    struct rb_node *node = r.start;
    printf( "Heap client segment starts at address %p, ends %p. %zu total bytes "
            "currently used.\n",
            node, r.end, heap_size );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "COLOR KEY: " COLOR_BLK "[BLACK NODE] " COLOR_NIL COLOR_RED "[RED NODE] " COLOR_NIL COLOR_GRN
            "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "%p: START OF  HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", r.start );
    struct rb_node *prev = node;
    while ( node != r.end ) {
        size_t full_size = get_size( node->header );

        if ( full_size == 0 ) {
            print_bad_jump( node, ( struct bad_jump ){ prev, root }, black_nil );
            printf( "Last known pointer before jump: %p", prev );
            return;
        }
        if ( (void *)node > r.end ) {
            print_error_block( node, full_size );
            return;
        }
        if ( is_block_allocated( node->header ) ) {
            print_alloc_block( node );
        } else {
            print_free_block( node );
        }
        prev = node;
        node = get_right_neighbor( node, full_size );
    }
    get_color( black_nil->header ) == BLACK ? printf( COLOR_BLK ) : printf( COLOR_RED );
    printf( "%p: BLACK NULL HDR->0x%016zX\n" COLOR_NIL, black_nil, black_nil->header );
    printf( "%p: FINAL ADDRESS", (uint8_t *)r.end + HEAP_NODE_WIDTH );
    printf( "\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "COLOR KEY: " COLOR_BLK "[BLACK NODE] " COLOR_NIL COLOR_RED "[RED NODE] " COLOR_NIL COLOR_GRN
            "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "\nRED BLACK TREE OF FREE NODES AND BLOCK SIZES.\n" );
    printf( "HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n" );
    printf( COLOR_CYN "(+X)" COLOR_NIL );
    printf( " INDICATES DUPLICATE NODES IN THE  THEY HAVE A NEXT NODE.\n" );
    print_rb_tree( root, black_nil, VERBOSE );
}

// NOLINTEND(misc-no-recursion)
