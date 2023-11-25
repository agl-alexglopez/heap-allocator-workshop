/// Author: Alexander Griffin Lopez
/// File: splaytree.c aka splaytree_bottomup.c
/// ------------------------------------------
/// This file contains my implementation of a splay tree heap allocator. A splay tree
/// is an interesting data structure to support the free nodes of a heap allocator
/// because perhaps we can benefit from the frequencies in access patterns. This is
/// just experimental, as I do not often here splay trees enter the discussion when
/// considering allocator schemes. This is a bottom up splay tree that does not use
/// a parent pointer, instead using a stack to track the history of a path to a node
/// that will be splayed to the root and removed or inserted and splayed. We need the
/// space a parent pointer would take up to track duplicate nodes of the same size in
/// order to support coalescing. For more information see the detailed writeup.
/// Citations:
/// ------------------------------------------
/// 1. Bryant and O'Hallaron, Computer Systems: A Programmer's Perspective, Chapter 9.
///    I used the explicit free list outline from the textbook, specifically
///    regarding how to implement left and right coalescing. I even used their
///    suggested optimization of an extra control bit so that the footers to the left
///    can be overwritten if the block is allocated so the user can have more space.
/// 2. Algorithm Tutors. https://algorithmtutor.com/Data-Structures/Tree/Splay-Trees/
///    The pseudocode is a good jumping off point for splay trees. However, significant
///    modification is required to abandon the parent pointer, contextualize the data
///    structure to a heap allocator, support coalescing, and optimize for duplicate nodes.
#include "allocator.h"
#include "debug_break.h"
#include "print_utility.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef size_t header;

/// The children of a node in a tree can be an array *nodes[2]. I prefer to
/// code binary trees this way because when the opportunity arrises you
/// can unite symmetric code cases for Left and Right into one piece of code
/// that uses a `tree_link dir = parent->links[R] == cur;` approach. Then
/// you have a `dir` and a `!dir` without needing to write code for each case
/// of the dir being left or right, just invert the direction you stored.
enum tree_link
{
    L = 0,
    R = 1
};

/// Duplicate nodes of the same size are attached to the representitive head
/// node in the tree. They are organized with a Previous and Next scheme to
/// avoid confusion and make it type-clear that we are dealing with list nodes
/// not tree nodes. This requires some limited and intentional casting but
/// is worth it for clarity/sanity while coding up these complicated cases.
enum list_link
{
    P = 0,
    N = 1
};

/// A node in this splay tree does not track its parent so we will manage a stack.
/// We also have the additional optimization of tracking duplicates in a linked
/// list of duplicate_node members. We can access that list via the list_start.
/// If the list is empty it will point to our placeholder node that acts as nil.
struct node
{
    header header;
    struct node *links[2];
    struct duplicate_node *list_start;
};

/// A node in the duplicate_node list contains the same number of payload bytes
/// as its duplicate in the tree. However, these nodes only point to duplicate
/// nodes to their left or right. The head node in this list after the node
/// in the tree is responsible for tracking the parent of the node in the tree.
/// This helps in the case of a tree node being coalesced or removed from the
/// tree. The duplicate can take its place, update the parent of the new node
/// as its child and no splaying operations take place saving many instructions.
struct duplicate_node
{
    header header;
    struct duplicate_node *links[2];
    struct node *parent;
};

/// When a splay tree node is removed we need to splay the node to the root, split
/// its lesser and greater arbitrary subtrees and then join those remaining subtrees
/// to form a new tree without the removed node. Track those subtrees with this.
struct tree_pair
{
    struct node *lesser;
    struct node *greater;
};

/// Mainly for internal validation and bookeeping. Run over the heap with this.
struct heap_range
{
    void *start;
    void *end;
};

/// If a header is corrupted why trying to jump through the heap headers this
/// will help us catch errors.
struct bad_jump
{
    struct node *prev;
    struct node *root;
};

/// Generic struct for tracking a size in bytes and quantity that makes up those bytes.
struct size_total
{
    size_t byte_size;
    size_t count_total;
};

/// Key struct that helps couple the path with its length. Think span in C++ or Slice
/// in Rust. It is a slice of our path to a node and the length of that path and we
/// use it in any functions that will splay a node to the root.
struct path_slice
{
    struct node **nodes;
    int len;
};

/// Logarithmic properties should have us covered here but use asserts just in case.
enum
{
    MAX_TREE_HEIGHT = 64
};

#define SIZE_MASK ~0x7UL
#define BLOCK_SIZE 40UL
#define HEADERSIZE sizeof( size_t )
#define FREED 0x0UL
#define ALLOCATED 0x1UL
#define LEFT_ALLOCATED 0x2UL
#define HEAP_NODE_WIDTH 32UL
#define LEFT_FREE ~0x2UL

// NOLINTBEGIN(*-non-const-global-variables)

/// Implemented as a splay tree of free nodes. I use an explicit nil node rather
/// than NULL, similar to what CLRS recommends for a Red-Black Tree. This helps
/// with some invariant coding patterns that I like especially for the duplicate list.
static struct free_nodes
{
    struct node *root;
    // These two pointers will point to the same address. Used for clarity between tree and list.
    struct node *nil;
    struct duplicate_node *list_tail;
    size_t total;
} free_nodes;

/// Useful for internal tracking and validating of the heap to speedup development.
static struct heap
{
    void *client_start;
    void *client_end;
    size_t heap_size;
} heap;

