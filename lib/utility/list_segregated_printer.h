/**
 * File: list_segregated_printer.h
 * -------------------------------
 * This file contains the printing interface for the list_segregated allocator. This is mostly
 * helpful for debugging while in gdb, but one function also makes an appearance in the print_peaks
 * program to help visualize the heap.
 */
#ifndef LIST_SEGREGATED_PRINTER_H
#define LIST_SEGREGATED_PRINTER_H

#include <stdbool.h>
#include <stddef.h>
#include "print_utility.h"
#include "list_segregated_design.h"


/* * * * * * * * * * * * * *         Printing Functions            * * * * * * * * * * * * * * * */


/* @brief print_all    prints our the complete status of the heap, all of its blocks, and the sizes
 *                     the blocks occupy. Printing should be clean with no overlap of unique id's
 *                     between heap blocks or corrupted headers.
 * @param client_start the starting address of the heap segment.
 * @param client_end   the final address of the heap segment.
 * @param client_size  the size in bytes of the heap.
 * @param table[]      the lookup table of segregated sizes of nodes stored in each slot.
 * @param *nil         the free node that serves as a universal head and tail to all lists.
 */
void print_all(void *client_start, void *client_end, size_t client_size,
               seg_node table[], free_node *nil);

/* @brief print_fits  prints the segregated fits free list in order to check if splicing and
 *                    adding is progressing correctly.
 * @param table[]     the lookup table that holds the list size ranges
 * @param *nil        a special free_node that serves as a sentinel for logic and edgecases.
 */
void print_fits(print_style style, seg_node table[], free_node *nil);


#endif
