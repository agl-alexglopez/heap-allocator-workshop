/// Author: Alexander Griffin Lopez
/// File: splaytree_topdown.c
/// ------------------------------------------
/// This file contains my implementation of a splay tree heap allocator. A splay tree
/// is an interesting data structure to support the free nodes of a heap allocator
/// because perhaps we can benefit from the frequencies in access patterns. This is
/// just experimental, as I do not often here splay trees enter the discussion when
/// considering allocator schemes. This is a topdown splay tree implementation based
/// on the work of Daniel Sleator (Carnegie Mellon University). There are significant
/// modifications to consider in the context of a heap allocator, however. Firstly,
/// splay trees do not support duplicates yet we must do so for a heap allocator.
/// Secondly, we must track parents even though topdown splay trees do not require
/// us to do so. We will save space by combining the duplicate logic with the parent
/// tracking logic. This is required because of coalescing and the possibility of
/// a child node changing as a result of a coalesce requiring an update for the parent.
/// There are many optimizations focused on speeding up coalescing and handling duplicates.
/// See the detailed writeup for more information or explore the code.
/// Citations:
/// ------------------------------------------
/// 1. Bryant and O'Hallaron, Computer Systems: A Programmer's Perspective, Chapter 9.
///    I used the explicit free list outline from the textbook, specifically
///    regarding how to implement left and right coalescing. I even used their
///    suggested optimization of an extra control bit so that the footers to the left
///    can be overwritten if the block is allocated so the user can have more space.
/// 2. Daniel Sleator, Carnegie Mellon University. Sleator's implementation of a
///    topdown splay tree was instrumental in starting things off, but required
///    extensive modification. I had to add the ability to track duplicates, update
///    parent and child tracking, and unite the left and right cases for fun. See
///    the code for a generalizable strategy to eliminate symmetric left and right
///    cases for any binary tree code.
///    https://www.link.cs.cmu.edu/link/ftp-site/splaying/top-down-splay.c
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

/// A node in this splay tree does not track its parent. We also have the additional
/// optimization of tracking duplicates in a linked list of duplicate_node members.
/// We can access that list via the list_start. If the list is empty it will point
/// to our placeholder node that acts as nil.
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
    size_t size;
    size_t total;
};

struct coalesce_report
{
    struct node *left;
    struct node *current;
    struct node *right;
    size_t available;
};

#define SIZE_MASK ~0x7UL
#define HEADERSIZE sizeof( size_t )
#define BLOCK_SIZE ( sizeof( struct node ) + HEADERSIZE )
#define FREED 0x0UL
#define ALLOCATED 0x1UL
#define LEFT_ALLOCATED 0x2UL
#define HEAP_NODE_WIDTH sizeof( struct node )
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
static void give_parent_subtree( struct node *parent, enum tree_link dir, struct node *subtree );
static void init_free_node( struct node *to_free, size_t block_size );
static void *split_alloc( struct node *free_block, size_t request, size_t block_space );
static struct coalesce_report check_neighbors( const void *old_ptr );
static void coalesce( struct coalesce_report *report );
static struct node *coalesce_splay( size_t key );
static void remove_head( struct node *head, struct node *lft_child, struct node *rgt_child );
static void *free_coalesced_node( void *to_coalesce );
static struct node *find_best_fit( size_t key );
static void insert_node( struct node *current );
static struct node *splay( struct node *root, size_t key );
static struct node *splay_bestfit( struct node *root, size_t key );
static void add_duplicate( struct node *head, struct duplicate_node *add, struct node *parent );
static struct node *delete_duplicate( struct node *head );
static bool check_init( struct heap_range r, size_t heap_size );
static bool is_memory_balanced( size_t *total_free_mem, struct heap_range r, struct size_total s );
static size_t extract_tree_mem( const struct node *root, const void *nil_and_tail );
static bool is_tree_mem_valid( const struct node *root, const void *nil_and_tail, size_t total_free_mem );
static bool are_subtrees_valid( const struct node *root, const struct node *nil );
static bool
is_duplicate_storing_parent( const struct node *parent, const struct node *root, const void *nil_and_tail );
static void print_tree( const struct node *root, const void *nil_and_tail, enum print_style style );
static void print_all( struct heap_range r, size_t heap_size, struct node *tree_root, struct node *nil );

