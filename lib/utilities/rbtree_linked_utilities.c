/// File rbtree_linked_design.c
/// ---------------------------------
/// This file contains the implementation of utility functions for the
/// rbtree_linked heap allocator. These functions serve as basic navigation for
/// nodes and blocks. These functions can distract from the algorithm
/// implementations in the actual rbtree_linked.c file so we seperate them out
/// here.
#include "rbtree_linked_utilities.h"
#include "debug_break.h"
#include "print_utility.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/////////////////////////////    Debugging and Testing Functions   //////////////////////////////////

// NOLINTBEGIN(misc-no-recursion)

bool check_init( heap_range r, size_t heap_size )
{
    if ( is_left_space( r.start ) ) {
        breakpoint();
        return false;
    }
    if ( (byte *)r.end - (byte *)r.start + HEAP_NODE_WIDTH != heap_size ) {
        breakpoint();
        return false;
    }
    return true;
}

bool is_memory_balanced( size_t *total_free_mem, heap_range r, size_total s )
{
    // Check that after checking all headers we end on size 0 tail and then end of
    // address space.
    rb_node *cur_node = r.start;
    size_t size_used = HEAP_NODE_WIDTH;
    size_t total_free_nodes = 0;
    while ( cur_node != r.end ) {
        size_t block_size_check = get_size( cur_node->header );
        if ( block_size_check == 0 ) {
            breakpoint();
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
        breakpoint();
        return false;
    }
    if ( total_free_nodes != s.total ) {
        breakpoint();
        return false;
    }
    return true;
}

bool is_red_red( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil || ( root->links[R] == black_nil && root->links[L] == black_nil ) ) {
        return false;
    }
    if ( get_color( root->header ) == RED ) {
        if ( get_color( root->links[L]->header ) == RED || get_color( root->links[R]->header ) == RED ) {
            breakpoint();
            return true;
        }
    }
    return is_red_red( root->links[R], black_nil ) || is_red_red( root->links[L], black_nil );
}

int calculate_bheight( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 0;
    }
    int lf_bheight = calculate_bheight( root->links[L], black_nil );
    int rt_bheight = calculate_bheight( root->links[R], black_nil );
    int add = get_color( root->header ) == BLACK;
    if ( lf_bheight == -1 || rt_bheight == -1 || lf_bheight != rt_bheight ) {
        breakpoint();
        return -1;
    }
    return lf_bheight + add;
}

bool is_bheight_valid( const rb_node *root, const rb_node *black_nil )
{
    return calculate_bheight( root, black_nil ) != -1;
}

size_t extract_tree_mem( const rb_node *root, const void *nil_and_tail )
{
    if ( root == nil_and_tail ) {
        return 0UL;
    }
    // We may have repeats so make sure to add the linked list values.
    size_t total_mem = get_size( root->header ) + HEADERSIZE;
    for ( duplicate_node *tally_list = root->list_start; tally_list != nil_and_tail;
          tally_list = tally_list->links[N] ) {
        total_mem += get_size( tally_list->header ) + HEADERSIZE;
    }
    return total_mem + extract_tree_mem( root->links[R], nil_and_tail )
           + extract_tree_mem( root->links[L], nil_and_tail );
}

bool is_rbtree_mem_valid( const rb_node *root, const void *nil_and_tail, size_t total_free_mem )
{
    if ( extract_tree_mem( root, nil_and_tail ) != total_free_mem ) {
        breakpoint();
        return false;
    }
    return true;
}

bool is_parent_valid( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil ) {
        return true;
    }
    if ( root->links[L] != black_nil && root->links[L]->parent != root ) {
        breakpoint();
        return false;
    }
    if ( root->links[R] != black_nil && root->links[R]->parent != root ) {
        breakpoint();
        return false;
    }
    return is_parent_valid( root->links[L], black_nil ) && is_parent_valid( root->links[R], black_nil );
}

int calculate_bheight_v2( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 1;
    }
    int left_height = calculate_bheight_v2( root->links[L], black_nil );
    int right_height = calculate_bheight_v2( root->links[R], black_nil );
    if ( left_height != 0 && right_height != 0 && left_height != right_height ) {
        breakpoint();
        return 0;
    }
    if ( left_height != 0 && right_height != 0 ) {
        return get_color( root->header ) == RED ? left_height : left_height + 1;
    }
    return 0;
}

bool is_bheight_valid_v2( const rb_node *root, const rb_node *black_nil )
{
    return calculate_bheight_v2( root, black_nil ) != 0;
}

bool is_binary_tree( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil ) {
        return true;
    }
    size_t root_value = get_size( root->header );
    if ( root->links[L] != black_nil && root_value < get_size( root->links[L]->header ) ) {
        breakpoint();
        return false;
    }
    if ( root->links[R] != black_nil && root_value > get_size( root->links[R]->header ) ) {
        breakpoint();
        return false;
    }
    return is_binary_tree( root->links[L], black_nil ) && is_binary_tree( root->links[R], black_nil );
}

/////////////////////////////           Printing Functions         //////////////////////////////////

/// @brief get_black_height  gets the black node height of the tree excluding the current node.
/// @param *root             the starting root to search from to find the height.
/// @param *black_nil        the sentinel node at the bottom of the tree that is always black.
/// @return                  the black height from the current node as an integer.
static int get_black_height( const rb_node *root, const rb_node *black_nil )
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
static void print_node( const rb_node *root, void *nil_and_tail, print_style style )
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
        printf( "(bh: %d)", get_black_height( root, nil_and_tail ) );
    }

    printf( COLOR_CYN );
    // If a node is a duplicate, we will give it a special mark among nodes.
    if ( root->list_start != nil_and_tail ) {
        int duplicates = 1;
        duplicate_node *duplicate = root->list_start;
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
static void print_inner_tree( const rb_node *root, void *nil_and_tail, const char *prefix,
                              const print_link node_type, print_style style )
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

void print_rb_tree( const rb_node *root, void *nil_and_tail, print_style style )
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
/// @param *node              a valid rb_node to a block of allocated memory.
static void print_alloc_block( const rb_node *node )
{
    size_t block_size = get_size( node->header );
    // We will see from what direction our header is messed up by printing 16
    // digits.
    printf( COLOR_GRN "%p: HDR->0x%016zX(%zubytes)\n" COLOR_NIL, node, node->header, block_size );
}

/// @brief print_free_block  prints the contents of a free block of heap memory.
/// @param *header           a valid header to a block of allocated memory.
static void print_free_block( const rb_node *node )
{
    size_t block_size = get_size( node->header );
    header *footer = (header *)( (byte *)node + block_size );
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
static void print_error_block( const rb_node *node, size_t block_size )
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
static void print_bad_jump( const rb_node *curr, const rb_node *prev, rb_node *root, void *nil_and_tail )
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

void print_all( heap_range r, size_t heap_size, rb_node *tree_root, rb_node *black_nil )
{
    rb_node *node = r.start;
    printf( "Heap client segment starts at address %p, ends %p. %zu total bytes "
            "currently used.\n",
            node, r.end, heap_size );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "COLOR KEY: " COLOR_BLK "[BLACK NODE] " COLOR_NIL COLOR_RED "[rED NODE] " COLOR_NIL COLOR_GRN
            "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "%p: START OF  HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", r.start );
    rb_node *prev = node;
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
    printf( "%p: FINAL ADDRESS", (byte *)r.end + HEAP_NODE_WIDTH );
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
