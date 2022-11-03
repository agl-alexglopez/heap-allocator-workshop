/**
 * File: rbtree_unified_printer.h
 * ------------------------------
 * This file contains the printing interface for the rbtree_unified allocator. They are most
 * helpful while debugging in gdb but one function is used for visualizations in the print_peaks
 * program.
 */
#ifndef RBTREE_UNIFIED_PRINTER_H
#define RBTREE_UNIFIED_PRINTER_H

#include <stdbool.h>
#include <stddef.h>
#include "rbtree_unified_utility.h"


/* * * * * * * * * * * * * *         Printing Functions            * * * * * * * * * * * * * * * */


/* @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
 * @param *root          the root node to begin at for printing recursively.
 * @param *black_nil     the sentinel node at the bottom of the tree that is always black.
 * @param style          the print style: PLAIN or VERBOSE(displays memory addresses).
 */
void print_rb_tree(const rb_node *root, const rb_node *black_nil, print_style style);

/* @brief print_all    prints our the complete status of the heap, all of its blocks, and the sizes
 *                     the blocks occupy. Printing should be clean with no overlap of unique id's
 *                     between heap blocks or corrupted headers.
 * @param client_start the starting address of the heap segment.
 * @param client_end   the final address of the heap segment.
 * @param heap_size    the size in bytes of the heap.
 * @param *root        the root of the tree we start at for printing.
 * @param *black_nil   the sentinel node that waits at the bottom of the tree for all leaves.
 */
void print_all(void *client_start, void *client_end, size_t heap_size, rb_node *root,
               rb_node *black_nil);


#endif