// NOLINTEND(*-non-const-global-variables)

///////////////////////////////   Forward Declarations   //////////////////////////////

// This lets us organize many functions how we wish for readability.

static size_t roundup( size_t requested_size, size_t multiple );
static size_t get_size( header header_val );
static bool is_block_allocated( header block_header );
static bool is_left_space( const struct node *node );
static void init_header_size( struct node *node, size_t payload );
static void init_footer( struct node *node, size_t payload );
static struct node *get_right_neighbor( const struct node *current, size_t payload );
static struct node *get_left_neighbor( const struct node *node );
static void *get_client_space( const struct node *node_header );
static struct node *get_node( const void *client_space );
static void init_free_node( struct node *to_free, size_t block_size );
static void *split_alloc( struct node *free_block, size_t request, size_t block_space );
static struct node *coalesce( struct node *leftmost_node );
static void remove_head( struct node *head, struct node *lft_child, struct node *rgt_child );
static void *free_coalesced_node( void *to_coalesce );
static struct node *find_best_fit( size_t key );
static void insert_node( struct node *current );
static void splay( struct node *cur, struct path_slice p );
static struct node *join( struct tree_pair pair, struct path_slice p );
static struct tree_pair split( struct node *remove, struct path_slice p );
static void add_duplicate( struct node *head, struct duplicate_node *add, struct node *parent );
static struct node *delete_duplicate( struct node *head );
static void rotate( enum tree_link rotation, struct node *current, struct path_slice p );
static bool check_init( struct heap_range r, size_t heap_size );
static bool is_memory_balanced( size_t *total_free_mem, struct heap_range r, struct size_total s );
static size_t extract_tree_mem( const struct node *root, const void *nil_and_tail );
static bool is_tree_mem_valid( const struct node *root, const void *nil_and_tail, size_t total_free_mem );
static bool are_subtrees_valid( const struct node *root, const struct node *nil );
static bool is_duplicate_storing_parent( const struct node *parent, const struct node *root,
                                         const void *nil_and_tail );
static void print_tree( const struct node *root, const void *nil_and_tail, enum print_style style );
static void print_all( struct heap_range r, size_t heap_size, struct node *tree_root, struct node *nil );

///////////////////////////////   Shared Heap Functions   ////////////////////////////////

size_t get_free_total( void ) { return free_nodes.total; }

