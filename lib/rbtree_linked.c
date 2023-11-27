/// Author: Alexander Griffin Lopez
/// File: rbtree_linked.c
/// ---------------------
///  This file contains my implementation of an explicit heap allocator. This allocator uses a tree
///  implementation to track the free space in the heap, relying on the properties of a red-black
///  tree to remain balanced. This implementation also uses some interesting strategies to unify
///  left and right cases for a red black tree and maintains a doubly linked list of duplicate nodes
///  of the same size if a node in the tree has repeats of its size.
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
///     bottom of the tree proved useful for the tree and the linked list of duplicate memory block
///     sizes.
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

/////////////////////////////         Type Definitions             //////////////////////////////////

typedef size_t header;

/// Red Black Free Tree:
///  - Maintain a red black tree of free nodes.
///  - Root is black
///  - No red node has a red child
///  - New insertions are red
///  - NULL is considered black. We use a black sentinel instead. Physically
/// lives on the heap.
///  - Every path from root to black_nil, root not included, has same number of
/// black nodes.
///  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
///  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor
/// status for coalescing.
///  - Use a list_start pointer to a doubly linked list of duplicate nodes of the
/// same size.
struct rb_node
{
    // block size, allocation status, left neighbor status, and node color.
    header header;
    struct rb_node *parent;
    struct rb_node *links[2];
    // Points to a list which we will use P and N to manage to distinguish from
    // the tree.
    struct duplicate_node *list_start;
};

struct duplicate_node
{
    header header;
    struct rb_node *parent;
    struct duplicate_node *links[2];
    struct rb_node *list_start;
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

struct swap
{
    struct rb_node *remove;
    struct rb_node *replacement;
};

enum rb_color
{
    BLACK = 0,
    RED = 1
};

/// We can unify symmetric cases with an enum because (!L == R) and (!R == L).
enum tree_link
{
    // (L == LEFT), (R == RIGHT)
    L = 0,
    R = 1
};

/// When you see these, know that we are working with a doubly linked list, not a tree.
enum list_link
{
    // (P == PREVIOUS), (N == NEXT)
    P = 0,
    N = 1
};

#define SIZE_MASK ~0x7UL
#define HEADERSIZE sizeof( size_t )
#define MIN_BLOCK_SIZE ( sizeof( struct rb_node ) + HEADERSIZE )
#define HEAP_NODE_WIDTH sizeof( struct rb_node )
#define FREED 0x0UL
#define ALLOCATED 0x1UL
#define LEFT_ALLOCATED 0x2UL
#define COLOR_MASK 0x4UL
#define RED_PAINT 0x4UL
#define BLK_PAINT ~0x4UL
#define LEFT_FREE ~0x2UL

/////////////////////////  Static Heap Tracking   /////////////////////////////

// NOLINTBEGIN(*-non-const-global-variables)

/// Red Black Free Tree:
///  - Maintain a red black tree of free nodes.
///  - Root is black
///  - No red node has a red child
///  - New insertions are red
///  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
///  - Every path from root to black_nil, root not included, has same number of black nodes.
///  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
///  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status for coalescing.
///  - Use a *list_start pointer to a doubly linked list of duplicate nodes of the same size.
///  - For more details on the types see the _utilities.h file.
static struct free_nodes
{
    struct rb_node *tree_root;
    struct rb_node *black_nil;
    struct duplicate_node *list_tail;
    size_t total;
} free_nodes;

// Mainly for debugging and verifying our heap.
static struct heap
{
    void *client_start;
    void *client_end;
    size_t heap_size;
} heap;

// NOLINTEND(*-non-const-global-variables)

////////////////////////    Forward Declarations   //////////////////////////////////

static void init_free_node( struct rb_node *to_free, size_t block_size );
static void *split_alloc( struct rb_node *free_block, size_t request, size_t block_space );
static struct rb_node *coalesce( struct rb_node *leftmost_node );
static void rotate( struct rb_node *current, enum tree_link rotation );
static void add_duplicate( struct rb_node *head, struct duplicate_node *to_add );
static void fix_rb_insert( struct rb_node *current );
static void insert_rb_node( struct rb_node *current );
static void rb_transplant( struct swap nodes );
static struct rb_node *delete_duplicate( struct rb_node *head );
static void fix_rb_delete( struct rb_node *extra_black );
static struct rb_node *delete_rb_node( struct rb_node *remove );
static struct rb_node *find_best_fit( size_t key );
static void remove_head( struct rb_node *head );
static struct rb_node *free_coalesced_node( void *to_coalesce );
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
static int calculate_bheight( const struct rb_node *root, const struct rb_node *black_nil );
static bool is_bheight_valid( const struct rb_node *root, const struct rb_node *black_nil );
static size_t extract_tree_mem( const struct rb_node *root, const void *nil_and_tail );
static bool is_rbtree_mem_valid( const struct rb_node *root, const void *nil_and_tail, size_t total_free_mem );
static bool is_parent_valid( const struct rb_node *root, const struct rb_node *black_nil );
static int calculate_bheight_v2( const struct rb_node *root, const struct rb_node *black_nil );
static bool is_bheight_valid_v2( const struct rb_node *root, const struct rb_node *black_nil );
static bool are_subtrees_valid( const struct rb_node *root, const struct rb_node *black_nil );
static void print_rb_tree( const struct rb_node *root, void *nil_and_tail, enum print_style style );
static void print_all( struct heap_range r, size_t heap_size, struct rb_node *tree_root,
                       struct rb_node *black_nil );

///////////////////////          Shared Heap Functions    ////////////////////////////////////

size_t get_free_total( void ) { return free_nodes.total; }

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
    free_nodes.black_nil = heap.client_end;
    free_nodes.list_tail = heap.client_end;
    free_nodes.black_nil->header = 1UL;
    paint_node( free_nodes.black_nil, BLACK );

