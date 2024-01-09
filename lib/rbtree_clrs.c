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

enum tree_link
{
    L = 0,
    R = 1
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

static struct coalesce_report check_neighbors( const void *old_ptr );
static void coalesce( struct coalesce_report *report );
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
static bool
is_rbtree_mem_valid( const struct rb_node *root, const struct rb_node *black_nil, size_t total_free_mem );
static bool is_parent_valid( const struct rb_node *root, const struct rb_node *black_nil );
static bool is_bheight_valid_v2( const struct rb_node *root, const struct rb_node *black_nil );
static bool are_subtrees_valid( const struct rb_node *root, const struct rb_node *black_nil );
static void print_rb_tree( const struct rb_node *root, const struct rb_node *black_nil, enum print_style style );
static void print_all( struct heap_range r, size_t heap_size, struct rb_node *root, struct rb_node *black_nil );

/////////////////////////////   Shared Heap Functions    ///////////////////////////////////////

size_t wget_free_total( void )
{
    return tree.total;
}

bool winit( void *heap_start, size_t heap_size )
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

void *wmalloc( size_t requested_size )
{
    if ( requested_size == 0 || requested_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    size_t client_request = roundup( requested_size, ALIGNMENT );
    // Search the tree for the best possible fitting node.
    struct rb_node *found_node = find_best_fit( client_request );
    if ( found_node == tree.black_nil ) {
        return NULL;
    }
    return split_alloc( found_node, client_request, get_size( found_node->header ) );
}

void *wrealloc( void *old_ptr, size_t new_size )
{
    if ( new_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    if ( old_ptr == NULL ) {
        return wmalloc( new_size );
    }
    if ( new_size == 0 ) {
        wfree( old_ptr );
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
    void *elsewhere = wmalloc( request );
    // No data has moved or been modified at this point we will will just do nothing.
    if ( !elsewhere ) {
        return NULL;
    }
    memcpy( elsewhere, old_ptr, old_size ); // NOLINT(*UnsafeBufferHandling)
    coalesce( &report );
    init_free_node( report.current, report.available );
    return elsewhere;
}

void wfree( void *ptr )
{
    if ( NULL == ptr ) {
        return;
    }
    struct coalesce_report report = check_neighbors( ptr );
    coalesce( &report );
    init_free_node( report.current, get_size( report.current->header ) );
}

/////////////////////////     Shared Debugger       //////////////////////////////////////////

bool wvalidate_heap( void )
{
    if ( !check_init( ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size ) ) {
        return false;
    }
    size_t total_free_mem = 0;
    if ( !is_memory_balanced(
             &total_free_mem,
             ( struct heap_range ){ heap.client_start, heap.client_end },
             ( struct size_total ){ heap.heap_size, tree.total }
         ) ) {
        return false;
    }
    if ( !is_rbtree_mem_valid( tree.root, tree.black_nil, total_free_mem ) ) {
        return false;
    }
    if ( is_red_red( tree.root, tree.black_nil ) ) {
        return false;
    }
    if ( !is_bheight_valid( tree.root, tree.black_nil ) ) {
        return false;
    }
    if ( !is_parent_valid( tree.root, tree.black_nil ) ) {
        return false;
    }
    if ( !is_bheight_valid_v2( tree.root, tree.black_nil ) ) {
        return false;
    }
    if ( !are_subtrees_valid( tree.root, tree.black_nil ) ) {
        return false;
    }
    return true;
}

size_t wheap_align( size_t request )
{
    return roundup( request, ALIGNMENT );
}

size_t wheap_capacity( void )
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

void wheap_diff( const struct heap_block expected[], struct heap_block actual[], size_t len )
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
        actual[len - 1].err = HEAP_CONTINUES;
    }
}

/////////////////////////     Shared Printer    /////////////////////////////////////////

void wprint_free_nodes( enum print_style style )
{
    printf( "\n" );
    print_rb_tree( tree.root, tree.black_nil, style );
}

void wdump_heap( void )
{
    print_all(
        ( struct heap_range ){ heap.client_start, heap.client_end }, heap.heap_size, tree.root, tree.black_nil
    );
}

///////////////////////    Static Heap Helper Functions   /////////////////////////////////

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

static void init_free_node( struct rb_node *to_free, size_t block_size )
{
    to_free->header = block_size | LEFT_ALLOCATED | RED_PAINT;
    init_footer( to_free, block_size );
    get_right_neighbor( to_free, block_size )->header &= LEFT_FREE;
    insert_rb_node( to_free );
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
        report->current = delete_rb_node( report->left );
    }
    if ( report->right ) {
        report->right = delete_rb_node( report->right );
    }
    init_header_size( report->current, report->available );
}

///////////////////   Red Black Tree Best Fit Search and Deletion   /////////////////////////////

static struct rb_node *find_best_fit( size_t key )
{
    if ( tree.root == tree.black_nil ) {
        return tree.black_nil;
    }
    struct rb_node *seeker = tree.root;
    size_t best_fit_size = ULLONG_MAX;
    struct rb_node *remove = seeker;
    while ( seeker != tree.black_nil ) {
        size_t seeker_size = get_size( seeker->header );
        if ( key == seeker_size ) {
            best_fit_size = key;
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
    if ( best_fit_size < key || best_fit_size == ULLONG_MAX ) {
        return tree.black_nil;
    }
    // We can decompose the Cormen et.al. deletion logic because coalesce and malloc use it.
    return delete_rb_node( remove );
}

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

////////////////////////     Red-Black Tree Insertion Logic   ////////////////////////////

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
    ++tree.total;
}

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

////////////////////////    Rotation Logic    ////////////////////////////////////

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

static inline struct rb_node *get_min( struct rb_node *root, struct rb_node *black_nil )
{
    for ( ; root->left != black_nil; root = root->left ) {}
    return root;
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
            ++total_free_nodes;
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

static bool is_bheight_valid( const struct rb_node *root, const struct rb_node *black_nil )
{
    return calculate_bheight( root, black_nil ) != -1;
}

static size_t extract_tree_mem( const struct rb_node *root, const struct rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 0UL;
    }
    return get_size( root->header ) + HEADERSIZE + extract_tree_mem( root->right, black_nil )
           + extract_tree_mem( root->left, black_nil );
}

static bool
is_rbtree_mem_valid( const struct rb_node *root, const struct rb_node *black_nil, size_t total_free_mem )
{
    if ( total_free_mem != extract_tree_mem( root, black_nil ) ) {
        BREAKPOINT();
        return false;
    }
    return true;
}

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
    return strict_bound_met( root->left, root_size, dir, nil )
           && strict_bound_met( root->right, root_size, dir, nil );
}

static bool are_subtrees_valid( const struct rb_node *root, const struct rb_node *nil )
{
    if ( root == nil ) {
        return true;
    }
    size_t root_size = get_size( root->header );
    if ( !strict_bound_met( root->left, root_size, L, nil )
         || !strict_bound_met( root->right, root_size, R, nil ) ) {
        BREAKPOINT();
        return false;
    }
    return are_subtrees_valid( root->left, nil ) && are_subtrees_valid( root->right, nil );
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
        printf( "%s(bh: %d)%s", COLOR_BLK, get_black_height( root, black_nil ), COLOR_NIL );
    }
    printf( "\n" );
}

static void print_inner_tree(
    const struct rb_node *root,
    const struct rb_node *black_nil,
    const char *prefix,
    const enum print_link node_type,
    enum print_style style
)
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
    // We should be able to see the header is the same as the footer. However, due to fixup functions,
    // the color may change for nodes and color is irrelevant to footers.
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
    // that is ok. we will only worry about the next node's color when we delete a duplicate.
    printf( COLOR_NIL );
    printf( "%*c", indent_struct_fields, ' ' );
    printf( "FTR->0x%016zX\n", to_print );
}

static void print_error_block( const struct rb_node *node, size_t block_size )
{
    printf( "\n%p: HDR->0x%016zX->%zubyts\n", node, node->header, block_size );
    printf( "Block size is too large and header is corrupted.\n" );
}

static void
print_bad_jump( const struct rb_node *current, const struct bad_jump j, const struct rb_node *black_nil )
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

static void print_all( struct heap_range r, size_t heap_size, struct rb_node *root, struct rb_node *black_nil )
{
    struct rb_node *node = r.start;
    printf(
        "Heap client segment starts at address %p, ends %p. %zu total bytes "
        "currently used.\n",
        node,
        r.end,
        heap_size
    );
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