///////////////////////////////   Shared Heap Functions   ////////////////////////////////

size_t get_free_total( void )
{
    return free_nodes.total;
}

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
    free_nodes.nil->header = 0UL;
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
    size_t client_request = roundup( requested_size, ALIGNMENT );
    // Search the tree for the best possible fitting node.
    struct node *found_node = find_best_fit( client_request );
    if ( found_node == free_nodes.nil ) {
        return NULL;
    }
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
    struct coalesce_report report = check_neighbors( old_ptr );
    size_t old_size = get_size( report.current->header );
    if ( report.available >= request ) {
        coalesce( &report );
        if ( report.current == report.left ) {
            memmove( get_client_space( report.current ), old_ptr, old_size ); // NOLINT(*UnsafeBufferHandling)
        }
        return split_alloc( report.current, request, report.available );
    }
    void *elsewhere = mymalloc( request );
    // No data has moved or been modified at this point we will will just do nothing.
    if ( !elsewhere ) {
        return NULL;
    }
    memcpy( elsewhere, old_ptr, old_size ); // NOLINT(*UnsafeBufferHandling)
    coalesce( &report );
    init_free_node( report.current, report.available );
    return elsewhere;
}

void myfree( void *ptr )
{
    if ( ptr == NULL ) {
        return;
    }
    struct coalesce_report report = check_neighbors( ptr );
    coalesce( &report );
    init_free_node( report.current, get_size( report.current->header ) );
}

//////////////////////////////////      Public Validation        /////////////////////////////////////////