bool myinit( void *heap_start, size_t heap_size )
{
    size_t client_request = roundup( heap_size, ALIGNMENT );
    if ( client_request < BLOCK_SIZE ) {
        return false;
    }
    heap.client_start = heap_start;
    heap.heap_size = client_request;
    heap.client_end = (uint8_t *)heap.client_start + heap.heap_size - HEAP_NODE_WIDTH;

    // This helps us clarify if we are referring to tree or duplicates in a list. Use same address.
    free_nodes.list_tail = heap.client_end;
    free_nodes.nil = heap.client_end;
    free_nodes.nil->header = 1UL;
    free_nodes.root = heap.client_start;
    init_header_size( free_nodes.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    init_footer( free_nodes.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    free_nodes.root->links[L] = free_nodes.nil;
    free_nodes.root->links[R] = free_nodes.nil;
    free_nodes.root->list_start = free_nodes.list_tail;
    free_nodes.total = 1;
    return true;
}

void *mymalloc( size_t requested_size )
{
    if ( requested_size == 0 || requested_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    size_t client_request = roundup( requested_size + HEAP_NODE_WIDTH, ALIGNMENT );
    // Search the tree for the best possible fitting node.
    struct node *found_node = find_best_fit( client_request );
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
    struct node *old_node = get_node( old_ptr );
    size_t old_size = get_size( old_node->header );

    struct node *leftmost_node = coalesce( old_node );
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
    struct node *to_insert = get_node( ptr );
    to_insert = coalesce( to_insert );
    init_free_node( to_insert, get_size( to_insert->header ) );
}

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
    if ( !is_tree_mem_valid( free_nodes.root, free_nodes.nil, total_free_mem ) ) {
        return false;
    }
    if ( !are_subtrees_valid( free_nodes.root, free_nodes.nil ) ) {
        return false;
    }
    if ( !is_duplicate_storing_parent( free_nodes.nil, free_nodes.root, free_nodes.nil ) ) {
        return false;
    }
    return true;
}

//////////////////////////       Printing Public Helpers    //////////////////////////////////

/// @note  the red and blue links represent the heavy/light decomposition of a splay tree. For more
///        information on this interpretation see any Stanford 166 lecture on splay trees.
void print_free_nodes( enum print_style style )
{
    printf( "%s(X)%s", COLOR_CYN, COLOR_NIL );
    printf( " Indicates number of nodes in the subtree rooted at X.\n" );
    printf( "%sBlue%s edge means total nodes rooted at X %s<=%s ((number of nodes rooted at Parent) / 2).\n",
            COLOR_BLU_BOLD, COLOR_NIL, COLOR_BLU_BOLD, COLOR_NIL );
    printf( "%sRed%s edge means total nodes rooted at X %s>%s ((number of nodes rooted at Parent) / 2).\n",
            COLOR_RED_BOLD, COLOR_NIL, COLOR_RED_BOLD, COLOR_NIL );
    printf( "This is the %sheavy%s/%slight%s decomposition of a Splay Tree.\n", COLOR_RED_BOLD, COLOR_NIL,
            COLOR_BLU_BOLD, COLOR_NIL );
    printf( "%s(+X)%s", COLOR_CYN, COLOR_NIL );
    printf( " Indicates duplicate nodes in the tree linked by a doubly-linked list.\n" );
    print_tree( free_nodes.root, free_nodes.nil, style );
}

void dump_heap( void )
{
    print_all( ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size, free_nodes.root,
               free_nodes.nil );
}

/////////////////////    Static Heap Helper Functions    //////////////////////////////////

/// @brief coalesce        attempts to coalesce left and right if the left and right node
///                        are free.
/// @param *leftmost_node  the current node that will move left if left is free to coalesce.
/// @return                the leftmost node from attempts to coalesce left and right. The leftmost
///                        node is initialized to reflect the correct size for the space it now has.
/// @warning               this function does not overwrite the data that may be in the middle if we
///                        expand left and right. The user may wish to move elsewhere if reallocing.
static struct node *coalesce( struct node *leftmost_node )
{
    size_t coalesced_space = get_size( leftmost_node->header );
    struct node *rightmost_node = get_right_neighbor( leftmost_node, coalesced_space );

    // The nil is the right boundary. We set it to always be allocated with size 0.
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

    // We do not initialize a footer here because we don't want to overwrite user data.
    init_header_size( leftmost_node, coalesced_space );
    return leftmost_node;
}

/// @brief free_coalesced_node  a specialized version of node freeing when we find a neighbor we
///                             need to free from the tree before absorbing into our coalescing. If
///                             this node is a duplicate we can splice it from a linked list.
/// @param *to_coalesce         the address for a node we must treat as a list or tree node.
/// @return                     the node we have now correctly freed given all cases to find it.
static void *free_coalesced_node( void *to_coalesce )
{
    struct node *tree_node = to_coalesce;
    // Go find and fix the node the normal way if it is unique.
    if ( tree_node->list_start == free_nodes.list_tail ) {
        return find_best_fit( get_size( tree_node->header ) );
    }
    struct duplicate_node *list_node = to_coalesce;
    struct node *lft_tree_node = tree_node->links[L];

    // Coalescing the first node in linked list. Dummy head, aka lft_tree_node, is to the left.
    if ( lft_tree_node != free_nodes.nil && lft_tree_node->list_start == to_coalesce ) {
        list_node->links[N]->parent = list_node->parent;
        lft_tree_node->list_start = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

        // All nodes besides the tree node head and the first duplicate node have parent set to NULL.
    } else if ( NULL == list_node->parent ) {
        list_node->links[P]->links[N] = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

        // Coalesce head of a doubly linked list in the tree. Remove and make a new head.
    } else {
        remove_head( tree_node, lft_tree_node, tree_node->links[R] );
    }
    free_nodes.total--;
    return to_coalesce;
}

/// @brief remove_head  frees the head of a doubly linked list of duplicates which is a node in a
///                     red black tree. The next duplicate must become the new tree node and the
///                     parent and children must be adjusted to track this new node.
/// @param head         the node in the tree that must now be coalesced.
/// @param lft_child    if the left child has a duplicate, it tracks the new tree node as parent.
/// @param rgt_child    if the right child has a duplicate, it tracks the new tree node as parent.
static void remove_head( struct node *head, struct node *lft_child, struct node *rgt_child )
{
    // Store the parent in an otherwise unused field for a major O(1) coalescing speed boost.
    struct node *tree_parent = head->list_start->parent;
    head->list_start->header = head->header;
    head->list_start->links[N]->parent = head->list_start->parent;

    struct node *new_tree_node = (struct node *)head->list_start;
    new_tree_node->list_start = head->list_start->links[N];
    new_tree_node->links[L] = lft_child;
    new_tree_node->links[R] = rgt_child;

    // We often write to fields of the nil, invariant. DO NOT use the nodes it stores!
    if ( lft_child != free_nodes.nil ) {
        lft_child->list_start->parent = new_tree_node;
    }
    if ( rgt_child != free_nodes.nil ) {
        rgt_child->list_start->parent = new_tree_node;
    }
    if ( tree_parent == free_nodes.nil ) {
        free_nodes.root = new_tree_node;
    } else {
        tree_parent->links[tree_parent->links[R] == head] = new_tree_node;
    }
}

/// @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
///                      split, it will add the newly freed split block to the free red black tree.
/// @param *free_block   a pointer to the node for a free block in its entirety.
/// @param request       the user request for space.
/// @param block_space   the entire space that we have to work with.
/// @return              a void pointer to generic space that is now ready for the client.
static void *split_alloc( struct node *free_block, size_t request, size_t block_space )
{
    if ( block_space >= request + BLOCK_SIZE ) {
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

/// @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
/// @param *to_free        the heap_node to add to the red black tree.
/// @param block_size      the size we use to initialize the node and find the right place in tree.
static void init_free_node( struct node *to_free, size_t block_size )
{
    to_free->header = block_size | LEFT_ALLOCATED;
    to_free->list_start = free_nodes.list_tail;
    init_footer( to_free, block_size );
    get_right_neighbor( to_free, block_size )->header &= LEFT_FREE;
    insert_node( to_free );
}

/////////////////////////////      Splay Tree Implementation       //////////////////////////////////

/// @brief find_best_fit  a splay tree is well suited to best fit search in O(logN) time. We
///                       will find the best fitting node possible given the options in our tree.
/// @param key            the size_t number of bytes we are searching for in our tree.
/// @return               the pointer to the struct node that is the best fit for our need.
static struct node *find_best_fit( size_t key )
{
    struct node *path[MAX_TREE_HEIGHT];
    path[0] = free_nodes.nil;
    int path_len = 1;
    int len_to_best_fit = 1;

    struct node *seeker = free_nodes.root;
    size_t best_fit_size = ULLONG_MAX;
    struct node *remove = seeker;
    while ( seeker != free_nodes.nil ) {
        size_t seeker_size = get_size( seeker->header );
        path[path_len++] = seeker;
        assert( path_len < MAX_TREE_HEIGHT );
        if ( key == seeker_size ) {
            remove = seeker;
            len_to_best_fit = path_len;
            break;
        }
        // The key is less than the current found size but let's remember this size on the way down
        // as a candidate for the best fit. The closest fit will have won when we reach the bottom.
        if ( seeker_size < best_fit_size && seeker_size >= key ) {
            remove = seeker;
            best_fit_size = seeker_size;
            len_to_best_fit = path_len;
        }
        seeker = seeker->links[seeker_size < key];
    }
    // While removing and we encounter a duplicate, we should still fixup the tree with a
    // splay operation before removing the duplicate. We will then leave it at the root.
    // It is possible requests for these sizes will continue making the nodes "hot". Maybe not.
    if ( remove->list_start != free_nodes.list_tail ) {
        splay( remove, ( struct path_slice ){ path, len_to_best_fit } );
        assert( remove == free_nodes.root );
        return delete_duplicate( remove );
    }
    struct tree_pair lg = split( remove, ( struct path_slice ){ path, len_to_best_fit } );
    if ( lg.lesser->links[L] != free_nodes.nil ) {
        lg.lesser->links[L]->list_start->parent = free_nodes.nil;
    }
    free_nodes.root
        = join( ( struct tree_pair ){ lg.lesser->links[L], lg.greater }, ( struct path_slice ){ path, 1 } );
    --free_nodes.total;
    return remove;
}

/// @brief split   splay a node and then split its arbitrary subtrees into lesser and greater trees in
///                in preparation for removal.
/// @param remove  the node we will splay and then split into two trees.
/// @param p       the path_slice we will use to splay the node for removal
/// @return        the tree_pair of a lesser and greater tree we will later join after node removal.
static struct tree_pair split( struct node *remove, struct path_slice p )
{
    splay( remove, p );
    struct tree_pair split = { .lesser = remove, .greater = free_nodes.nil };
    if ( remove->links[R] != free_nodes.nil ) {
        split.greater = remove->links[R];
        split.greater->list_start->parent = free_nodes.nil;
    }
    split.lesser->links[R] = free_nodes.nil;
    return split;
}

/// @brief join  when joining, we have just splayed a node we wish to remove to the root of a tree, then split.
///              We now must make a new root and join the two trees split trees. The smaller tree contains the
///              maximum we will use. This maximum then takes the larger tree as its right child.
static struct node *join( struct tree_pair pair, struct path_slice p )
{
    if ( pair.lesser == free_nodes.nil ) {
        return pair.greater;
    }
    if ( pair.greater == free_nodes.nil ) {
        return pair.lesser;
    }
    for ( struct node *seeker = pair.lesser; seeker != free_nodes.nil; seeker = seeker->links[R] ) {
        p.nodes[p.len++] = seeker;
    }
    struct node *inorder_predecessor = p.nodes[p.len - 1];
    splay( inorder_predecessor, p );
    inorder_predecessor->links[R] = pair.greater;
    pair.greater->list_start->parent = inorder_predecessor;
    return inorder_predecessor;
}

/// @brief delete_duplicate  will remove a duplicate node from the tree when the request is coming
///                          from a call from malloc. Address of duplicate does not matter so we
///                          remove the first node from the linked list.
/// @param *head             We know this node has a next node and it must be removed for malloc.
static struct node *delete_duplicate( struct node *head )
{
    struct duplicate_node *next_node = head->list_start;
    /// Take care of the possible node to the right in the doubly linked list first. This could be
    /// another node or it could be free_nodes.list_tail, it does not matter either way.
    next_node->links[N]->parent = next_node->parent;
    next_node->links[N]->links[P] = (struct duplicate_node *)head;
    head->list_start = next_node->links[N];
    --free_nodes.total;
    return (struct node *)next_node;
}

/// @brief insert_node  a modified insertion with additional logic to add duplicates if the
///                     size in bytes of the block is already in the tree.
/// @param *current     we must insert to tree or add to a list as duplicate.
static void insert_node( struct node *current )
{
    size_t current_key = get_size( current->header );
    struct node *path[MAX_TREE_HEIGHT];
    path[0] = free_nodes.nil;
    int path_len = 1;
    struct node *seeker = free_nodes.root;
    while ( seeker != free_nodes.nil ) {
        path[path_len++] = seeker;
        assert( path_len < MAX_TREE_HEIGHT );
        size_t parent_size = get_size( seeker->header );
        // The user has just requested this amount of space but it is a duplicate. However, if we move the
        // existing duplicate to the root via splaying before adding the new duplicate we benefit from
        // splay fixups and we have multiple "hot" nodes at the root available in O(1) if requested again.
        // If request patterns changes then oh well, can't win 'em all.
        if ( current_key == parent_size ) {
            splay( seeker, ( struct path_slice ){ path, path_len } );
            assert( seeker == free_nodes.root );
            add_duplicate( seeker, (struct duplicate_node *)current, free_nodes.nil );
            return;
        }
        // You may see this idiom throughout. L(0) if key fits in tree to left, R(1) if not.
        seeker = seeker->links[parent_size < current_key];
    }
    struct node *parent = path[path_len - 1];
    if ( parent == free_nodes.nil ) {
        free_nodes.root = current;
    } else {
        parent->links[get_size( parent->header ) < current_key] = current;
    }
    current->links[L] = free_nodes.nil;
    current->links[R] = free_nodes.nil;
    // Store the doubly linked duplicates in list. list_tail aka nil is the dummy tail.
    current->list_start = free_nodes.list_tail;
    path[path_len++] = current;
    splay( current, ( struct path_slice ){ path, path_len } );
    ++free_nodes.total;
}

/// @brief splay  splays a node from its location in the tree to the root. A splay is defined by all
///               combinations of the Zig and Zag operations. A Zig or Zag operation is a simple rotation
///               that rotates a child node in the opposite direction from its relationship to its parent.
///               That means we have Zag, Zig, Zig-Zig, Zag-Zag, Zig-Zag, and Zag-Zig to consider.
/// @param cur    the current node making its way up the tree.
/// @param p      the path_slice we will use to find the lineage of cur as it Zig and or Zags up the tree.
static void splay( struct node *cur, struct path_slice p )
{
    while ( p.len >= 3 && p.nodes[p.len - 2] != free_nodes.nil ) {
        struct node *gparent = p.nodes[p.len - 3];
        struct node *parent = p.nodes[p.len - 2];
        if ( gparent == free_nodes.nil ) {
            // Zig or Zag rotates in the opposite direction of the child relationship.
            rotate( !( parent->links[R] == cur ), parent, ( struct path_slice ){ p.nodes, p.len - 1 } );
            p.nodes[p.len - 2] = cur;
            --p.len;
            continue;
        }
        enum tree_link parent_to_current = cur == parent->links[R];
        enum tree_link gparent_to_parent = parent == gparent->links[R];
        // The Zag-Zag and Zig-Zig cases are symmetrical and easily united into a case of a direction and opposite.
        if ( parent_to_current == gparent_to_parent ) {
            rotate( !parent_to_current, gparent, ( struct path_slice ){ p.nodes, p.len - 2 } );
            p.nodes[p.len - 3] = parent;
            p.nodes[p.len - 2] = cur;
            rotate( !parent_to_current, parent, ( struct path_slice ){ p.nodes, p.len - 2 } );
            p.nodes[p.len - 3] = cur;
            p.len -= 2;
            continue;
        }
        // Zig-Zag and Zag-Zig branches are also easily united. Think of the shapes we have.
        //  gparent          gparent       gparent         gparent        current             current
        //   \              /              \               /               |    |             |     |
        //    parent   or   parent     ->   current   or  current   -> gparent parent or  parent   gparent
        //   /              \                \           /
        //  current          current          parent     parent
        // We want the parent-child link to rotate the same direction as the grandparent-parent link
        // and then for the gparent-rotatedchild link to rotate the same direction as the original parent-child.
        rotate( gparent_to_parent, parent, ( struct path_slice ){ p.nodes, p.len - 1 } );
        p.nodes[p.len - 2] = cur;
        rotate( parent_to_current, gparent, ( struct path_slice ){ p.nodes, p.len - 2 } );
        p.nodes[p.len - 3] = cur;
        p.len -= 2;
    }
}

/// @brief rotate    a unified version of the traditional left and right rotation functions. The
///                  rotation is either left or right and opposite is its opposite direction. We
///                  take the current nodes child, and swap them and their arbitrary subtrees are
///                  re-linked correctly depending on the direction of the rotation.
/// @param *current  the node around which we will rotate.
/// @param rotation  either left or right. Determines the rotation and its opposite direction.
/// @warning         this function modifies the stack.
static void rotate( enum tree_link rotation, struct node *current, struct path_slice p )
{
    if ( p.len < 2 ) {
        printf( "Path length is %d but request for path len %d - 2 was made.", p.len, p.len );
        abort();
    }
    struct node *parent = p.nodes[p.len - 2];
    struct node *child = current->links[!rotation];
    current->links[!rotation] = child->links[rotation];
    if ( child->links[rotation] != free_nodes.nil ) {
        child->links[rotation]->list_start->parent = current;
    }
    if ( child != free_nodes.nil ) {
        child->list_start->parent = parent;
    }
    if ( parent == free_nodes.nil ) {
        free_nodes.root = child;
    } else {
        parent->links[parent->links[R] == current] = child;
    }
    child->links[rotation] = current;
    current->list_start->parent = child;
}

/// @brief add_duplicate  this implementation stores duplicate nodes in a linked list to prevent the
///                       rotation of duplicates in the tree. This adds the duplicate node to the
///                       linked list of the node already present.
/// @param *head          the node currently organized in the tree. We will add to its list.
/// @param *to_add        the node to add to the linked list.
static void add_duplicate( struct node *head, struct duplicate_node *add, struct node *parent )
{
    add->header = head->header;
    // The first node in the list can store the tree nodes parent for faster coalescing later.
    if ( head->list_start == free_nodes.list_tail ) {
        add->parent = parent;
    } else {
        add->parent = head->list_start->parent;
        head->list_start->parent = NULL;
    }
    // Get the first next node in the doubly linked list, invariant and correct its left field.
    head->list_start->links[P] = add;
    add->links[N] = head->list_start;
    head->list_start = add;
    add->links[P] = (struct duplicate_node *)head;
    ++free_nodes.total;
}

/////////////////////////////   Basic Block and Header Operations  //////////////////////////////////

/// @brief roundup         rounds up size to the nearest multiple of two to be aligned in the heap.
/// @param requested_size  size given to us by the client.
/// @param multiple        the nearest multiple to raise our number to.
/// @return                rounded number.
static inline size_t roundup( size_t requested_size, size_t multiple )
{
    return ( requested_size + multiple - 1 ) & ~( multiple - 1 );
}

/// @brief get_size    returns size in bytes as a size_t from the value of node's header.
/// @param header_val  the value of the node in question passed by value.
/// @return            the size in bytes as a size_t of the node.
static inline size_t get_size( header header_val ) { return SIZE_MASK & header_val; }

/// @brief is_block_allocated  determines if a node is allocated or free.
/// @param block_header        the header value of a node passed by value.
/// @return                    true if allocated false if not.
static inline bool is_block_allocated( const header block_header ) { return block_header & ALLOCATED; }

/// @brief is_left_space  determines if the left neighbor of a block is free or allocated.
/// @param *node          the node to check.
/// @return               true if left is free false if left is allocated.
static inline bool is_left_space( const struct node *node ) { return !( node->header & LEFT_ALLOCATED ); }

/// @brief init_header_size  initializes any node as the size and indicating left is allocated.
///                          Left is allocated because we always coalesce left and right.
/// @param *node             the region of possibly uninitialized heap we must initialize.
/// @param payload           the payload in bytes as a size_t of the current block we initialize
static inline void init_header_size( struct node *node, size_t payload )
{
    node->header = LEFT_ALLOCATED | payload;
}

/// @brief init_footer  initializes footer at end of the heap block to matcht the current header.
/// @param *node        the current node with a header field we will use to set the footer.
/// @param payload      the size of the current nodes free memory.
static inline void init_footer( struct node *node, size_t payload )
{
    header *footer = (header *)( (uint8_t *)node + payload );
    *footer = node->header;
}

/// @brief get_right_neighbor  gets the address of the next struct node in the heap to the right.
/// @param *current            the struct node we start at to then jump to the right.
/// @param payload             the size in bytes as a size_t of the current struct node block.
/// @return                    the struct node to the right of the current.
static inline struct node *get_right_neighbor( const struct node *current, size_t payload )
{
    return (struct node *)( (uint8_t *)current + HEADERSIZE + payload );
}

/// @brief *get_left_neighbor  uses the left block size gained from the footer to move to the header.
/// @param *node               the current header at which we reside.
/// @return                    a header pointer to the header for the block to the left.
static inline struct node *get_left_neighbor( const struct node *node )
{
    header *left_footer = (header *)( (uint8_t *)node - HEADERSIZE );
    return (struct node *)( (uint8_t *)node - ( *left_footer & SIZE_MASK ) - HEADERSIZE );
}

/// @brief get_client_space  steps into the client space just after the header of a node.
/// @param *node_header      the struct node we start at before retreiving the client space.
/// @return                  the void address of the client space they are now free to use.
static inline void *get_client_space( const struct node *node_header )
{
    return (uint8_t *)node_header + HEADERSIZE;
}

/// @brief get_struct node    steps to the struct node header from the space the client was using.
/// @param *client_space  the void the client was using for their type. We step to the left.
/// @return               a pointer to the struct node of our heap block.
static inline struct node *get_node( const void *client_space )
{
    return (struct node *)( (uint8_t *)client_space - HEADERSIZE );
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
/// @param hr                  start and end of the heap
/// @param s                   size of the heap memory and total free nodes.
/// @return                    true if our tallying is correct and our totals match.
static bool is_memory_balanced( size_t *total_free_mem, struct heap_range r, struct size_total s )
{
    // Check that after checking all headers we end on size 0 tail and then end of
    // address space.
    struct node *cur_node = r.start;
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

/// @brief extract_tree_mem  sums the total memory in the red black tree to see if it matches
///                          the total memory we got from traversing blocks of the heap.
/// @param *root             the root to start at for the summing recursive search.
/// @param *nil_and_tail     the address of a sentinel node serving as both list tail and black nil.
/// @return                  the total memory in bytes as a size_t in the red black tree.
static size_t extract_tree_mem( const struct node *root, const void *nil_and_tail )
{
    if ( root == nil_and_tail ) {
        return 0UL;
    }
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
static bool is_tree_mem_valid( const struct node *root, const void *nil_and_tail, size_t total_free_mem )
{
    if ( total_free_mem != extract_tree_mem( root, nil_and_tail ) ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

/// @brief strict_bounds_met  all nodes to the left of the root of a binary tree must be strictly less than the
///                           root. All nodes to the right must be strictly greater than the root. If you mess
///                           rotations you may have a valid binary tree in terms of a root and its two direct
///                           children but are violating some bound on a root further up the tree.
/// @param root               the recursive root we will check as we descend.
/// @param root_size          the original root size to which all nodes in the subtrees must obey.
/// @param dir                if we check the right subtree all nodes are greater, left subtree lesser.
/// @param nil                the nil of the tree. could be NULL or some dedicated address.
static bool strict_bound_met( const struct node *root, size_t root_size, enum tree_link dir,
                              const struct node *nil )
{
    if ( root == nil ) {
        return true;
    }
    size_t node_size = get_size( root->header );
    if ( dir == L && node_size > root_size ) {
        BREAKPOINT();
        return false;
    }
    if ( dir == R && node_size < root_size ) {
        BREAKPOINT();
        return false;
    }
    return strict_bound_met( root->links[L], root_size, dir, nil )
           && strict_bound_met( root->links[R], root_size, dir, nil );
}

/// @brief are_subtrees_valid  fully checks the size of all subtrees to the left and right of the current node.
///                            There must not be a node lesser than the root size in the right subtrees and no node
///                            exceeding the root size in the left subtree.
/// @param root                the recursive root we are checking as we traverse the tree dfs-style.
/// @param nil                 the nil of the tree either NULL or a dedicated address.
static bool are_subtrees_valid( const struct node *root, const struct node *nil )
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

/// @brief is_parent_valid  for duplicate node operations it is important to check the parents
///                         and fields are updated corectly so we can continue using the tree.
/// @param *parent          the parent of the current root.
/// @param *root            the root to start at for the recursive search.
/// @param *nil_and_tail    address of a sentinel node serving as both list tail and black nil.
/// @return                 true if all parent child relationships are correct.
static bool is_duplicate_storing_parent( const struct node *parent, const struct node *root,
                                         const void *nil_and_tail )
{
    if ( root == nil_and_tail ) {
        return true;
    }
    if ( root->list_start != nil_and_tail && root->list_start->parent != parent ) {
        BREAKPOINT();
        return false;
    }
    return is_duplicate_storing_parent( root, root->links[L], nil_and_tail )
           && is_duplicate_storing_parent( root, root->links[R], nil_and_tail );
}

/////////////////////////////        Printing Functions            //////////////////////////////////

static size_t get_subtree_size( const struct node *root )
{
    if ( root == free_nodes.nil ) {
        return 0;
    }
    return 1 + get_subtree_size( root->links[L] ) + get_subtree_size( root->links[R] );
}

static const char *get_edge_color( const struct node *root, size_t parent_size )
{
    if ( root == free_nodes.nil ) {
        return "";
    }
    return get_subtree_size( root ) <= parent_size / 2 ? COLOR_BLU_BOLD : COLOR_RED_BOLD;
}

/// @brief print_node     prints an individual node in its color and status as left or right child.
/// @param *root          the root we will print with the appropriate info.
/// @param *nil_and_tail  address of a sentinel node serving as both list tail and black nil.
/// @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
static void print_node( const struct node *root, const void *nil_and_tail, enum print_style style )
{
    size_t block_size = get_size( root->header );
    if ( style == VERBOSE ) {
        printf( "%p:", root );
    }
    printf( "(%zubytes)", block_size );
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
/// @param *prefix           the string we print spacing and characters across recursive calls.
/// @param node_type         the node to print can either be a leaf or internal branch.
/// @param dir               no parent field so we need to track where we came from.
/// @param style             the print style: PLAIN or VERBOSE(displays memory addresses).
/// @warning                 this function is hideous/slow but it prints the edge colors correctly.
static void print_inner_tree( const struct node *root, size_t parent_size, const char *prefix,
                              const char *prefix_branch_color, const enum print_link node_type,
                              const enum tree_link dir, enum print_style style )
{
    if ( root == free_nodes.nil ) {
        return;
    }
    size_t subtree_size = get_subtree_size( root );
    printf( "%s", prefix );
    printf( "%s%s%s", subtree_size <= parent_size / 2 ? COLOR_BLU_BOLD : COLOR_RED_BOLD,
            node_type == LEAF ? " └──" : " ├──", COLOR_NIL );
    printf( COLOR_CYN );
    printf( "(%zu)", subtree_size );
    dir == L ? printf( "L:" COLOR_NIL ) : printf( "R:" COLOR_NIL );
    print_node( root, free_nodes.nil, style );

    char *str = NULL;
    int string_length = snprintf( NULL, 0, "%s%s%s", prefix, prefix_branch_color, // NOLINT
                                  node_type == LEAF ? "     " : " │   " );
    if ( string_length > 0 ) {
        str = malloc( string_length + 1 );
        (void)snprintf( str, string_length, "%s%s%s", prefix, prefix_branch_color, // NOLINT
                        node_type == LEAF ? "     " : " │   " );
    }
    if ( str == NULL ) {
        printf( COLOR_ERR "memory exceeded. Cannot display tree." COLOR_NIL );
        return;
    }

    // With this print style the only continuing prefix we need colored is the left, the unicode vertical bar.
    const char *left_edge_color = get_edge_color( root->links[L], subtree_size );
    if ( root->links[R] == free_nodes.nil ) {
        print_inner_tree( root->links[L], subtree_size, str, left_edge_color, LEAF, L, style );
    } else if ( root->links[L] == free_nodes.nil ) {
        print_inner_tree( root->links[R], subtree_size, str, left_edge_color, LEAF, R, style );
    } else {
        print_inner_tree( root->links[R], subtree_size, str, left_edge_color, BRANCH, R, style );
        print_inner_tree( root->links[L], subtree_size, str, left_edge_color, LEAF, L, style );
    }
    free( str );
}

/// @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
/// @param *root          the root node to begin at for printing recursively.
/// @param *nil_and_tail  address of a sentinel node serving as both list tail and black nil.
/// @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
static void print_tree( const struct node *root, const void *nil_and_tail, enum print_style style )
{
    if ( root == nil_and_tail ) {
        return;
    }
    size_t subtree_size = get_subtree_size( root );
    printf( "%s(%zu)%s", COLOR_CYN, subtree_size, COLOR_NIL );
    print_node( root, nil_and_tail, style );

    // With this print style the only continuing prefix we need colored is the left.
    const char *left_edge_color = get_edge_color( root->links[L], subtree_size );
    if ( root->links[R] == nil_and_tail ) {
        print_inner_tree( root->links[L], subtree_size, "", left_edge_color, LEAF, L, style );
    } else if ( root->links[L] == nil_and_tail ) {
        print_inner_tree( root->links[R], subtree_size, "", left_edge_color, LEAF, R, style );
    } else {
        print_inner_tree( root->links[R], subtree_size, "", left_edge_color, BRANCH, R, style );
        print_inner_tree( root->links[L], subtree_size, "", left_edge_color, LEAF, L, style );
    }
}

/// @brief print_alloc_block  prints the contents of an allocated block of memory.
/// @param *node              a valid struct node to a block of allocated memory.
static void print_alloc_block( const struct node *node )
{
    size_t block_size = get_size( node->header );
    // We will see from what direction our header is messed up by printing 16
    // digits.
    printf( COLOR_GRN "%p: HDR->0x%016zX(%zubytes)\n" COLOR_NIL, node, node->header, block_size );
}

/// @brief print_free_block  prints the contents of a free block of heap memory.
/// @param *node             a valid header to a block of allocated memory.
static void print_free_block( const struct node *node )
{
    size_t block_size = get_size( node->header );
    header *footer = (header *)( (uint8_t *)node + block_size );
    // We should be able to see the header is the same as the footer. However, due
    // to fixup functions, the color may change for nodes and color is irrelevant to footers.
    header to_print = *footer;
    if ( get_size( *footer ) != get_size( node->header ) ) {
        to_print = ULLONG_MAX;
    }
    short indent_struct_fields = PRINTER_INDENT;
    printf( "%p: HDR->0x%016zX(%zubytes)\n", node, node->header, block_size );
    printf( "%*c", indent_struct_fields, ' ' );
    if ( node->links[L] ) {
        printf( "LFT->%p\n", node->links[L] );
    } else {
        printf( "LFT->%p\n", NULL );
    }
    printf( COLOR_NIL );
    printf( "%*c", indent_struct_fields, ' ' );
    if ( node->links[R] ) {
        printf( "RGT->%p\n", node->links[R] );
    } else {
        printf( "RGT->%p\n", NULL );
    }
    printf( "%*c", indent_struct_fields, ' ' );
    printf( "LST->%p\n", node->list_start ? node->list_start : NULL );
    printf( "%*c", indent_struct_fields, ' ' );
    printf( "FTR->0x%016zX\n", to_print );
}

/// @brief print_error_block  prints a helpful error message if a block is corrupted.
/// @param *header            a header to a block of memory.
/// @param full_size          the full size of a block of memory, not just the user block size.
static void print_error_block( const struct node *node, size_t block_size )
{
    printf( "\n%p: HDR->0x%016zX->%zubyts\n", node, node->header, block_size );
    printf( "Block size is too large and header is corrupted.\n" );
}

/// @brief print_bad_jump  If we overwrite data in a header, this print statement will help
///                        us notice where we went wrong and what the addresses were.
/// @param *current        the current node that is likely garbage values that don't make sense.
/// @param *prev           the previous node that we jumped from.
/// @param *root           the root node to begin at for printing recursively.
/// @param *nil_and_tail   address of a sentinel node serving as both list tail and black nil.
static void print_bad_jump( const struct node *current, struct bad_jump j, const void *nil_and_tail )
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
    print_tree( j.root, nil_and_tail, VERBOSE );
}

/// @brief print_all    prints our the complete status of the heap, all of its blocks, and
///                     the sizes the blocks occupy. Printing should be clean with no overlap
///                     of unique id's between heap blocks or corrupted headers.
/// @param hr           start and end of the heap
/// @param heap_size    the size in bytes of the
/// @param *root        the root of the tree we start at for printing.
/// @param *nil   the sentinel node that waits at the bottom of the tree for all leaves.
static void print_all( struct heap_range r, size_t heap_size, struct node *tree_root, struct node *nil )
{
    struct node *node = r.start;
    printf( "Heap client segment starts at address %p, ends %p. %zu total bytes "
            "currently used.\n",
            node, r.end, heap_size );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "COLOR KEY: " COLOR_GRN "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "%p: START OF  HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", r.start );
    struct node *prev = node;
    while ( node != r.end ) {
        size_t full_size = get_size( node->header );

        if ( full_size == 0 ) {
            print_bad_jump( node, ( struct bad_jump ){ prev, tree_root }, nil );
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
    printf( "%p: NIL HDR->0x%016zX\n", nil, nil->header );
    printf( "%p: FINAL ADDRESS", (uint8_t *)r.end + HEAP_NODE_WIDTH );
    printf( "\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "COLOR KEY: " COLOR_GRN "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "\nSPLAY TREE OF FREE NODES AND BLOCK SIZES.\n" );
    printf( "HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n" );
    printf( COLOR_CYN "(+X)" COLOR_NIL );
    printf( " INDICATES DUPLICATE NODES IN THE TREE. THEY HAVE A N NODE.\n" );
    print_tree( tree_root, nil, VERBOSE );
}

// NOLINTEND(misc-no-recursion)
