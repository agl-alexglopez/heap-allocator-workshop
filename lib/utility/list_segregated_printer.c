/**
 * File: list_segregated_printer.c
 * -------------------------------
 * This file contains the printing implementation for the list_segregated allocator. This is mostly
 * helpful for debugging while in gdb, but one function also makes an appearance in the print_peaks
 * program to help visualize the heap.
 */
#include <limits.h>
#include <stdio.h>
#include "list_segregated_printer.h"
#include "print_utility.h"


/* * * * * * * * * * * * * *         Printing Functions            * * * * * * * * * * * * * * * */


/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *cur_header        a valid header to a block of allocated memory.
 */
static void print_alloc_block(header *cur_header) {
    size_t block_size = get_size(*cur_header) - HEADERSIZE;
    printf(COLOR_GRN);
    // We will see from what direction our header is messed up by printing 16 digits.
    printf("%p: HEADER->0x%016zX->[ALOC-%zubytes]\n", cur_header, *cur_header, block_size);
    printf(COLOR_NIL);
}

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *cur_header       a valid header to a block of allocated memory.
 */
static void print_free_block(header *cur_header) {
    size_t full_size = get_size(*cur_header);
    size_t block_size = full_size - HEADERSIZE;
    header *footer = (header *)((byte *)cur_header + full_size - HEADERSIZE);
    /* We should be able to see the header is the same as the footer. If they are not the same
     * we will face subtle bugs that are very hard to notice.
     */
    if (*footer != *cur_header) {
        *footer = ULONG_MAX;
    }
    printf(COLOR_RED);
    printf("%p: HEADER->0x%016zX->[FREE-%zubytes->FOOTER->%016zX]\n",
            cur_header, *cur_header, block_size, *footer);
    printf(COLOR_NIL);
}

/* @brief print_error_block  prints a helpful error message if a block is corrupted.
 * @param *cur_header        a header to a block of memory.
 * @param full_size          the full size of a block of memory, not just the user block size.
 */
static void print_error_block(header *cur_header, size_t full_size) {
    size_t block_size = full_size - HEADERSIZE;
    printf(COLOR_CYN);
    printf("\n%p: HEADER->0x%016zX->%zubytes\n",
            cur_header, *cur_header, block_size);
    printf("Block size is too large and header is corrupted.\n");
    printf(COLOR_NIL);
}

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 * @param table[]         the lookup table that holds the list size ranges
 * @param *nil            a special free_node that serves as a sentinel for logic and edgecases.
 */
static void print_bad_jump(header *current, header *prev, seg_node table[], free_node *nil) {
    size_t prev_size = get_size(*prev);
    size_t cur_size = get_size(*current);
    printf(COLOR_CYN);
    printf("A bad jump from the value of a header has occured. Bad distance to next header.\n");
    printf("The previous address: %p:\n", prev);
    printf("\tHeader Hex Value: %016zX:\n", *prev);
    printf("\tBlock Byte Value: %zubytes:\n", prev_size);
    printf("\nJump by %zubytes...\n", prev_size);
    printf("The current address: %p:\n", current);
    printf("\tHeader Hex Value: %016zX:\n", *current);
    printf("\tBlock Byte Value: %zubytes:\n", cur_size);
    printf("\nJump by %zubytes...\n", cur_size);
    printf("Current state of the free list:\n");
    printf(COLOR_NIL);
    print_fits(VERBOSE, table, nil);
}

/* @brief print_fits  prints the segregated fits free list in order to check if splicing and
 *                    adding is progressing correctly.
 * @param table[]     the lookup table that holds the list size ranges
 * @param *nil        a special free_node that serves as a sentinel for logic and edgecases.
 */
void print_fits(print_style style, seg_node table[], free_node *nil) {
    bool alternate = false;
    for (int i = 0; i < TABLE_SIZE; i++, alternate = !alternate) {
        printf(COLOR_GRN);
        if (style == VERBOSE) {
            printf("%p: ", &table[i]);
        }
        if (i == TABLE_SIZE - 1) {
            printf("[CLASS:%ubytes+]=>", table[i].size);
        } else if (i >= SMALL_TABLE_SIZE) {
            printf("[CLASS:%u-%ubytes]=>", table[i].size, table[i + 1].size - 1U);
        } else {
            printf("[CLASS:%ubytes]=>", table[i].size);
        }
        printf(COLOR_NIL);
        if (alternate) {
            printf(COLOR_RED);
        } else {
            printf(COLOR_CYN);
        }

        for (free_node *cur = table[i].start; cur != nil; cur = cur->next) {
            if (cur) {
                header *cur_header = get_block_header(cur);
                printf("<=>[");
                if (style == VERBOSE) {
                    printf("%p:", get_block_header(cur));
                }
                printf("(%zubytes)]", get_size(*cur_header));
            } else {
                printf("Something went wrong. NULL free fits node.\n");
                break;
            }
        }
        printf("<=>[%p]\n", nil);
        printf(COLOR_NIL);
    }
}

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
               seg_node table[], free_node *nil) {
    header *cur_header = client_start;
    printf("Heap client segment starts at address %p, ends %p. %zu total bytes currently used.\n",
            cur_header, client_end, client_size);
    printf("A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n\n");

    printf("%p: FIRST ADDRESS\n", table);

    // This will create large amount of output but realistically table is before the rest of heap.
    print_fits(VERBOSE, table, nil);
    printf("--END OF LOOKUP TABLE, START OF HEAP--\n");

    header *prev = cur_header;
    while (cur_header != client_end) {
        size_t full_size = get_size(*cur_header);

        if (full_size == 0) {
            print_bad_jump(cur_header, prev, table, nil);
            printf("Last known pointer before jump: %p", prev);
            return;
        }

        if (is_block_allocated(*cur_header)) {
            print_alloc_block(cur_header);
        } else {
            print_free_block(cur_header);
        }
        prev = cur_header;
        cur_header = get_right_header(cur_header, full_size);
    }
    printf("%p: END OF HEAP\n", client_end);
    printf(COLOR_RED);
    printf("<-%p:SENTINEL->\n", nil);
    printf(COLOR_NIL);
    printf("%p: LAST ADDRESS\n", (byte *)nil + FREE_NODE_WIDTH);
    printf("\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n");
    printf("\nSEGREGATED LIST OF FREE NODES AND BLOCK SIZES.\n");
    printf("HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n");
    // For large heaps we wouldn't be able to scroll to the table location so print again here.
    print_fits(VERBOSE, table, nil);
}
