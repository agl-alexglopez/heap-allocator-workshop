/// Author: Alexander Griffin Lopez
/// File: rbtree_topdown.c
/// ---------------------
///  This file contains my implementation of an explicit heap allocator. This allocator uses a tree
///  implementation to track the free space in the heap, relying on the properties of a red-black
///  tree to remain balanced. This implementation also uses some interesting strategies to unify
///  left and right cases for a red black tree and maintains a doubly linked list of duplicate nodes
///  of the same size if a node in the tree has repeats of its size. We do not use a parent field,
///  instead opting to form a stack of nodes and pass that stack to the necessary functions. We also
///  use a topdown approach to insertion and deletion meaning we fix the tree on the way down, not
///  up.
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
///  3. I used Julienne Walker's implementation of top down insertion and deletion into a red black
///     tree to guide my topdown implementation. Specifically, I followed Walker's insert and delete
///     functions. However, I modified both to allow for duplicates in my tree because the heap
///     requires duplicate nodes to be stored. I used a doubly linked list to acheive this.
///          https://web.archive.org/web/20141129024312/http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx
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

/////////////////////////////           Type Definitions           //////////////////////////////////

typedef size_t header;

/// Red Black Free Tree:
///  - Maintain a red black tree of free nodes.
///  - Root is black
///  - No red node has a red child
///  - New insertions are red
///  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
///  - Every path from root to free_nodes.black_nil, root not included, has same number of black nodes.
///  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
///  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status for coalescing.
///  - Use a *list_start pointer to a doubly linked list of duplicate nodes of
/// the same size.
struct rb_node
{
    // The header will store block size, allocation status, left neighbor status,
    // and node color.
    header header;
    struct rb_node *links[2];
    // If we enter a doubly linked list with this pointer the idiom is p/n, NOT l/r.
    struct duplicate_node *list_start;
};

struct duplicate_node
{
    header header;
    struct duplicate_node *links[2];
    // We can acheive O(1) coalescing of any duplicate if we store parent in first
    // node in list.
    struct rb_node *parent;
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

struct coalesce_report
{
    struct rb_node *left;
    struct rb_node *current;
    struct rb_node *right;
    size_t available;
};

enum rb_color
{
    BLACK = 0,
    RED = 1
};

// Symmetry can be unified to one case because !l == r and !r == l.
enum tree_link
{
    // (L == LEFT), (R == RIGHT)
    L = 0,
    R = 1
};

// When you see these indices, know we are referring to a doubly linked list.
enum list_link
{
    // (P == PREVIOUS), (N == NEXT)
    P = 0,
    N = 1
};

#define SIZE_MASK ~0x7UL
#define MIN_BLOCK_SIZE 40UL
#define HEADERSIZE sizeof( size_t )
#define FREED 0x0UL
#define ALLOCATED 0x1UL
#define LEFT_ALLOCATED 0x2UL
#define COLOR_MASK 0x4UL
#define HEAP_NODE_WIDTH 32UL
#define RED_PAINT 0x4UL
#define BLK_PAINT ~0x4UL
#define LEFT_FREE ~0x2UL

/////////////////             Static Heap Tracking    /////////////////////////////////

struct rotation
{
    struct rb_node *root;
    struct rb_node *parent;
};

struct replacement
{
    struct rb_node *remove;
    struct rb_node *replacement_parent;
    struct rb_node *replacement;
};

// NOLINTBEGIN(*non-const-global-variables)

/// Red Black Free Tree:
///  - Maintain a red black tree of free nodes.
///  - Root is black
///  - No red node has a red child
///  - New insertions are red
///  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
///  - Every path from root to free_nodes.black_nil, root not included, has same number of black nodes.
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

static struct heap
{
    void *client_start;
    void *client_end;
    size_t heap_size;
} heap;

// NOLINTEND(*non-const-global-variables)

////////////////////////////    Forward Declarations    ////////////////////////////////

static void init_free_node( struct rb_node *to_free, size_t block_size );
static void *split_alloc( struct rb_node *free_block, size_t request, size_t block_space );
static struct coalesce_report check_neighbors( const void *old_ptr );
static void coalesce( struct coalesce_report *report );
static struct rb_node *single_rotation( struct rotation root_parent, enum tree_link dir );
static struct rb_node *double_rotation( struct rotation root_parent, enum tree_link dir );
static void add_duplicate( struct rb_node *head, struct duplicate_node *to_add, struct rb_node *parent );
static void insert_rb_topdown( struct rb_node *current );
static void rb_transplant( struct rb_node *parent, struct rb_node *remove, struct rb_node *replace );
static struct rb_node *delete_duplicate( struct rb_node *head );
static struct rb_node *remove_node( struct rb_node *parent, struct replacement r );
static struct rb_node *delete_rb_topdown( size_t key );
static void remove_head( struct rb_node *head, struct rb_node *lft_child, struct rb_node *rgt_child );
static void *free_coalesced_node( void *to_coalesce );
static size_t roundup( size_t requested_size, size_t multiple );
static void paint_node( struct rb_node *node, enum rb_color color );
static enum rb_color get_color( header header_val );
static size_t get_size( header header_val );
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
static size_t extract_tree_mem( const struct rb_node *root, const void *nil_and_tail );
static bool is_rbtree_mem_valid( const struct rb_node *root, const void *nil_and_tail, size_t total_free_mem );
static bool is_bheight_valid_v2( const struct rb_node *root, const struct rb_node *black_nil );
static bool are_subtrees_valid( const struct rb_node *root, const struct rb_node *black_nil );
static bool
is_duplicate_storing_parent( const struct rb_node *parent, const struct rb_node *root, const void *nil_and_tail );
static void print_rb_tree( const struct rb_node *root, const void *nil_and_tail, enum print_style style );
static void
print_all( struct heap_range r, size_t heap_size, struct rb_node *tree_root, struct rb_node *black_nil );

////////////////////////    Shared Heap Functions   ////////////////////////////////////////

size_t get_free_total( void )
{
    return free_nodes.total;
}

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

