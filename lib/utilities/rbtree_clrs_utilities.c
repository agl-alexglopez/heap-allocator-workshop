/// File rbtree_clrs_design.c
/// ---------------------------------
/// This file contains the implementation of utility functions for the
/// rbtree_clrs heap allocator. These functions serve as basic navigation for
/// nodes and blocks. We defined them as inline in the header file so we declare
/// them here and hope the compiler inlines them in "hot" functions.
#include "rbtree_clrs_utilities.h"
#include "debug_break.h"
#include "print_utility.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// NOLINTBEGIN(misc-no-recursion)

/////////////////////////////    Debugging and Testing Functions   //////////////////////////////////

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

/// @brief is_red_red  determines if a red red violation of a red black tree has occured.
/// @param *root       the current root of the tree to begin at for checking all subtrees.
/// @param *black_nil  the sentinel node at the bottom of the tree that is always black.
/// @return            true if there is a red-red violation, false if we pass.
bool is_red_red( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil || ( root->right == black_nil && root->left == black_nil ) ) {
        return false;
    }
    if ( get_color( root->header ) == RED ) {
        if ( get_color( root->left->header ) == RED || get_color( root->right->header ) == RED ) {
            breakpoint();
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
static int calculate_bheight( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 0;
    }
    int lf_bheight = calculate_bheight( root->left, black_nil );
    int rt_bheight = calculate_bheight( root->right, black_nil );
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

size_t extract_tree_mem( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 0UL;
    }
    return get_size( root->header ) + HEADERSIZE + extract_tree_mem( root->right, black_nil )
           + extract_tree_mem( root->left, black_nil );
}

bool is_rbtree_mem_valid( const rb_node *root, const rb_node *black_nil, size_t total_free_mem )
{
    return total_free_mem == extract_tree_mem( root, black_nil );
}

bool is_parent_valid( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil ) {
        return true;
    }
    if ( root->left != black_nil && root->left->parent != root ) {
        breakpoint();
        return false;
    }
    if ( root->right != black_nil && root->right->parent != root ) {
        breakpoint();
        return false;
    }
    return is_parent_valid( root->left, black_nil ) && is_parent_valid( root->right, black_nil );
}

static int calculate_bheight_v2( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 1;
    }
    int left_height = calculate_bheight_v2( root->left, black_nil );
    int right_height = calculate_bheight_v2( root->right, black_nil );
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
    if ( root->left != black_nil && root_value < get_size( root->left->header ) ) {
        breakpoint();
        return false;
    }
    if ( root->right != black_nil && root_value > get_size( root->right->header ) ) {
        breakpoint();
        return false;
    }
    return is_binary_tree( root->left, black_nil ) && is_binary_tree( root->right, black_nil );
}

/////////////////////////////        Printing Functions            //////////////////////////////////

static int get_black_height( const rb_node *root, const rb_node *black_nil )
{
    if ( root == black_nil ) {
        return 0;
    }
    if ( get_color( root->left->header ) == BLACK ) {
        return 1 + get_black_height( root->left, black_nil );
    }
    return get_black_height( root->left, black_nil );
}

static void print_node( const rb_node *root, const rb_node *black_nil, print_style style )
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
static void print_inner_tree( const rb_node *root, const rb_node *black_nil, const char *prefix,
                              const print_link node_type, print_style style )
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
static void print_error_block( const rb_node *node, size_t block_size )
{
    printf( "\n%p: HDR->0x%016zX->%zubyts\n", node, node->header, block_size );
    printf( "Block size is too large and header is corrupted.\n" );
}

/// @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
///                        notice where we went wrong and what the addresses were.
/// @param bad_jump        two nodes with a bad jump from one to the other
/// @param *root           the root node of the tree to start at for an overall heap check.
/// @param *black_nil      the sentinel node at the bottom of the tree that is always black.
static void print_bad_jump( const rb_node *current, const bad_jump j, const rb_node *black_nil )
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

void print_rb_tree( const rb_node *root, const rb_node *black_nil, print_style style )
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

void print_all( heap_range r, size_t heap_size, rb_node *root, rb_node *black_nil )
{
    rb_node *node = r.start;
    printf( "Heap client segment starts at address %p, ends %p. %zu total bytes "
            "currently used.\n",
            node, r.end, heap_size );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "COLOR KEY: " COLOR_BLK "[BLACK NODE] " COLOR_NIL COLOR_RED "[RED NODE] " COLOR_NIL COLOR_GRN
            "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "%p: START OF  HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", r.start );
    rb_node *prev = node;
    while ( node != r.end ) {
        size_t full_size = get_size( node->header );

        if ( full_size == 0 ) {
            print_bad_jump( node, ( bad_jump ){ prev, root }, black_nil );
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
    printf( "COLOR KEY: " COLOR_BLK "[BLACK NODE] " COLOR_NIL COLOR_RED "[RED NODE] " COLOR_NIL COLOR_GRN
            "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "\nRED BLACK TREE OF FREE NODES AND BLOCK SIZES.\n" );
    printf( "HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n" );
    printf( COLOR_CYN "(+X)" COLOR_NIL );
    printf( " INDICATES DUPLICATE NODES IN THE  THEY HAVE A NEXT NODE.\n" );
    print_rb_tree( root, black_nil, VERBOSE );
}

// NOLINTEND(misc-no-recursion)