    // Set up the root of the tree (top) that starts as our largest free block.
    free_nodes.tree_root = heap.client_start;
    init_header_size( free_nodes.tree_root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    paint_node( free_nodes.tree_root, BLACK );
    init_footer( free_nodes.tree_root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    free_nodes.tree_root->parent = free_nodes.black_nil;
    free_nodes.tree_root->links[L] = free_nodes.black_nil;
    free_nodes.tree_root->links[R] = free_nodes.black_nil;
    free_nodes.tree_root->list_start = free_nodes.list_tail;
    free_nodes.total = 1;
    return true;
}

void *mymalloc( size_t requested_size )
{
    if ( requested_size == 0 || requested_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    size_t client_request = roundup( requested_size, ALIGNMENT );
    // Search the tree for the best possible fitting node.
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
    size_t request = roundup( new_size, ALIGNMENT );
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
    if ( ptr == NULL ) {
        return;
    }
    struct rb_node *to_insert = get_rb_node( ptr );
    to_insert = coalesce( to_insert );
    init_free_node( to_insert, get_size( to_insert->header ) );
}

////////////////////////////       Shared Debugging    /////////////////////////////////////

bool validate_heap( void )
{
    if ( !check_init( ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size ) ) {
        return false;
    }
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    size_t total_free_mem = 0;
    if ( !is_memory_balanced( &total_free_mem, ( struct heap_range ){ heap.client_start, heap.client_end },
                              ( struct size_total ){ heap.heap_size, free_nodes.total } ) ) {
        return false;
    }
    // Does a tree search for all memory match the linear heap search for totals?
    if ( !is_rbtree_mem_valid( free_nodes.tree_root, free_nodes.black_nil, total_free_mem ) ) {
        return false;
    }
    // Two red nodes in a row are invalid for the table.
    if ( is_red_red( free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }
    // Does every path from a node to the black sentinel contain the same number of black nodes.
    if ( !is_bheight_valid( free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }
    // This comes from a more official write up on red black trees so I included it.
    if ( !is_bheight_valid_v2( free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }
    // Check that the parents and children are updated correctly if duplicates are deleted.
    if ( !is_parent_valid( free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }
    if ( !are_subtrees_valid( free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }
    return true;
}

size_t align( size_t request ) { return roundup( request, ALIGNMENT ); }

size_t capacity( void )
{
    size_t total_free_mem = 0;
    size_t block_size_check = 0;
    for ( struct rb_node *cur_node = heap.client_start; cur_node != heap.client_end;
          cur_node = get_right_neighbor( cur_node, block_size_check ) ) {
        block_size_check = get_size( cur_node->header );
        if ( !is_block_allocated( cur_node->header ) ) {
            total_free_mem += block_size_check;
        }
    }
    return total_free_mem;
}

void validate_heap_state( const struct heap_block expected[], struct heap_block actual[], size_t len )
{
    (void)expected;
    (void)actual;
    (void)len;
    UNIMPLEMENTED();
}

///////////////////////////         Shared Printer      //////////////////////////////////////

void print_free_nodes( enum print_style style )
{
    printf( COLOR_CYN "(+X)" COLOR_NIL );
    printf( " Indicates duplicate nodes in the tree linked by a doubly-linked list.\n" );
    print_rb_tree( free_nodes.tree_root, free_nodes.black_nil, style );
}

void dump_heap( void )
{
    print_all( ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size, free_nodes.tree_root,
               free_nodes.black_nil );
}

////////////////////////    Static Heap Helper Functions   //////////////////////////////////

/// @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
/// @param *to_free        the heap_node to add to the red black tree.
/// @param block_size      the size we use to initialize the node and find the right place in tree.
static void init_free_node( struct rb_node *to_free, size_t block_size )
{
    to_free->header = block_size | LEFT_ALLOCATED | RED_PAINT;
    to_free->list_start = free_nodes.list_tail;
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
        (void)free_coalesced_node( rightmost_node );
    }
    // We use our static struct for convenience here to tell where our segment start is.
    if ( leftmost_node != heap.client_start && is_left_space( leftmost_node ) ) {
        leftmost_node = get_left_neighbor( leftmost_node );
        coalesced_space += get_size( leftmost_node->header ) + HEADERSIZE;
        leftmost_node = free_coalesced_node( leftmost_node );
    }

    // Do not initialize footer here because we may coalesce during realloc. Preserve user data.
    init_header_size( leftmost_node, coalesced_space );
    return leftmost_node;
}

//////////////////////////   Static RBTree Implementation    ////////////////////////////////////

/// @brief rotate     a unified version of the traditional left and right rotation functions. The
///                   rotation is either left or right and opposite is its opposite direction. We
///                   take the current nodes child, and struct swap them and their arbitrary subtrees are
///                   re-linked correctly depending on the direction of the rotation.
/// @param *current   the node around which we will rotate.
/// @param rotation   either left or right. Determines the rotation and its opposite direction.
static void rotate( struct rb_node *current, enum tree_link rotation )
{
    struct rb_node *child = current->links[!rotation];
    current->links[!rotation] = child->links[rotation];

    if ( child->links[rotation] != free_nodes.black_nil ) {
        child->links[rotation]->parent = current;
    }
    child->parent = current->parent;
    if ( current->parent == free_nodes.black_nil ) {
        free_nodes.tree_root = child;
    } else {
        // True == 1 == R, otherwise False == 0 == L
        current->parent->links[current->parent->links[R] == current] = child;
    }
    child->links[rotation] = current;
    current->parent = child;
}

///     Static Red-Black Tree Insertion Helper Function

/// @brief add_duplicate  this implementation stores duplicate nodes in a linked list to prevent the
///                       rotation of duplicates in the tree. This adds the duplicate node to the
///                       linked list of the node already present.
/// @param *head          the node currently organized in the tree. We will add to its list.
/// @param *to_add        the node to add to the linked list.
static void add_duplicate( struct rb_node *head, struct duplicate_node *to_add )
{
    to_add->header = head->header;
    // These fields should not matter but we should initialize them to be safe.
    to_add->parent = NULL;
    to_add->list_start = NULL;
    // Get the first next node in the doubly linked list, invariant and correct its left field.
    head->list_start->links[P] = to_add;
    to_add->links[N] = head->list_start;
    to_add->links[P] = (struct duplicate_node *)head;
    head->list_start = to_add;
    free_nodes.total++;
}

///      Static Red-Black Tree Insertion Logic

/// @brief fix_rb_insert  implements a modified Cormen et.al. red black fixup after the insertion of
///                       a new node. Unifies the symmetric left and right cases with the use of
///                       an array and an enum tree_link.
/// @param *current       the current node that has just been added to the red black tree.
static void fix_rb_insert( struct rb_node *current )
{
    while ( get_color( current->parent->header ) == RED ) {
        enum tree_link symmetric_case = current->parent->parent->links[R] == current->parent;
        struct rb_node *aunt = current->parent->parent->links[!symmetric_case];
        if ( get_color( aunt->header ) == RED ) {
            paint_node( aunt, BLACK );
            paint_node( current->parent, BLACK );
            paint_node( current->parent->parent, RED );
            current = current->parent->parent;
        } else {
            if ( current == current->parent->links[!symmetric_case] ) {
                current = current->parent;
                rotate( current, symmetric_case );
            }
            paint_node( current->parent, BLACK );
            paint_node( current->parent->parent, RED );
            rotate( current->parent->parent, !symmetric_case );
        }
    }
    paint_node( free_nodes.tree_root, BLACK );
}

/// @brief insert_struct rb_node  a modified insertion with additional logic to add duplicates if the
///                        size in bytes of the block is already in the tree.
/// @param *current        we must insert to tree or add to a list as duplicate.
static void insert_rb_node( struct rb_node *current )
{
    struct rb_node *seeker = free_nodes.tree_root;
    struct rb_node *parent = free_nodes.black_nil;
    size_t current_key = get_size( current->header );
    while ( seeker != free_nodes.black_nil ) {
        parent = seeker;
        size_t seeker_size = get_size( seeker->header );

        // Duplicates with a linked list. No duplicates in tree while staying O(1) coalescing.
        if ( current_key == seeker_size ) {
            add_duplicate( seeker, (struct duplicate_node *)current );
            return;
        }
        // You may see this idiom throughout. L(0) if key fits in tree to left, R(1) if not.
        seeker = seeker->links[seeker_size < current_key];
    }
    current->parent = parent;
    if ( parent == free_nodes.black_nil ) {
        free_nodes.tree_root = current;
    } else {
        parent->links[get_size( parent->header ) < current_key] = current;
    }
    current->links[L] = current->links[R] = free_nodes.black_nil;
    // Every node in the tree is a dummy head for a doubly linked list of duplicates.
    current->list_start = free_nodes.list_tail;
    paint_node( current, RED );
    fix_rb_insert( current );
    free_nodes.total++;
}

///     Static Red-Black Tree Deletion Helper Functions

/// @brief rb_transplant  replaces node with the appropriate node to start balancing the tree.
/// @param *remove        the node we are removing from the tree.
/// @param *replacement   the node that will fill the remove position. It can be black_nil.
static void rb_transplant( struct swap nodes )
{
    if ( nodes.remove->parent == free_nodes.black_nil ) {
        free_nodes.tree_root = nodes.replacement;
    } else {
        nodes.remove->parent->links[nodes.remove->parent->links[R] == nodes.remove] = nodes.replacement;
    }
    nodes.replacement->parent = nodes.remove->parent;
}

/// @brief delete_duplicate  will remove a duplicate node from the tree when the request is coming
///                          from a call from malloc. Address of duplicate does not matter so we
///                          remove the first node from the linked list.
/// @param *head             We know this node has a next node and it must be removed for malloc.
static struct rb_node *delete_duplicate( struct rb_node *head )
{
    struct duplicate_node *next_node = head->list_start;
    next_node->links[N]->links[P] = (struct duplicate_node *)head;
    head->list_start = next_node->links[N];
    free_nodes.total--;
    return (struct rb_node *)next_node;
}

///      Static Red-Black Tree Deletion Logic

/// @brief fix_rb_delete  completes a unified Cormen et.al. fixup function. Uses a direction enum
///                       and an array to help unify code paths based on direction and opposites.
/// @param *extra_black   the extra_black node that was moved into place from the previous delete.
///                       May have broken rules of the tree or thrown off balance.
static void fix_rb_delete( struct rb_node *extra_black )
{
    // The extra_black is "doubly black" if we enter the loop, requiring repairs.
    while ( extra_black != free_nodes.tree_root && get_color( extra_black->header ) == BLACK ) {

        // We can cover left and right cases in one with simple directional link and its opposite.
        enum tree_link symmetric_case = extra_black->parent->links[R] == extra_black;

        struct rb_node *sibling = extra_black->parent->links[!symmetric_case];
        if ( get_color( sibling->header ) == RED ) {
            paint_node( sibling, BLACK );
            paint_node( extra_black->parent, RED );
            rotate( extra_black->parent, symmetric_case );
            sibling = extra_black->parent->links[!symmetric_case];
        }
        if ( get_color( sibling->links[L]->header ) == BLACK && get_color( sibling->links[R]->header ) == BLACK ) {
            paint_node( sibling, RED );
            extra_black = extra_black->parent;
        } else {
            if ( get_color( sibling->links[!symmetric_case]->header ) == BLACK ) {
                paint_node( sibling->links[symmetric_case], BLACK );
                paint_node( sibling, RED );
                rotate( sibling, !symmetric_case );
                sibling = extra_black->parent->links[!symmetric_case];
            }
            paint_node( sibling, get_color( extra_black->parent->header ) );
            paint_node( extra_black->parent, BLACK );
            paint_node( sibling->links[!symmetric_case], BLACK );
            rotate( extra_black->parent, symmetric_case );
            extra_black = free_nodes.tree_root;
        }
    }
    // The extra_black has reached a red node, making it "red-and-black", or the root. Paint BLACK.
    paint_node( extra_black, BLACK );
}

/// @brief delete_struct rb_node  performs the necessary steps to have a functional, balanced tree after
///                        deletion of any node in the tree.
/// @param *remove         the node to remove from the tree from a call to malloc or coalesce.
static struct rb_node *delete_rb_node( struct rb_node *remove )
{
    enum rb_color fixup_color_check = get_color( remove->header );

    struct rb_node *extra_black = NULL;
    if ( remove->links[L] == free_nodes.black_nil || remove->links[R] == free_nodes.black_nil ) {
        enum tree_link nil_link = remove->links[L] != free_nodes.black_nil;
        rb_transplant( ( struct swap ){ remove, extra_black = remove->links[!nil_link] } );
    } else {
        struct rb_node *replacement = get_min( remove->links[R], free_nodes.black_nil );
        fixup_color_check = get_color( replacement->header );
        extra_black = replacement->links[R];
        if ( replacement != remove->links[R] ) {
            rb_transplant( ( struct swap ){ replacement, extra_black } );
            replacement->links[R] = remove->links[R];
            replacement->links[R]->parent = replacement;
        } else {
            extra_black->parent = replacement;
        }
        rb_transplant( ( struct swap ){ remove, replacement } );
        replacement->links[L] = remove->links[L];
        replacement->links[L]->parent = replacement;
        paint_node( replacement, get_color( remove->header ) );
    }
    if ( fixup_color_check == BLACK ) {
        fix_rb_delete( extra_black );
    }
    free_nodes.total--;
    return remove;
}

/// @brief find_best_fit  a red black tree is well suited to best fit search in O(logN) time. We
///                       will find the best fitting node possible given the options in our tree.
/// @param key            the size_t number of bytes we are searching for in our tree.
/// @return               the pointer to the struct rb_node that is the best fit for our need.
static struct rb_node *find_best_fit( size_t key )
{
    struct rb_node *seeker = free_nodes.tree_root;
    size_t best_fit_size = ULLONG_MAX;
    struct rb_node *remove = seeker;
    while ( seeker != free_nodes.black_nil ) {
        size_t seeker_size = get_size( seeker->header );
        if ( key == seeker_size ) {
            remove = seeker;
            break;
        }
        /// The key is less than the current found size but let's remember this size on the way down
        /// as a candidate for the best fit. The closest fit will have won when we reach the bottom.
        if ( seeker_size < best_fit_size && seeker_size >= key ) {
            remove = seeker;
            best_fit_size = seeker_size;
        }
        seeker = seeker->links[seeker_size < key];
    }

    if ( remove->list_start != free_nodes.list_tail ) {
        // We will keep remove in the tree and just get the first node in doubly linked list.
        return delete_duplicate( remove );
    }
    return delete_rb_node( remove );
}

/// @brief remove_head  frees the head of a doubly linked list of duplicates which is a node in a
///                     red black tree. The next duplicate must become the new tree node and the
///                     parent and children must be adjusted to track this new node.
/// @param head         the node in the tree that must now be coalesced.
static void remove_head( struct rb_node *head )
{
    struct rb_node *new_head = (struct rb_node *)head->list_start;
    new_head->header = head->header;
    // Make sure we set up new start of list correctly for linked list.
    new_head->list_start = head->list_start->links[N];

    // Now transition to thinking about this new_head as a node in a tree, not a list.
    new_head->links[L] = head->links[L];
    new_head->links[R] = head->links[R];
    head->links[L]->parent = new_head;
    head->links[R]->parent = new_head;
    new_head->parent = head->parent;
    if ( head->parent == free_nodes.black_nil ) {
        free_nodes.tree_root = new_head;
    } else {
        head->parent->links[head->parent->links[R] == head] = new_head;
    }
}

/// @brief free_coalesced_node  a specialized version of node freeing when we find a neighbor we
///                             need to free from the tree before absorbing into our coalescing. If
///                             this node is a duplicate we can splice it from a linked list.
/// @param *to_coalesce         the node we now must find by address in the tree.
/// @return                     the node we have now correctly freed given all cases to find it.
static struct rb_node *free_coalesced_node( void *to_coalesce )
{
    struct rb_node *tree_node = to_coalesce;
    // Quick return if we just have a standard deletion.
    if ( tree_node->list_start == free_nodes.list_tail ) {
        return delete_rb_node( tree_node );
    }

    struct duplicate_node *list_node = to_coalesce;
    // to_coalesce is the head of a doubly linked list. Remove and make a new head.
    if ( tree_node->parent ) {
        remove_head( tree_node );

        // to_coalesce is next after the head and needs special attention due to list_start field.
    } else if ( list_node->links[P]->list_start == to_coalesce ) {
        tree_node = (struct rb_node *)list_node->links[P];
        tree_node->list_start = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

        // Finally the simple invariant case of the node being in middle or end of list.
    } else {
        list_node->links[P]->links[N] = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];
    }
    free_nodes.total--;
    return to_coalesce;
}

/////////////////////////////   Basic Block and Header Operations  //////////////////////////////////

/// @brief roundup         rounds up size to the nearest multiple of multiple to be aligned in the heap.
/// @param requested_size  size given to us by the client.
/// @param multiple        the nearest multiple to raise our number to.
/// @return                rounded number.
static inline size_t roundup( size_t requested_size, size_t multiple )
{
    return requested_size <= HEAP_NODE_WIDTH ? HEAP_NODE_WIDTH
                                             : ( requested_size + multiple - 1 ) & ~( multiple - 1 );
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
    for ( ; root->links[L] != black_nil; root = root->links[L] ) {}
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

/// @brief get_right_neighbor  gets the address of the next struct rb_node in the heap to the right.
/// @param *current            the struct rb_node we start at to then jump to the right.
/// @param payload             the size in bytes as a size_t of the current struct rb_node block.
/// @return                    the struct rb_node to the right of the current.
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
/// @param *node_header      the struct rb_node we start at before retreiving the client space.
/// @return                  the void address of the client space they are now free to use.
static inline void *get_client_space( const struct rb_node *node_header )
{
    return (uint8_t *)node_header + HEADERSIZE;
}

/// @brief get_struct rb_node    steps to the struct rb_node header from the space the client was using.
/// @param *client_space  the voi/// the client was using for their type. We step to the left.
/// @return               a pointer to the struct rb_node of our heap block.
static inline struct rb_node *get_rb_node( const void *client_space )
{
    return (struct rb_node *)( (uint8_t *)client_space - HEADERSIZE );
}

/////////////////////////////    Debugging and Testing Functions   //////////////////////////////////

// NOLINTBEGIN(misc-no-recursion)

/// @breif check_init    checks the internal representation of our heap, especially the head and tail
///                      nodes for any issues that would ruin our algorithms.
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
    if ( root == black_nil || ( root->links[R] == black_nil && root->links[L] == black_nil ) ) {
        return false;
    }
    if ( get_color( root->header ) == RED ) {
        if ( get_color( root->links[L]->header ) == RED || get_color( root->links[R]->header ) == RED ) {
            BREAKPOINT();
            return true;
        }
    }
    return is_red_red( root->links[R], black_nil ) || is_red_red( root->links[L], black_nil );
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
    int lf_bheight = calculate_bheight( root->links[L], black_nil );
    int rt_bheight = calculate_bheight( root->links[R], black_nil );
    int add = get_color( root->header ) == BLACK;
    if ( lf_bheight == -1 || rt_bheight == -1 || lf_bheight != rt_bheight ) {
        BREAKPOINT();
        return -1;
    }
    return lf_bheight + add;
}

/// @brief is_bheight_valid  the wrapper for calculate_bheight that verifies that
///                          the black height property is upheld.
/// @param *root             the starting node of the red black tree to check.
/// @param *black_nil        the sentinel node at the bottom of the tree that is always black.
/// @return                  true if proper black height is consistently maintained throughout tree.
static bool is_bheight_valid( const struct rb_node *root, const struct rb_node *black_nil )
{
    return calculate_bheight( root, black_nil ) != -1;
}

/// @brief extract_tree_mem  sums the total memory in the red black tree to see if it matches
///                          the total memory we got from traversing blocks of the heap.
/// @param *root             the root to start at for the summing recursive search.
/// @param *nil_and_tail     the address of a sentinel node serving as both list tail and black nil.
/// @return                  the total memory in bytes as a size_t in the red black tree.
static size_t extract_tree_mem( const struct rb_node *root, const void *nil_and_tail )
{
    if ( root == nil_and_tail ) {
        return 0UL;
    }
    // We may have repeats so make sure to add the linked list values.
    size_t total_mem = get_size( root->header ) + HEADERSIZE;
    for ( struct duplicate_node *tally_list = root->list_start; tally_list != nil_and_tail;
          tally_list = tally_list->links[N] ) {
        total_mem += get_size( tally_list->header ) + HEADERSIZE;
    }
    return total_mem + extract_tree_mem( root->links[R], nil_and_tail )
           + extract_tree_mem( root->links[L], nil_and_tail );
}

/// @brief is_rbtree_mem_valid  a wrapper for tree memory sum function used to check correctness.
/// @param *root                the root node to begin at for the recursive summing search.
/// @param *nil_and_tail        address of a sentinel node serving as both list tail and black nil.
/// @param total_free_mem       the total free memory collected from a linear heap search.
/// @return                     true if the totals match false if they do not.
static bool is_rbtree_mem_valid( const struct rb_node *root, const void *nil_and_tail, size_t total_free_mem )
{
    if ( extract_tree_mem( root, nil_and_tail ) != total_free_mem ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

/// @brief is_parent_valid  for duplicate node operations it is important to check the parents
///                         and fields are updated corectly so we can continue using the tree.
/// @param *root            the root to start at for the recursive search.
/// @param *black_nil       the sentinel node at the bottom of the tree that is always black.
/// @return                 true if all parent child relationships are correct.
static bool is_parent_valid( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil ) {
        return true;
    }
    if ( root->links[L] != black_nil && root->links[L]->parent != root ) {
        BREAKPOINT();
        return false;
    }
    if ( root->links[R] != black_nil && root->links[R]->parent != root ) {
        BREAKPOINT();
        return false;
    }
    return is_parent_valid( root->links[L], black_nil ) && is_parent_valid( root->links[R], black_nil );
}

/// @brief calculate_bheight_v2  verifies that the height of a red-black tree is valid.
///                              This is a similar function to calculate_bheight but comes
///                              from a more reliable source, because I saw results that made
///                              me doubt V1.
/// @param *root                 the root to start at for the recursive search.
/// @param *black_nil            the sentinel node at the bottom of the tree that is always black.
/// @citation                    Julienne Walker's writeup on topdown Red-Black trees has a
///                              helpful function for verifying black heights.
static int calculate_bheight_v2( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 1;
    }
    int left_height = calculate_bheight_v2( root->links[L], black_nil );
    int right_height = calculate_bheight_v2( root->links[R], black_nil );
    if ( left_height != 0 && right_height != 0 && left_height != right_height ) {
        BREAKPOINT();
        return 0;
    }
    if ( left_height != 0 && right_height != 0 ) {
        return get_color( root->header ) == RED ? left_height : left_height + 1;
    }
    return 0;
}

/// @brief is_bheight_valid_v2  the wrapper for calculate_bheight_v2 that
///                             verifies that the black height property is upheld.
/// @param *root                the starting node of the red black tree to check.
/// @param *black_nil           the sentinel node at the bottom of the tree that is always black.
/// @return                     true if the paths are valid, false if not.
static bool is_bheight_valid_v2( const struct rb_node *root, const struct rb_node *black_nil )
{
    return calculate_bheight_v2( root, black_nil ) != 0;
}

/// @brief strict_bounds_met  all rb_nodes to the left of the root of a binary tree must be strictly less than the
///                           root. All rb_nodes to the right must be strictly greater than the root. If you mess
///                           rotations you may have a valid binary tree in terms of a root and its two direct
///                           children but are violating some bound on a root further up the tree.
/// @param root               the recursive root we will check as we descend.
/// @param root_size          the original root size to which all rb_nodes in the subtrees must obey.
/// @param dir                if we check the right subtree all rb_nodes are greater, left subtree lesser.
/// @param nil                the nil of the tree. could be NULL or some dedicated address.
static bool strict_bound_met( const struct rb_node *root, size_t root_size, enum tree_link dir,
                              const struct rb_node *nil )
{
    if ( root == nil ) {
        return true;
    }
    size_t rb_node_size = get_size( root->header );
    if ( dir == L && rb_node_size > root_size ) {
        BREAKPOINT();
        return false;
    }
    if ( dir == R && rb_node_size < root_size ) {
        BREAKPOINT();
        return false;
    }
    return strict_bound_met( root->links[L], root_size, dir, nil )
           && strict_bound_met( root->links[R], root_size, dir, nil );
}

/// @brief are_subtrees_valid  fully checks the size of all subtrees to the left and right of the current rb_node.
///                            There must not be a rb_node lesser than the root size in the right subtrees and no
///                            rb_node exceeding the root size in the left subtree.
/// @param root                the recursive root we are checking as we traverse the tree dfs-style.
/// @param nil                 the nil of the tree either NULL or a dedicated address.
static bool are_subtrees_valid( const struct rb_node *root, const struct rb_node *nil )
{
    if ( root == nil ) {
        return true;
    }
    size_t root_size = get_size( root->header );
    if ( !strict_bound_met( root->links[L], root_size, L, nil )
         || !strict_bound_met( root->links[R], root_size, R, nil ) ) {
        BREAKPOINT();
        return false;
    }
    return are_subtrees_valid( root->links[L], nil ) && are_subtrees_valid( root->links[R], nil );
}

/////////////////////////////           Printing Functions         //////////////////////////////////

/// @brief get_black_height  gets the black node height of the tree excluding the current node.
/// @param *root             the starting root to search from to find the height.
/// @param *black_nil        the sentinel node at the bottom of the tree that is always black.
/// @return                  the black height from the current node as an integer.
static int get_black_height( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 0;
    }
    if ( get_color( root->links[L]->header ) == BLACK ) {
        return 1 + get_black_height( root->links[L], black_nil );
    }
    return get_black_height( root->links[L], black_nil );
}

/// @brief print_node     prints an individual node in its color and status as left or right child.
/// @param *root          the root we will print with the appropriate info.
/// @param *nil_and_tail  address of a sentinel node serving as both list tail and black nil.
/// @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
static void print_node( const struct rb_node *root, void *nil_and_tail, enum print_style style )
{
    size_t block_size = get_size( root->header );
    printf( COLOR_CYN );
    if ( root->parent != nil_and_tail ) {
        root->parent->links[L] == root ? printf( "L:" ) : printf( "R:" );
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
        printf( "%s(bh: %d)%s", COLOR_BLK, get_black_height( root, nil_and_tail ), COLOR_NIL );
    }

    printf( COLOR_CYN );
    // If a node is a duplicate, we will give it a special mark among nodes.
    if ( root->list_start != nil_and_tail ) {
        int duplicates = 1;
        struct duplicate_node *duplicate = root->list_start;
        for ( ; ( duplicate = duplicate->links[N] ) != nil_and_tail; duplicates++ ) {}
        printf( "(+%d)", duplicates );
    }
    printf( COLOR_NIL );
    printf( "\n" );
}

/// @brief print_inner_tree  recursively prints the contents of a red black tree with color
///                          and in a style similar to a directory structure to be read from
///                          left to right.
/// @param *root             the root node to start at.
/// @param *nil_and_tail     address of a sentinel node serving as both list tail and black nil.
/// @param *prefix           the string we print spacing and characters across recursive calls.
/// @param node_type         the node to print can either be a leaf or internal branch.
/// @param style             the print style: PLAIN or VERBOSE(displays memory addresses).
static void print_inner_tree( const struct rb_node *root, void *nil_and_tail, const char *prefix,
                              const enum print_link node_type, enum print_style style )
{
    if ( root == nil_and_tail ) {
        return;
    }
    // Print the root node
    printf( "%s", prefix );
    printf( "%s", node_type == LEAF ? " └──" : " ├──" );
    print_node( root, nil_and_tail, style );

    // Print any subtrees
    char *str = NULL;
    int string_length = snprintf( NULL, 0, "%s%s", prefix, node_type == LEAF ? "     " : " │   " ); // NOLINT
    if ( string_length > 0 ) {
        str = malloc( string_length + 1 );
        (void)snprintf( str, string_length, "%s%s", prefix, node_type == LEAF ? "     " : " │   " ); // NOLINT
    }
    if ( str != NULL ) {
        if ( root->links[R] == nil_and_tail ) {
            print_inner_tree( root->links[L], nil_and_tail, str, LEAF, style );
        } else if ( root->links[L] == nil_and_tail ) {
            print_inner_tree( root->links[R], nil_and_tail, str, LEAF, style );
        } else {
            print_inner_tree( root->links[R], nil_and_tail, str, BRANCH, style );
            print_inner_tree( root->links[L], nil_and_tail, str, LEAF, style );
        }
    } else {
        printf( COLOR_ERR "memory exceeded. Cannot display " COLOR_NIL );
    }
    free( str );
}

/// @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
/// @param *root          the root node to begin at for printing recursively.
/// @param *nil_and_tail  address of a sentinel node serving as both list tail and black nil.
/// @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
static void print_rb_tree( const struct rb_node *root, void *nil_and_tail, enum print_style style )
{
    if ( root == nil_and_tail ) {
        return;
    }
    // Print the root node
    printf( " " );
    print_node( root, nil_and_tail, style );

    // Print any subtrees
    if ( root->links[R] == nil_and_tail ) {
        print_inner_tree( root->links[L], nil_and_tail, "", LEAF, style );
    } else if ( root->links[L] == nil_and_tail ) {
        print_inner_tree( root->links[R], nil_and_tail, "", LEAF, style );
    } else {
        print_inner_tree( root->links[R], nil_and_tail, "", BRANCH, style );
        print_inner_tree( root->links[L], nil_and_tail, "", LEAF, style );
    }
}

/// @brief print_alloc_block  prints the contents of an allocated block of memory.
/// @param *node              a valid struct rb_node to a block of allocated memory.
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
    // We should be able to see the header is the same as the footer. However, due
    // to fixup functions, the color may change for nodes and color is irrelevant
    // to footers.
    header to_print = *footer;
    if ( get_size( *footer ) != get_size( node->header ) ) {
        to_print = ULLONG_MAX;
    }
    // How far indented the Header field normally is for all blocks.
    short indent_struct_fields = PRINTER_INDENT;
    get_color( node->header ) == BLACK ? printf( COLOR_BLK ) : printf( COLOR_RED );
    printf( "%p: HDR->0x%016zX(%zubytes)\n", node, node->header, block_size );
    printf( "%*c", indent_struct_fields, ' ' );

    // Printing color logic will help us spot red black violations. Tree printing
    // later helps too.
    if ( node->parent ) {
        printf( get_color( node->parent->header ) == BLACK ? COLOR_BLK : COLOR_RED );
        printf( "PRN->%p\n", node->parent );
    } else {
        printf( "PRN->%p\n", NULL );
    }
    printf( COLOR_NIL );
    printf( "%*c", indent_struct_fields, ' ' );
    if ( node->links[L] ) {
        printf( get_color( node->links[L]->header ) == BLACK ? COLOR_BLK : COLOR_RED );
        printf( "LFT->%p\n", node->links[L] );
    } else {
        printf( "LFT->%p\n", NULL );
    }
    printf( COLOR_NIL );
    printf( "%*c", indent_struct_fields, ' ' );
    if ( node->links[R] ) {
        printf( get_color( node->links[R]->header ) == BLACK ? COLOR_BLK : COLOR_RED );
        printf( "RGT->%p\n", node->links[R] );
    } else {
        printf( "RGT->%p\n", NULL );
    }

    // The next and footer fields may not match the current node's color bit, and
    // that is ok. we will only worry about the next node's color when we delete a
    // duplicate.
    printf( COLOR_NIL );
    printf( "%*c", indent_struct_fields, ' ' );
    printf( "LST->%p\n", node->list_start ? node->list_start : NULL );
    printf( "%*c", indent_struct_fields, ' ' );
    printf( "FTR->0x%016zX\n", to_print );
}

/// @brief print_error_block  prints a helpful error message if a block is corrupted.
/// @param *header            a header to a block of memory.
/// @param full_size          the full size of a block of memory, not just the user block size.
static void print_error_block( const struct rb_node *node, size_t block_size )
{
    printf( "\n%p: HDR->0x%016zX->%zubyts\n", node, node->header, block_size );
    printf( "Block size is too large and header is corrupted.\n" );
}

/// @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
///                        notice where we went wrong and what the addresses were.
/// @param *curr           the current node that is likely garbage values that don't make sense.
/// @param *prev           the previous node that we jumped from.
/// @param *root           the root node to begin at for printing recursively.
/// @param *nil_and_tail   address of a sentinel node serving as both list tail and black nil.
static void print_bad_jump( const struct rb_node *curr, const struct rb_node *prev, struct rb_node *root,
                            void *nil_and_tail )
{
    size_t prev_size = get_size( prev->header );
    size_t cur_size = get_size( curr->header );
    printf( "A bad jump from the value of a header has occured. Bad distance to "
            "next header.\n" );
    printf( "The previous address: %p:\n", prev );
    printf( "\tHeader Hex Value: %016zX:\n", prev->header );
    printf( "\tBlock Byte Value: %zubytes:\n", prev_size );
    printf( "\nJump by %zubytes...\n", prev_size );
    printf( "The current address: %p:\n", curr );
    printf( "\tHeader Hex Value: 0x%016zX:\n", curr->header );
    printf( "\tBlock Byte Value: %zubytes:\n", cur_size );
    printf( "\nJump by %zubytes...\n", cur_size );
    printf( "Current state of the free tree:\n" );
    print_rb_tree( root, nil_and_tail, VERBOSE );
}

/// @brief print_all    prints our the complete status of the heap, all of its blocks,
///                     and the sizes the blocks occupy. Printing should be clean with no overlap
///                     of unique id's between heap blocks or corrupted headers.
/// @param hr           start and end of the heap
/// @param heap_size    the size in bytes of the
/// @param *root        the root of the tree we start at for printing.
/// @param *black_nil   the sentinel node that waits at the bottom of the tree for all leaves.
static void print_all( struct heap_range r, size_t heap_size, struct rb_node *tree_root, struct rb_node *black_nil )
{
    struct rb_node *node = r.start;
    printf( "Heap client segment starts at address %p, ends %p. %zu total bytes "
            "currently used.\n",
            node, r.end, heap_size );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "COLOR KEY: " COLOR_BLK "[BLACK NODE] " COLOR_NIL COLOR_RED "[rED NODE] " COLOR_NIL COLOR_GRN
            "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "%p: START OF  HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", r.start );
    struct rb_node *prev = node;
    while ( node != r.end ) {
        size_t full_size = get_size( node->header );

        if ( full_size == 0 ) {
            print_bad_jump( node, prev, tree_root, black_nil );
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
    printf( "COLOR KEY: " COLOR_BLK "[BLACK NODE] " COLOR_NIL COLOR_RED "[rED NODE] " COLOR_NIL COLOR_GRN
            "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "\nRED BLACK TREE OF FREE NODES AND BLOCK SIZES.\n" );
    printf( "HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n" );
    printf( COLOR_CYN "(+X)" COLOR_NIL );
    printf( " INDICATES DUPLICATE NODES IN THE TREE. THEY HAVE A N NODE.\n" );
    print_rb_tree( tree_root, black_nil, VERBOSE );
}

// NOLINTEND(misc-no-recursion)