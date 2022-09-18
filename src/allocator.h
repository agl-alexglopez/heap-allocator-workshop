/* File: allocator.h
 * -----------------
 * Interface file for the custom heap allocator.
 */
#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include <stdbool.h> // for bool
#include <stddef.h>  // for size_t

// PLAIN prints free block sizes, VERBOSE shows address in the heap and black height of tree.
typedef enum print_style {
    PLAIN = 0,
    VERBOSE = 1
}print_style;

// Printing enum for printing red black tree structure.
typedef enum print_link {
    BRANCH = 0, // ├──
    LEAF = 1    // └──
}print_link;

// Alignment requirement for all blocks
#define ALIGNMENT 8

// maximum size of block that must be accommodated
#define MAX_REQUEST_SIZE (1 << 30)

/* Text coloring macros (ANSI character escapes) for printing function colorful output. Consider
 * changing to a more portable library like ncurses.h. However, I don't want others to install
 * ncurses just to explore the project. They already must install gnuplot. Hope this works.
 */
#define COLOR_BLK "\033[34;1m"
#define COLOR_RED "\033[31;1m"
#define COLOR_CYN "\033[36;1m"
#define COLOR_GRN "\033[32;1m"
#define COLOR_NIL "\033[0m"
#define COLOR_ERR COLOR_RED "Error: " COLOR_NIL
#define PRINTER_INDENT (short)13


/* Function: myinit
 * ----------------
 * This must be called by a client before making any allocation
 * requests.  The function returns true if initialization was successful,
 * or false otherwise. The myinit function can be called to reset
 * the heap to an empty state. When running against a set of
 * of test scripts, our test harness calls myinit before starting
 * each new script.
 */
bool myinit(void *heap_start, size_t heap_size);

/* Function: mymalloc
 * ------------------
 * Custom version of malloc.
 */
void *mymalloc(size_t requested_size);


/* Function: myrealloc
 * -------------------
 * Custom version of realloc.
 */
void *myrealloc(void *old_ptr, size_t new_size);


/* Function: myfree
 * ----------------
 * Custom version of free.
 */
void myfree(void *ptr);


/* Function: validate_heap
 * -----------------------
 * This is the hook for your heap consistency checker. Returns true
 * if all is well, or false on any problem.  This function is
 * called periodically by the test harness to check the state of
 * the heap allocator.
 */
bool validate_heap();

/* Function: get_free_total
 * ------------------------
 * Simple function to get the size of whatever data structure the allocator is using to manage free
 * nodes in the heap. Garunteed to be an O(1) operation.
 */
size_t get_free_total();

/* Function: print_free_nodes
 * --------------------------
 * Prints a visual representation of the free nodes in the heap in the form of the data structure
 * being used to manage them. You can print the nodes in the PLAIN or VERBOSE style. Plain will
 * only show the sizes in bytes that the blocks store, while VERBOSE will show their addresses in
 * the heap and for the tree allocators, the black height of the tree as well.
 */
void print_free_nodes(print_style style);

#endif
