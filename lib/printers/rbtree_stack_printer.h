/**
 * File: rbtree_stack_printer.h
 * ----------------------------
 * This file contains the printing utilities for the rbtree_stack allocator. These are most helpful
 * while debugging in gdb. However, we use one of these functions to help visualize the heap in
 * the print_peaks program.
 */
#ifndef RBTREE_STACK_PRINTER_H
#define RBTREE_STACK_PRINTER_H

#include "print_utility.h"
#include "rbtree_stack_design.h"


/* * * * * * * * * * * * * *            Printing Functions         * * * * * * * * * * * * * * * */


/* @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
 * @param *root          the root node to begin at for printing recursively.
 * @param *nil_and_tail  address of a sentinel node serving as both list tail and black nil.
 * @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
 */
void print_rb_tree(const rb_node *root, const void *nil_and_tail, print_style style);

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
