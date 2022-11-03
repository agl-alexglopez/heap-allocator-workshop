/**
 * File: rbtree_linked_printer.h
 * -----------------------------
 * This file contains the interface for printing information about the rbtree_linked allocator. We
 * use these in gdb while debugging, mostly. However, one of these functions is helpful for
 * visualizations and is used in the print_peaks program.
 */
#ifndef RBTREE_LINKED_PRINTER_H
#define RBTREE_LINKED_PRINTER_H

#include "print_utility.h"
#include "rbtree_linked_utility.h"


/* * * * * * * * * * * * * *            Printing Functions         * * * * * * * * * * * * * * * */


/* @brief get_black_height  gets the black node height of the tree excluding the current node.
 * @param *root             the starting root to search from to find the height.
 * @param *black_nil        the sentinel node at the bottom of the tree that is always black.
 * @return                  the black height from the current node as an integer.
 */
int get_black_height(const rb_node *root, const rb_node *black_nil);

/* @brief print_node     prints an individual node in its color and status as left or right child.
 * @param *root          the root we will print with the appropriate info.
 * @param *nil_and_tail  address of a sentinel node serving as both list tail and black nil.
 * @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
 */
void print_node(const rb_node *root, void *nil_and_tail, print_style style);

/* @brief print_inner_tree  recursively prints the contents of a red black tree with color and in
 *                          a style similar to a directory structure to be read from left to right.
 * @param *root             the root node to start at.
 * @param *nil_and_tail     address of a sentinel node serving as both list tail and black nil.
 * @param *prefix           the string we print spacing and characters across recursive calls.
 * @param node_type         the node to print can either be a leaf or internal branch.
 * @param style             the print style: PLAIN or VERBOSE(displays memory addresses).
 */
void print_inner_tree(const rb_node *root, void *nil_and_tail, const char *prefix,
                      const print_link node_type, print_style style);

/* @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
 * @param *root          the root node to begin at for printing recursively.
 * @param *nil_and_tail  address of a sentinel node serving as both list tail and black nil.
 * @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
 */
void print_rb_tree(const rb_node *root, void *nil_and_tail, print_style style);

/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *node              a valid rb_node to a block of allocated memory.
 */
void print_alloc_block(const rb_node *node);

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *header           a valid header to a block of allocated memory.
 */
void print_free_block(const rb_node *node);

/* @brief print_error_block  prints a helpful error message if a block is corrupted.
 * @param *node              a header to a block of memory.
 * @param block_size         the full size of a block of memory, not just the user block size.
 */
void print_error_block(const rb_node *node, size_t block_size);

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 * @param *root           the root node to begin at for printing recursively.
 * @param *nil_and_tail   address of a sentinel node serving as both list tail and black nil.
 */
void print_bad_jump(const rb_node *curr, const rb_node *prev, rb_node *root, void *nil_and_tail);

/* @brief print_all    prints our the complete status of the heap, all of its blocks, and the sizes
 *                     the blocks occupy. Printing should be clean with no overlap of unique id's
 *                     between heap blocks or corrupted headers.
 * @param client_start the starting address of the heap segment.
 * @param client_end   the final address of the heap segment.
 * @param heap_size    the size in bytes of the
 * @param *root        the root of the tree we start at for printing.
 * @param *black_nil   the sentinel node that waits at the bottom of the tree for all leaves.
 */
void print_all(void *client_start, void *client_end, size_t heap_size, rb_node *tree_root,
               rb_node *black_nil);

#endif
