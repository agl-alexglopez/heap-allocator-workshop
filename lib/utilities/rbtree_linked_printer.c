/**
 * File: rbtree_linked_printer.c
 * -----------------------------
 * This file contains implementations for printing information on the
 * rbtree_linked allocator. We use these in gdb while debugging, mostly.
 * However, one of these functions is helpful for visualizations and is used in
 * the print_peaks program.
 */
#include "rbtree_linked_utilities.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* * * * * * * * * * * * * *            Printing Functions         * * * * * * *
 * * * * * * * * * */

/* @brief get_black_height  gets the black node height of the tree excluding the
 * current node.
 * @param *root             the starting root to search from to find the height.
 * @param *black_nil        the sentinel node at the bottom of the tree that is
 * always black.
 * @return                  the black height from the current node as an
 * integer.
 */
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

/* @brief print_node     prints an individual node in its color and status as
 * left or right child.
 * @param *root          the root we will print with the appropriate info.
 * @param *nil_and_tail  address of a sentinel node serving as both list tail
 * and black nil.
 * @param style          the print style: PLAIN or VERBOSE(displays memory
 * addresses).
 */
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

/* @brief print_inner_tree  recursively prints the contents of a red black tree
 * with color and in a style similar to a directory structure to be read from
 * left to right.
 * @param *root             the root node to start at.
 * @param *nil_and_tail     address of a sentinel node serving as both list tail
 * and black nil.
 * @param *prefix           the string we print spacing and characters across
 * recursive calls.
 * @param node_type         the node to print can either be a leaf or internal
 * branch.
 * @param style             the print style: PLAIN or VERBOSE(displays memory
 * addresses).
 */
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
    int string_length = snprintf( NULL, 0, "%s%s", prefix, node_type == LEAF ? "     " : " │   " );
    if ( string_length > 0 ) {
        str = malloc( string_length + 1 );
        snprintf( str, string_length, "%s%s", prefix, node_type == LEAF ? "     " : " │   " );
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

/* @brief print_rb_tree  prints the contents of an entire rb tree in a directory
 * tree style.
 * @param *root          the root node to begin at for printing recursively.
 * @param *nil_and_tail  address of a sentinel node serving as both list tail
 * and black nil.
 * @param style          the print style: PLAIN or VERBOSE(displays memory
 * addresses).
 */
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

/* @brief print_alloc_block  prints the contents of an allocated block of
 * memory.
 * @param *node              a valid rb_node to a block of allocated memory.
 */
static void print_alloc_block( const rb_node *node )
{
    size_t block_size = get_size( node->header );
    // We will see from what direction our header is messed up by printing 16
    // digits.
    printf( COLOR_GRN "%p: HDR->0x%016zX(%zubytes)\n" COLOR_NIL, node, node->header, block_size );
}

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *header           a valid header to a block of allocated memory.
 */
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

    /* The next and footer fields may not match the current node's color bit, and
     * that is ok. we will only worry about the next node's color when we delete a
     * duplicate.
     */
    printf( COLOR_NIL );
    printf( "%*c", indent_struct_fields, ' ' );
    printf( "LST->%p\n", node->list_start ? node->list_start : NULL );
    printf( "%*c", indent_struct_fields, ' ' );
    printf( "FTR->0x%016zX\n", to_print );
}

/* @brief print_error_block  prints a helpful error message if a block is
 * corrupted.
 * @param *header            a header to a block of memory.
 * @param full_size          the full size of a block of memory, not just the
 * user block size.
 */
static void print_error_block( const rb_node *node, size_t block_size )
{
    printf( "\n%p: HDR->0x%016zX->%zubyts\n", node, node->header, block_size );
    printf( "Block size is too large and header is corrupted.\n" );
}

/* @brief print_bad_jump  If we overwrite data in a header, this print statement
 * will help us notice where we went wrong and what the addresses were.
 * @param *curr           the current node that is likely garbage values that
 * don't make sense.
 * @param *prev           the previous node that we jumped from.
 * @param *root           the root node to begin at for printing recursively.
 * @param *nil_and_tail   address of a sentinel node serving as both list tail
 * and black nil.
 */
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

/* @brief print_all    prints our the complete status of the heap, all of its
 * blocks, and the sizes the blocks occupy. Printing should be clean with no
 * overlap of unique id's between heap blocks or corrupted headers.
 * @param client_start the starting address of the heap segment.
 * @param client_end   the final address of the heap segment.
 * @param heap_size    the size in bytes of the
 * @param *root        the root of the tree we start at for printing.
 * @param *black_nil   the sentinel node that waits at the bottom of the tree
 * for all leaves.
 */
void print_all( void *client_start, void *client_end, size_t heap_size, rb_node *tree_root, rb_node *black_nil )
{
    rb_node *node = client_start;
    printf( "Heap client segment starts at address %p, ends %p. %zu total bytes "
            "currently used.\n",
            node, client_end, heap_size );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "COLOR KEY: " COLOR_BLK "[BLACK NODE] " COLOR_NIL COLOR_RED "[RED NODE] " COLOR_NIL COLOR_GRN
            "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "%p: START OF  HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", client_start );
    rb_node *prev = node;
    while ( node != client_end ) {
        size_t full_size = get_size( node->header );

        if ( full_size == 0 ) {
            print_bad_jump( node, prev, tree_root, black_nil );
            printf( "Last known pointer before jump: %p", prev );
            return;
        }
        if ( (void *)node > client_end ) {
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
    printf( "%p: FINAL ADDRESS", (byte *)client_end + HEAP_NODE_WIDTH );
    printf( "\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "COLOR KEY: " COLOR_BLK "[BLACK NODE] " COLOR_NIL COLOR_RED "[RED NODE] " COLOR_NIL COLOR_GRN
            "[ALLOCATED BLOCK]" COLOR_NIL "\n\n" );

    printf( "\nRED BLACK TREE OF FREE NODES AND BLOCK SIZES.\n" );
    printf( "HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n" );
    printf( COLOR_CYN "(+X)" COLOR_NIL );
    printf( " INDICATES DUPLICATE NODES IN THE TREE. THEY HAVE A N NODE.\n" );
    print_rb_tree( tree_root, black_nil, VERBOSE );
}
