/* File: allocator.h
 * -----------------
 * Interface file for the custom heap allocator.
 */
#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include <stdbool.h> // for bool
#include <stddef.h>  // for size_t

// Alignment requirement for all blocks
#define ALIGNMENT 8

// maximum size of block that must be accommodated
#define MAX_REQUEST_SIZE (1 << 30)



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

#endif
