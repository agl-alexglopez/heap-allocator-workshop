/**
 * File: list_bestfit_printer.h
 * -----------------------------
 * This interface defines the printer functions for the list_bestfit allocator. While one of these
 * is used in the print_peaks program, mostly these are helpful to use while debugging in gdb.
 */
#ifndef LIST_BESTFIT_PRINTER_H
#define LIST_BESTFIT_PRINTER_H

#include <stdio.h>
#include "print_utility.h"
#include "list_bestfit_design.h"

/* * * * * * * * * * * * * *         Printing Functions            * * * * * * * * * * * * * * * */

/* @brief print_all    prints our the complete status of the heap, all of its blocks, and the sizes
 *                     the blocks occupy. Printing should be clean with no overlap of unique id's
 *                     between heap blocks or corrupted headers.
 * @param client_start the starting address of the heap segment.
 * @param client_end   the final address of the heap segment.
 * @param client_size  the size in bytes of the heap.
 * @param *head        the first node of the doubly linked list of nodes.
 * @param *tail        the last node of the doubly linked list of nodes.
 */
void print_all(void *client_start, void *client_end, size_t client_size,
               free_node *head, free_node *tail);

/* @brief print_linked_free  prints the doubly linked free list in order to check if splicing and
 *                           adding is progressing correctly.
 * @param *head              the free_node head of the linked list.
 * @param *tail              the free_node tail of the linked list.
 */
void print_linked_free(print_style style, free_node *head, free_node *tail);


#endif
