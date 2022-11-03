/**
 * File: list_addressorder_printer.h
 * ---------------------------------
 * This file contains functions that help visualize the heap for debugging purposes. These are
 * particularly helpful to use while in gdb.
 */
#ifndef LIST_ADDRESSORDER_PRINTER_H
#define LIST_ADDRESSORDER_PRINTER_H
#include "list_addressorder_utility.h"
#include "print_utility.h"


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