bool validate_heap( void )
{

    if ( !check_init( ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size ) ) {
        return false;
    }
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    size_t total_free_mem = 0;
    if ( !is_memory_balanced(
             &total_free_mem,
             ( struct heap_range ){ heap.client_start, heap.client_end },
             ( struct size_total ){ heap.heap_size, free_nodes.total }
         ) ) {
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

size_t myheap_align( size_t request )
{
    return roundup( request, ALIGNMENT );
}

size_t myheap_capacity( void )
{
    size_t total_free_mem = 0;
    size_t block_size_check = 0;
    for ( struct node *cur_node = heap.client_start; cur_node != heap.client_end;
          cur_node = get_right_neighbor( cur_node, block_size_check ) ) {
        block_size_check = get_size( cur_node->header );
        if ( !is_block_allocated( cur_node->header ) ) {
            total_free_mem += block_size_check;
        }
    }
    return total_free_mem;
}

void myheap_diff( const struct heap_block expected[], struct heap_block actual[], size_t len )
{
    struct node *cur_node = heap.client_start;
    size_t i = 0;
    for ( ; i < len && cur_node != heap.client_end; ++i ) {
        bool is_allocated = is_block_allocated( cur_node->header );
        const size_t next_jump = get_size( cur_node->header );
        size_t cur_size = get_size( cur_node->header );
        void *client_addr = get_client_space( cur_node );
        if ( !expected[i].address && is_allocated ) {
            actual[i] = ( struct heap_block ){
                client_addr,
                cur_size,
                ER,
            };
        } else if ( NA == expected[i].payload_bytes ) {
            actual[i] = ( struct heap_block ){
                is_allocated ? client_addr : NULL,
                NA,
                OK,
            };
        } else if ( expected[i].payload_bytes != cur_size ) {
            actual[i] = ( struct heap_block ){
                is_allocated ? client_addr : NULL,
                cur_size,
                ER,
            };
        } else {
            actual[i] = ( struct heap_block ){
                is_allocated ? client_addr : NULL,
                cur_size,
                OK,
            };
        }
        cur_node = get_right_neighbor( cur_node, next_jump );
    }
    if ( i < len ) {
        for ( size_t fill = i; fill < len; ++fill ) {
            actual[fill].err = OUT_OF_BOUNDS;
        }
        return;
    }
    if ( cur_node != heap.client_end ) {
        actual[i].err = HEAP_CONTINUES;
    }
}

//////////////////////////////////   Public Printers        /////////////////////////////////////////

/// @note  the red and blue links represent the heavy/light decomposition of a splay tree. For more
///        information on this interpretation see any Stanford 166 lecture on splay trees.
void print_free_nodes( enum print_style style )
{
    printf( "%s(X)%s", COLOR_CYN, COLOR_NIL );
    printf( " Indicates number of nodes in the subtree rooted at X.\n" );
    printf(
        "%sBlue%s edge means total nodes rooted at X %s<=%s ((number of nodes rooted at Parent) / 2).\n",
        COLOR_BLU_BOLD,
        COLOR_NIL,
        COLOR_BLU_BOLD,
        COLOR_NIL
    );
    printf(
        "%sRed%s edge means total nodes rooted at X %s>%s ((number of nodes rooted at Parent) / 2).\n",
        COLOR_RED_BOLD,
        COLOR_NIL,
        COLOR_RED_BOLD,
        COLOR_NIL
    );
    printf(
        "This is the %sheavy%s/%slight%s decomposition of a Splay Tree.\n",
        COLOR_RED_BOLD,
        COLOR_NIL,
        COLOR_BLU_BOLD,
        COLOR_NIL
    );
    printf( "%s(+X)%s", COLOR_CYN, COLOR_NIL );
    printf( " Indicates duplicate nodes in the tree linked by a doubly-linked list.\n" );
    print_tree( free_nodes.root, free_nodes.nil, style );
}

void dump_heap( void )
{
    print_all(
        ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size, free_nodes.root, free_nodes.nil
    );
}

/////////////////////    Static Heap Helper Functions    //////////////////////////////////

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

static void init_free_node( struct node *to_free, size_t block_size )
{
    to_free->header = block_size | LEFT_ALLOCATED;
    to_free->list_start = free_nodes.list_tail;
    init_footer( to_free, block_size );
    get_right_neighbor( to_free, block_size )->header &= LEFT_FREE;
    insert_node( to_free );
}

static struct coalesce_report check_neighbors( const void *old_ptr )
{
    struct node *current_node = get_node( old_ptr );
    const size_t original_space = get_size( current_node->header );
    struct coalesce_report result = { NULL, current_node, NULL, original_space };

    struct node *rightmost_node = get_right_neighbor( current_node, original_space );
    if ( !is_block_allocated( rightmost_node->header ) ) {
        result.available += get_size( rightmost_node->header ) + HEADERSIZE;
        result.right = rightmost_node;
    }

    if ( current_node != heap.client_start && is_left_space( current_node ) ) {
        result.left = get_left_neighbor( current_node );
        result.available += get_size( result.left->header ) + HEADERSIZE;
    }
    return result;
}

static inline void coalesce( struct coalesce_report *report )
{
    if ( report->left ) {
        report->current = free_coalesced_node( report->left );
    }
    if ( report->right ) {
        report->right = free_coalesced_node( report->right );
    }
    init_header_size( report->current, report->available );
}

static void *free_coalesced_node( void *to_coalesce )
{
    struct node *tree_node = to_coalesce;
    // Go find and fix the node the normal way if it is unique.
    if ( tree_node->list_start == free_nodes.list_tail ) {
        return coalesce_splay( get_size( tree_node->header ) );
    }
    struct duplicate_node *list_node = to_coalesce;
    struct node *lft_tree_node = tree_node->links[L];

    if ( lft_tree_node != free_nodes.nil && lft_tree_node->list_start == to_coalesce ) {
        // Coalescing the first node in linked list. Dummy head, aka lft_tree_node, is to the left.
        list_node->links[N]->parent = list_node->parent;
        lft_tree_node->list_start = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

    } else if ( NULL == list_node->parent ) {
        // All nodes besides the tree node head and the first duplicate node have parent set to NULL.
        list_node->links[P]->links[N] = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];
    } else {
        // Coalesce head of a doubly linked list in the tree. Remove and make a new head.
        remove_head( tree_node, lft_tree_node, tree_node->links[R] );
    }
    free_nodes.total--;
    return to_coalesce;
}

static struct node *coalesce_splay( size_t key )
{
    if ( free_nodes.root == free_nodes.nil ) {
        return free_nodes.nil;
    }
    struct node *to_return = splay( free_nodes.root, key );
    free_nodes.root = to_return;
    if ( get_size( to_return->header ) < key ) {
        return free_nodes.nil;
    }
    if ( to_return->list_start != free_nodes.list_tail ) {
        free_nodes.root->list_start->parent = free_nodes.nil;
        return delete_duplicate( to_return );
    }
    if ( to_return->links[L] == free_nodes.nil ) {
        free_nodes.root = to_return->links[R];
    } else {
        free_nodes.root = splay( to_return->links[L], key );
        give_parent_subtree( free_nodes.root, R, to_return->links[R] );
    }
    if ( free_nodes.root != free_nodes.nil && free_nodes.root->list_start != free_nodes.list_tail ) {
        free_nodes.root->list_start->parent = free_nodes.nil;
    }
    --free_nodes.total;
    return to_return;
}

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

////////////////////////      Splay Tree Best Fit Implementation       /////////////////////////////

static struct node *find_best_fit( size_t key )
{
    if ( free_nodes.root == free_nodes.nil ) {
        return free_nodes.nil;
    }
    struct node *to_return = splay_bestfit( free_nodes.root, key );
    free_nodes.root = to_return;
    if ( get_size( to_return->header ) < key ) {
        return free_nodes.nil;
    }
    if ( to_return->list_start != free_nodes.list_tail ) {
        free_nodes.root->list_start->parent = free_nodes.nil;
        return delete_duplicate( free_nodes.root );
    }
    if ( to_return->links[L] == free_nodes.nil ) {
        free_nodes.root = to_return->links[R];
    } else {
        free_nodes.root = splay_bestfit( to_return->links[L], key );
        give_parent_subtree( free_nodes.root, R, to_return->links[R] );
    }
    if ( free_nodes.root != free_nodes.nil && free_nodes.root->list_start != free_nodes.list_tail ) {
        free_nodes.root->list_start->parent = free_nodes.nil;
    }
    --free_nodes.total;
    return to_return;
}

static struct node *delete_duplicate( struct node *head )
{
    struct duplicate_node *next_node = head->list_start;
    // Take care of the possible node to the right in the doubly linked list first. This could be
    // another node or it could be free_nodes.list_tail, it does not matter either way.
    next_node->links[N]->parent = next_node->parent;
    next_node->links[N]->links[P] = (struct duplicate_node *)head;
    head->list_start = next_node->links[N];
    --free_nodes.total;
    return (struct node *)next_node;
}

/////////////////////   Core Splay Operations for Insertion and Deletion   ///////////////////////

static struct node *splay_bestfit( struct node *root, size_t key )
{
    // Pointers in an array and we can use the symmetric enum and flip it to choose the Left or Right subtree.
    // Another benefit of our nil node: use it as our helper tree because we don't need its Left Right fields.
    free_nodes.nil->links[L] = free_nodes.nil;
    free_nodes.nil->links[R] = free_nodes.nil;
    struct node *left_right_subtrees[2] = { free_nodes.nil, free_nodes.nil };
    struct node *finger = NULL;
    size_t best_fit = ULLONG_MAX;
    for ( ;; ) {
        size_t root_size = get_size( root->header );
        // Because topdown rotations may move the best fit to the left or right subtrees we are building we
        // need to check if the best fit is one of the children before potentially rotating/linking them away.
        best_fit = root_size < best_fit && root_size >= key ? root_size : best_fit;
        size_t left_child_size = get_size( root->links[L]->header );
        best_fit = left_child_size < best_fit && left_child_size >= key ? left_child_size : best_fit;
        size_t right_child_size = get_size( root->links[R]->header );
        best_fit = right_child_size < best_fit && right_child_size >= key ? right_child_size : best_fit;
        enum tree_link link_to_descend = root_size < key;
        if ( key == root_size || root->links[link_to_descend] == free_nodes.nil ) {
            break;
        }

        size_t child_size = get_size( root->links[link_to_descend]->header );
        enum tree_link link_to_descend_from_child = child_size < key;
        if ( key != child_size && link_to_descend == link_to_descend_from_child ) {
            finger = root->links[link_to_descend];
            give_parent_subtree( root, link_to_descend, finger->links[!link_to_descend] );
            give_parent_subtree( finger, !link_to_descend, root );
            root = finger;
            if ( root->links[link_to_descend] == free_nodes.nil ) {
                break;
            }
        }
        give_parent_subtree( left_right_subtrees[!link_to_descend], link_to_descend, root );
        left_right_subtrees[!link_to_descend] = root;
        root = root->links[link_to_descend];
    }
    give_parent_subtree( left_right_subtrees[L], R, root->links[L] );
    give_parent_subtree( left_right_subtrees[R], L, root->links[R] );
    give_parent_subtree( root, L, free_nodes.nil->links[R] );
    give_parent_subtree( root, R, free_nodes.nil->links[L] );
    // We need bestfit which is not what splaytrees are made for. We can't rewind all the fixups or
    // cherry pick the best fit; we need splay operations to fixup the tree for our selection. So,
    // we will run another search for that specific bestfit value. Total, that's 2 traversals in worst case.
    if ( get_size( root->header ) < key ) {
        return splay( root, best_fit );
    }
    return root;
}

static struct node *splay( struct node *root, size_t key )
{
    // Pointers in an array and we can use the symmetric enum and flip it to choose the Left or Right subtree.
    // Another benefit of our nil node: use it as our helper tree because we don't need its Left Right fields.
    free_nodes.nil->links[L] = free_nodes.nil;
    free_nodes.nil->links[R] = free_nodes.nil;
    struct node *left_right_subtrees[2] = { free_nodes.nil, free_nodes.nil };
    struct node *finger = NULL;
    for ( ;; ) {
        size_t root_size = get_size( root->header );
        enum tree_link link_to_descend = root_size < key;
        if ( key == root_size || root->links[link_to_descend] == free_nodes.nil ) {
            break;
        }
        size_t child_size = get_size( root->links[link_to_descend]->header );
        enum tree_link link_to_descend_from_child = child_size < key;
        if ( key != child_size && link_to_descend == link_to_descend_from_child ) {
            finger = root->links[link_to_descend];
            give_parent_subtree( root, link_to_descend, finger->links[!link_to_descend] );
            give_parent_subtree( finger, !link_to_descend, root );
            root = finger;
            if ( root->links[link_to_descend] == free_nodes.nil ) {
                break;
            }
        }
        give_parent_subtree( left_right_subtrees[!link_to_descend], link_to_descend, root );
        left_right_subtrees[!link_to_descend] = root;
        root = root->links[link_to_descend];
    }
    give_parent_subtree( left_right_subtrees[L], R, root->links[L] );
    give_parent_subtree( left_right_subtrees[R], L, root->links[R] );
    give_parent_subtree( root, L, free_nodes.nil->links[R] );
    give_parent_subtree( root, R, free_nodes.nil->links[L] );
    return root;
}

/////////////////////////    Splay Tree Insertion Logic           ////////////////////////////

static void insert_node( struct node *current )
{
    size_t current_key = get_size( current->header );
    if ( free_nodes.root == free_nodes.nil ) {
        current->links[L] = free_nodes.nil;
        current->links[R] = free_nodes.nil;
        current->list_start = free_nodes.list_tail;
        free_nodes.root = current;
        free_nodes.total = 1;
        return;
    }
    free_nodes.root = splay( free_nodes.root, current_key );
    size_t found_size = get_size( free_nodes.root->header );
    if ( current_key == found_size ) {
        if ( free_nodes.root->list_start != free_nodes.list_tail ) {
            free_nodes.root->list_start->parent = free_nodes.nil;
        }
        add_duplicate( free_nodes.root, (struct duplicate_node *)current, free_nodes.nil );
        return;
    }
    enum tree_link link = found_size < current_key;
    give_parent_subtree( current, link, free_nodes.root->links[link] );
    give_parent_subtree( current, !link, free_nodes.root );
    free_nodes.root->links[link] = free_nodes.nil;
    free_nodes.root = current;
    ++free_nodes.total;
}

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

/////////////////////////////   Basic Block, Header, and Tree Operations  //////////////////////////////////

static inline void give_parent_subtree( struct node *parent, enum tree_link dir, struct node *subtree )
{
    parent->links[dir] = subtree;
    if ( subtree != free_nodes.nil && subtree->list_start != free_nodes.list_tail ) {
        subtree->list_start->parent = parent;
    }
}

static inline size_t roundup( size_t requested_size, size_t multiple )
{
    return requested_size <= HEAP_NODE_WIDTH ? HEAP_NODE_WIDTH
                                             : ( requested_size + multiple - 1 ) & ~( multiple - 1 );
}

static inline size_t get_size( header header_val )
{
    return SIZE_MASK & header_val;
}

static inline bool is_block_allocated( const header block_header )
{
    return block_header & ALLOCATED;
}

static inline bool is_left_space( const struct node *node )
{
    return !( node->header & LEFT_ALLOCATED );
}

static inline void init_header_size( struct node *node, size_t payload )
{
    node->header = LEFT_ALLOCATED | payload;
}

static inline void init_footer( struct node *node, size_t payload )
{
    header *footer = (header *)( (uint8_t *)node + payload );
    *footer = node->header;
}

static inline struct node *get_right_neighbor( const struct node *current, size_t payload )
{
    return (struct node *)( (uint8_t *)current + HEADERSIZE + payload );
}

static inline struct node *get_left_neighbor( const struct node *node )
{
    header *left_footer = (header *)( (uint8_t *)node - HEADERSIZE );
    return (struct node *)( (uint8_t *)node - ( *left_footer & SIZE_MASK ) - HEADERSIZE );
}

static inline void *get_client_space( const struct node *node_header )
{
    return (uint8_t *)node_header + HEADERSIZE;
}

static inline struct node *get_node( const void *client_space )
{
    return (struct node *)( (uint8_t *)client_space - HEADERSIZE );
}

/////////////////////////////    Debugging and Testing Functions   //////////////////////////////////

// NOLINTBEGIN(misc-no-recursion)

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
    if ( size_used + *total_free_mem != s.size ) {
        BREAKPOINT();
        return false;
    }
    if ( total_free_nodes != s.total ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

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

static bool is_tree_mem_valid( const struct node *root, const void *nil_and_tail, size_t total_free_mem )
{
    if ( total_free_mem != extract_tree_mem( root, nil_and_tail ) ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

static bool
strict_bound_met( const struct node *root, size_t root_size, enum tree_link dir, const struct node *nil )
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

static bool
is_duplicate_storing_parent( const struct node *parent, const struct node *root, const void *nil_and_tail )
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

static void print_inner_tree(
    const struct node *root,
    size_t parent_size,
    const char *prefix,
    const char *prefix_branch_color,
    const enum print_link node_type,
    const enum tree_link dir,
    enum print_style style
)
{
    if ( root == free_nodes.nil ) {
        return;
    }
    size_t subtree_size = get_subtree_size( root );
    printf( "%s", prefix );
    printf(
        "%s%s%s",
        subtree_size <= parent_size / 2 ? COLOR_BLU_BOLD : COLOR_RED_BOLD,
        node_type == LEAF ? " └──" : " ├──",
        COLOR_NIL
    );
    printf( COLOR_CYN );
    printf( "(%zu)", subtree_size );
    dir == L ? printf( "L:" COLOR_NIL ) : printf( "R:" COLOR_NIL );
    print_node( root, free_nodes.nil, style );

    char *str = NULL;
    int string_length = snprintf(
        NULL,
        0,
        "%s%s%s",
        prefix,
        prefix_branch_color, // NOLINT
        node_type == LEAF ? "     " : " │   "
    );
    if ( string_length > 0 ) {
        str = malloc( string_length + 1 );
        (void)snprintf(
            str,
            string_length,
            "%s%s%s",
            prefix,
            prefix_branch_color, // NOLINT
            node_type == LEAF ? "     " : " │   "
        );
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

static void print_alloc_block( const struct node *node )
{
    size_t block_size = get_size( node->header );
    // We will see from what direction our header is messed up by printing 16
    // digits.
    printf( COLOR_GRN "%p: HDR->0x%016zX(%zubytes)\n" COLOR_NIL, node, node->header, block_size );
}

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

static void print_error_block( const struct node *node, size_t block_size )
{
    printf( "\n%p: HDR->0x%016zX->%zubyts\n", node, node->header, block_size );
    printf( "Block size is too large and header is corrupted.\n" );
}

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

static void print_all( struct heap_range r, size_t heap_size, struct node *tree_root, struct node *nil )
{
    struct node *node = r.start;
    printf(
        "Heap client segment starts at address %p, ends %p. %zu total bytes "
        "currently used.\n",
        node,
        r.end,
        heap_size
    );
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