    // Two sentinels will point to same address. Makes it clear which we refer to and saves space.
    free_nodes.black_nil = heap.client_end;
    free_nodes.list_tail = heap.client_end;
    free_nodes.black_nil->header = 1UL;
    paint_node( free_nodes.black_nil, BLACK );

    free_nodes.tree_root = heap.client_start;
    init_header_size( free_nodes.tree_root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    paint_node( free_nodes.tree_root, BLACK );
    init_footer( free_nodes.tree_root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
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
    struct rb_node *found_node = delete_rb_topdown( client_request );
    if ( found_node == free_nodes.black_nil ) {
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

///////////////////////////      Shared Debugging    ////////////////////////////////

bool validate_heap( void )
{
    if ( !check_init( ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size ) ) {
        return false;
    }
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    size_t total_free_mem = 0;
    if ( !is_memory_balanced(
             &total_free_mem, ( struct heap_range ){ heap.client_start, heap.client_end },
             ( struct size_total ){ heap.heap_size, free_nodes.total }
         ) ) {
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
    if ( !are_subtrees_valid( free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }
    if ( !is_duplicate_storing_parent( free_nodes.black_nil, free_nodes.tree_root, free_nodes.black_nil ) ) {
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
    for ( struct rb_node *cur_node = heap.client_start; cur_node != heap.client_end;
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
    struct rb_node *cur_node = heap.client_start;
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

////////////////////////   Shared Printing Debugger    //////////////////////////////////

void print_free_nodes( enum print_style style )
{
    printf( COLOR_CYN "(+X)" COLOR_NIL );
    printf( " Indicates duplicate nodes in the tree linked by a doubly-linked list.\n" );
    print_rb_tree( free_nodes.tree_root, free_nodes.black_nil, style );
}

void dump_heap( void )
{
    print_all(
        ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size, free_nodes.tree_root,
        free_nodes.black_nil
    );
}

////////////////////////////    Static Heap Helper Function    ////////////////////////////////

static void init_free_node( struct rb_node *to_free, size_t block_size )
{
    to_free->header = block_size | LEFT_ALLOCATED | RED_PAINT;
    to_free->list_start = free_nodes.list_tail;
    init_footer( to_free, block_size );
    get_right_neighbor( to_free, block_size )->header &= LEFT_FREE;
    insert_rb_topdown( to_free );
}

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

static struct coalesce_report check_neighbors( const void *old_ptr )
{
    struct rb_node *current_node = get_rb_node( old_ptr );
    const size_t original_space = get_size( current_node->header );
    struct coalesce_report result = { NULL, current_node, NULL, original_space };

    struct rb_node *rightmost_node = get_right_neighbor( current_node, original_space );
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

//////////////////////    Red-Black Tree Best Fit Implementation   /////////////////////////////

static struct rb_node *delete_rb_topdown( size_t key ) // NOLINT(*cognitive-complexity)
{
    if ( free_nodes.tree_root == free_nodes.black_nil ) {
        return free_nodes.black_nil;
    }
    struct rb_node *child = free_nodes.black_nil;
    struct rb_node *parent = free_nodes.black_nil;
    struct rb_node *gparent = NULL;
    struct rb_node *best = free_nodes.black_nil;
    struct rb_node *best_parent = free_nodes.black_nil;
    size_t best_fit_size = ULLONG_MAX;
    enum tree_link link = R;
    child->links[R] = free_nodes.tree_root;
    child->links[L] = free_nodes.black_nil;

    while ( child->links[link] != free_nodes.black_nil ) {
        enum tree_link prev_link = link;
        gparent = parent;
        parent = child;
        child = child->links[link];
        size_t child_size = get_size( child->header );
        link = child_size < key;

        // Best fit approximation and the best choice will win by the time we reach bottom.
        if ( child_size < best_fit_size && child_size >= key ) {
            best_parent = parent;
            best = child;
        }
        // We can cut the link off early just to save more needless fixup work if duplicate.
        if ( key == child_size && best->list_start != free_nodes.list_tail ) {
            return delete_duplicate( best );
        }

        // Double black needs our attention due to black height requirements.
        if ( get_color( child->header ) == BLACK && get_color( child->links[link]->header ) == BLACK ) {

            // We need access to six pointers, and two directions. Decomposition is difficult.
            struct rb_node *nxt_sibling = child->links[!link];
            struct rb_node *sibling = parent->links[!prev_link];
            if ( get_color( nxt_sibling->header ) == RED ) {
                gparent = nxt_sibling;
                parent = parent->links[prev_link] = single_rotation( ( struct rotation ){ child, parent }, link );
                if ( child == best ) {
                    best_parent = gparent;
                }
            } else if ( sibling != free_nodes.black_nil && get_color( nxt_sibling->header ) == BLACK
                        && get_color( sibling->links[!prev_link]->header ) == BLACK
                        && get_color( sibling->links[prev_link]->header ) == BLACK ) {
                // Our black height will be altered. Recolor.
                paint_node( parent, BLACK );
                paint_node( sibling, RED );
                paint_node( child, RED );
            } else if ( sibling != free_nodes.black_nil && get_color( nxt_sibling->header ) == BLACK ) {
                // Another black is waiting down the tree. Red violations and path violations possible.
                enum tree_link to_parent = gparent->links[R] == parent;
                // These two cases may ruin lineage of node to be removed. Repair if necessary.
                if ( get_color( sibling->links[prev_link]->header ) == RED ) {
                    gparent->links[to_parent]
                        = double_rotation( ( struct rotation ){ parent, gparent }, prev_link );
                    if ( best == parent ) {
                        best_parent = gparent->links[to_parent];
                    }
                } else if ( get_color( sibling->links[!prev_link]->header ) == RED ) {
                    gparent->links[to_parent]
                        = single_rotation( ( struct rotation ){ parent, gparent }, prev_link );
                    if ( best == parent ) {
                        best_parent = sibling;
                    }
                }
                paint_node( child, RED );
                paint_node( gparent->links[to_parent], RED );
                paint_node( gparent->links[to_parent]->links[L], BLACK );
                paint_node( gparent->links[to_parent]->links[R], BLACK );
            }
        }
    }
    if ( get_size( best->header ) < key || best == free_nodes.black_nil ) {
        return free_nodes.black_nil;
    }
    return remove_node( best_parent, ( struct replacement ){ best, parent, child } );
}

static struct rb_node *remove_node( struct rb_node *parent, struct replacement r )
{
    if ( r.remove->list_start != free_nodes.list_tail ) {
        return delete_duplicate( r.remove );
    }
    if ( r.remove->links[L] == free_nodes.black_nil || r.remove->links[R] == free_nodes.black_nil ) {
        enum tree_link nil_link = r.remove->links[L] != free_nodes.black_nil;
        rb_transplant( parent, r.remove, r.remove->links[!nil_link] );
    } else {
        if ( r.replacement != r.remove->links[R] ) {
            rb_transplant( r.replacement_parent, r.replacement, r.replacement->links[R] );
            r.replacement->links[R] = r.remove->links[R];
            r.replacement->links[R]->list_start->parent = r.replacement;
        }
        rb_transplant( parent, r.remove, r.replacement );
        r.replacement->links[L] = r.remove->links[L];
        if ( r.replacement->links[L] != free_nodes.black_nil ) {
            r.replacement->links[L]->list_start->parent = r.replacement;
        }
        r.replacement->list_start->parent = parent;
    }
    paint_node( r.replacement, get_color( r.remove->header ) );
    paint_node( free_nodes.black_nil, BLACK );
    paint_node( free_nodes.tree_root, BLACK );
    free_nodes.total--;
    return r.remove;
}

static struct rb_node *delete_duplicate( struct rb_node *head )
{
    struct duplicate_node *next_node = head->list_start;
    // Take care of the possible node to the right in the doubly linked list first. This could be
    // another node or it could be free_nodes.black_nil, it does not matter either way.
    next_node->links[N]->parent = next_node->parent;
    next_node->links[N]->links[P] = (struct duplicate_node *)head;
    head->list_start = next_node->links[N];
    free_nodes.total--;
    return (struct rb_node *)next_node;
}

static void rb_transplant( struct rb_node *parent, struct rb_node *remove, struct rb_node *replace )
{
    if ( parent == free_nodes.black_nil ) {
        free_nodes.tree_root = replace;
    } else {
        parent->links[parent->links[R] == remove] = replace;
    }
    if ( replace != free_nodes.black_nil ) {
        replace->list_start->parent = parent;
    }
}

static void *free_coalesced_node( void *to_coalesce )
{
    struct rb_node *tree_node = to_coalesce;
    // Go find and fix the node the normal way if it is unique.
    if ( tree_node->list_start == free_nodes.list_tail ) {
        return delete_rb_topdown( get_size( tree_node->header ) );
    }
    struct duplicate_node *list_node = to_coalesce;
    struct rb_node *lft_tree_node = tree_node->links[L];

    // Coalescing the first node in linked list. Dummy head, aka lft_tree_node, is to the left.
    if ( lft_tree_node != free_nodes.black_nil && lft_tree_node->list_start == to_coalesce ) {
        list_node->links[N]->parent = list_node->parent;
        lft_tree_node->list_start = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

        // All nodes besides the tree node head and the first duplicate node have parent set to NULL.
    } else if ( NULL == list_node->parent ) {
        list_node->links[P]->links[N] = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

        // Coalesce the head of a doubly linked list in the tree. Remove and make a new head.
    } else {
        remove_head( tree_node, lft_tree_node, tree_node->links[R] );
    }
    free_nodes.total--;
    return to_coalesce;
}

static void remove_head( struct rb_node *head, struct rb_node *lft_child, struct rb_node *rgt_child )
{
    // Store the parent in an otherwise unused field for a major O(1) coalescing speed boost.
    struct rb_node *tree_parent = head->list_start->parent;
    head->list_start->header = head->header;
    head->list_start->links[N]->parent = head->list_start->parent;

    struct rb_node *new_tree_node = (struct rb_node *)head->list_start;
    new_tree_node->list_start = head->list_start->links[N];
    new_tree_node->links[L] = lft_child;
    new_tree_node->links[R] = rgt_child;

    // We often write to fields of the black_nil, invariant. DO NOT use the nodes it stores!
    if ( lft_child != free_nodes.black_nil ) {
        lft_child->list_start->parent = new_tree_node;
    }
    if ( rgt_child != free_nodes.black_nil ) {
        rgt_child->list_start->parent = new_tree_node;
    }
    if ( tree_parent == free_nodes.black_nil ) {
        free_nodes.tree_root = new_tree_node;
    } else {
        tree_parent->links[tree_parent->links[R] == head] = new_tree_node;
    }
}

//////////////////////     Red-Black Tree Insertion Logic    ///////////////////////////////

static void insert_rb_topdown( struct rb_node *current )
{
    size_t key = get_size( current->header );
    paint_node( current, RED );

    enum tree_link prev_link = L;
    enum tree_link link = R;
    struct rb_node *ancestor = free_nodes.black_nil;
    struct rb_node *gparent = free_nodes.black_nil;
    struct rb_node *parent = free_nodes.black_nil;
    struct rb_node *child = free_nodes.tree_root;
    size_t child_size = 0;

    // Unfortunate infinite loop structure due to odd nature of topdown fixups. I kept the search
    // and pointer updates at top of loop for clarity but can't figure out how to make proper
    // loop conditions from this logic.
    for ( ;; ancestor = gparent, gparent = parent, parent = child, prev_link = link,
             child = child->links[link = child_size < key] ) {

        child_size = get_size( child->header );
        if ( child_size == key ) {
            add_duplicate( child, (struct duplicate_node *)current, parent );
        } else if ( child == free_nodes.black_nil ) {
            child = current;
            child_size = key;
            parent->links[link] = current;
            current->links[L] = free_nodes.black_nil;
            current->links[R] = free_nodes.black_nil;
            current->list_start = free_nodes.list_tail;
        } else if ( get_color( child->links[L]->header ) == RED && get_color( child->links[R]->header ) == RED ) {
            paint_node( child, RED );
            // If you split a black node down the tree the black height remains constant.
            paint_node( child->links[L], BLACK );
            paint_node( child->links[R], BLACK );
        }

        // Our previous fix could have created a violation further up tree.
        if ( get_color( parent->header ) == RED && get_color( child->header ) == RED ) {
            enum tree_link ancestor_link = ancestor->links[R] == gparent;
            if ( child == parent->links[prev_link] ) {
                ancestor->links[ancestor_link]
                    = single_rotation( ( struct rotation ){ .root = gparent, .parent = ancestor }, !prev_link );
            } else {
                ancestor->links[ancestor_link]
                    = double_rotation( ( struct rotation ){ .root = gparent, .parent = ancestor }, !prev_link );
            }
        }
        if ( child_size == key ) {
            break;
        }
    }

    if ( parent == free_nodes.black_nil ) {
        free_nodes.tree_root = child;
    }
    paint_node( free_nodes.tree_root, BLACK );
    free_nodes.total++;
}

static void add_duplicate( struct rb_node *head, struct duplicate_node *to_add, struct rb_node *parent )
{
    to_add->header = head->header;
    // This will tell us if we are coalescing a duplicate node. Only linked list will have NULL.
    if ( head->list_start == free_nodes.list_tail ) {
        to_add->parent = parent;
    } else {
        to_add->parent = head->list_start->parent;
        head->list_start->parent = NULL;
    }

    // Get the first next node in the doubly linked list, invariant and correct its left field.
    head->list_start->links[P] = to_add;
    to_add->links[N] = head->list_start;
    head->list_start = to_add;
    to_add->links[P] = (struct duplicate_node *)head;
}

////////////////////////////////   Rotation Logic    /////////////////////////////////

static struct rb_node *single_rotation( struct rotation root_parent, enum tree_link dir )
{
    struct rb_node *save = root_parent.root->links[!dir];
    root_parent.root->links[!dir] = save->links[dir];
    if ( save->links[dir] != free_nodes.black_nil ) {
        save->links[dir]->list_start->parent = root_parent.root;
    }
    if ( save != free_nodes.black_nil ) {
        save->list_start->parent = root_parent.parent;
    }
    if ( root_parent.root == free_nodes.tree_root ) {
        free_nodes.tree_root = save;
    }
    save->links[dir] = root_parent.root;
    root_parent.root->list_start->parent = save;
    paint_node( root_parent.root, RED );
    paint_node( save, BLACK );
    return save;
}

static struct rb_node *double_rotation( struct rotation root_parent, enum tree_link dir )
{
    root_parent.root->links[!dir] = single_rotation(
        ( struct rotation ){
            .root = root_parent.root->links[!dir],
            .parent = root_parent.root,
        },
        !dir
    );
    return single_rotation( root_parent, dir );
}

/////////////////////////////   Basic Block and Header Operations  //////////////////////////////////

static inline size_t roundup( size_t requested_size, size_t multiple )
{
    return requested_size <= HEAP_NODE_WIDTH ? HEAP_NODE_WIDTH
                                             : ( requested_size + multiple - 1 ) & ~( multiple - 1 );
}

static inline void paint_node( struct rb_node *node, enum rb_color color )
{
    color == RED ? ( node->header |= RED_PAINT ) : ( node->header &= BLK_PAINT );
}

static inline enum rb_color get_color( header header_val )
{
    return ( header_val & COLOR_MASK ) == RED_PAINT;
}

static inline size_t get_size( header header_val )
{
    return SIZE_MASK & header_val;
}

static inline bool is_block_allocated( const header block_header )
{
    return block_header & ALLOCATED;
}

static inline bool is_left_space( const struct rb_node *node )
{
    return !( node->header & LEFT_ALLOCATED );
}

static inline void init_header_size( struct rb_node *node, size_t payload )
{
    node->header = LEFT_ALLOCATED | payload;
}

static inline void init_footer( struct rb_node *node, size_t payload )
{
    header *footer = (header *)( (uint8_t *)node + payload );
    *footer = node->header;
}

static inline struct rb_node *get_right_neighbor( const struct rb_node *current, size_t payload )
{
    return (struct rb_node *)( (uint8_t *)current + HEADERSIZE + payload );
}

static inline struct rb_node *get_left_neighbor( const struct rb_node *node )
{
    header *left_footer = (header *)( (uint8_t *)node - HEADERSIZE );
    return (struct rb_node *)( (uint8_t *)node - ( *left_footer & SIZE_MASK ) - HEADERSIZE );
}

static inline void *get_client_space( const struct rb_node *node_header )
{
    return (uint8_t *)node_header + HEADERSIZE;
}

static inline struct rb_node *get_rb_node( const void *client_space )
{
    return (struct rb_node *)( (uint8_t *)client_space - HEADERSIZE );
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

static bool is_bheight_valid( const struct rb_node *root, const struct rb_node *black_nil )
{
    return calculate_bheight( root, black_nil ) != -1;
}

static size_t extract_tree_mem( const struct rb_node *root, const void *nil_and_tail )
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

static bool is_rbtree_mem_valid( const struct rb_node *root, const void *nil_and_tail, size_t total_free_mem )
{
    if ( total_free_mem != extract_tree_mem( root, nil_and_tail ) ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

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

static bool is_bheight_valid_v2( const struct rb_node *root, const struct rb_node *black_nil )
{
    return calculate_bheight_v2( root, black_nil ) != 0;
}

static bool
strict_bound_met( const struct rb_node *root, size_t root_size, enum tree_link dir, const struct rb_node *nil )
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

static bool
is_duplicate_storing_parent( const struct rb_node *parent, const struct rb_node *root, const void *nil_and_tail )
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

static void print_node( const struct rb_node *root, const void *nil_and_tail, enum print_style style )
{
    size_t block_size = get_size( root->header );
    get_color( root->header ) == BLACK ? printf( COLOR_BLK ) : printf( COLOR_RED );

    if ( style == VERBOSE ) {
        printf( "%p:", root );
    }

    printf( "(%zubytes)", block_size );
    printf( COLOR_NIL );

    if ( style == VERBOSE ) {
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

static void print_inner_tree(
    const struct rb_node *root, const void *nil_and_tail, const char *prefix, const enum print_link node_type,
    const enum tree_link dir, enum print_style style
)
{
    if ( root == nil_and_tail ) {
        return;
    }
    printf( "%s", prefix );
    printf( "%s", node_type == LEAF ? " └──" : " ├──" );
    printf( COLOR_CYN );
    dir == L ? printf( "L:" ) : printf( "R:" );
    printf( COLOR_NIL );
    print_node( root, nil_and_tail, style );

    char *str = NULL;
    int string_length = snprintf( NULL, 0, "%s%s", prefix, node_type == LEAF ? "     " : " │   " ); // NOLINT
    if ( string_length > 0 ) {
        str = malloc( string_length + 1 );
        (void)snprintf( str, string_length, "%s%s", prefix, node_type == LEAF ? "     " : " │   " ); // NOLINT
    }
    if ( str != NULL ) {
        if ( root->links[R] == nil_and_tail ) {
            print_inner_tree( root->links[L], nil_and_tail, str, LEAF, L, style );
        } else if ( root->links[L] == nil_and_tail ) {
            print_inner_tree( root->links[R], nil_and_tail, str, LEAF, R, style );
        } else {
            print_inner_tree( root->links[R], nil_and_tail, str, BRANCH, R, style );
            print_inner_tree( root->links[L], nil_and_tail, str, LEAF, L, style );
        }
    } else {
        printf( COLOR_ERR "memory exceeded. Cannot display tree." COLOR_NIL );
    }
    free( str );
}

static void print_rb_tree( const struct rb_node *root, const void *nil_and_tail, enum print_style style )
{
    if ( root == nil_and_tail ) {
        return;
    }
    printf( " " );
    print_node( root, nil_and_tail, style );

    if ( root->links[R] == nil_and_tail ) {
        print_inner_tree( root->links[L], nil_and_tail, "", LEAF, L, style );
    } else if ( root->links[L] == nil_and_tail ) {
        print_inner_tree( root->links[R], nil_and_tail, "", LEAF, R, style );
    } else {
        print_inner_tree( root->links[R], nil_and_tail, "", BRANCH, R, style );
        print_inner_tree( root->links[L], nil_and_tail, "", LEAF, L, style );
    }
}

static void print_alloc_block( const struct rb_node *node )
{
    size_t block_size = get_size( node->header );
    // We will see from what direction our header is messed up by printing 16 digits.
    printf( COLOR_GRN "%p: HDR->0x%016zX(%zubytes)\n" COLOR_NIL, node, node->header, block_size );
}

static void print_free_block( const struct rb_node *node )
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
    get_color( node->header ) == BLACK ? printf( COLOR_BLK ) : printf( COLOR_RED );
    printf( "%p: HDR->0x%016zX(%zubytes)\n", node, node->header, block_size );
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
    // that is ok. we will only worry about the next node's color when we delete a duplicate.
    printf( COLOR_NIL );
    printf( "%*c", indent_struct_fields, ' ' );
    printf( "LST->%p\n", node->list_start ? node->list_start : NULL );
    printf( "%*c", indent_struct_fields, ' ' );
    printf( "FTR->0x%016zX\n", to_print );
}

static void print_error_block( const struct rb_node *node, size_t block_size )
{
    printf( "\n%p: HDR->0x%016zX->%zubyts\n", node, node->header, block_size );
    printf( "Block size is too large and header is corrupted.\n" );
}

static void print_bad_jump( const struct rb_node *current, struct bad_jump j, const void *nil_and_tail )
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
    print_rb_tree( j.root, nil_and_tail, VERBOSE );
}

static void print_all( struct heap_range r, size_t heap_size, struct rb_node *tree_root, struct rb_node *black_nil )
{
    struct rb_node *node = r.start;
    printf(
        "Heap client segment starts at address %p, ends %p. %zu total bytes "
        "currently used.\n",
        node, r.end, heap_size
    );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "COLOR KEY: " COLOR_BLK "[BLACK NODE] " COLOR_NIL COLOR_RED "[rED NODE] " COLOR_NIL COLOR_GRN
            "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "%p: START OF  HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", r.start );
    struct rb_node *prev = node;
    while ( node != r.end ) {
        size_t full_size = get_size( node->header );

        if ( full_size == 0 ) {
            print_bad_jump( node, ( struct bad_jump ){ prev, tree_root }, black_nil );
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
