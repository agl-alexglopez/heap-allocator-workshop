/// File: allocator.h
/// -----------------
/// Interface file for the custom heap allocator.
#ifdef __cplusplus
extern "C" {
#endif
#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "print_utility.h"
#include <stdbool.h>
#include <stddef.h>

enum
{
    // Alignment requirement for all blocks
    ALIGNMENT = 8,
    // maximum size of block that must be accommodated
    MAX_REQUEST_SIZE = ( 1 << 30 )
};

#define FOREACH_ERR( ERR ) /*NOLINT*/                                                                              \
    ERR( OK )                                                                                                      \
    ERR( MISMATCH )                                                                                                \
    ERR( HEAP_CONTINUES )                                                                                          \
    ERR( OUT_OF_BOUNDS )
#define GENERATE_ENUM( ENUM ) ENUM,        /*NOLINT*/
#define GENERATE_STRING( STRING ) #STRING, /*NOLINT*/

enum status_error
{
    FOREACH_ERR( GENERATE_ENUM )
};

static const char *const err_string[] = { // NOLINT
    FOREACH_ERR( GENERATE_STRING ) };

struct heap_block
{
    bool allocated;
    size_t payload_bytes;
    enum status_error err;
};

struct mismatch
{
    size_t fail_index;
    bool actual_status;
    size_t actual_payload_bytes;
};

struct state_check
{
    bool passed;
    struct mismatch log;
};

/// @brief myinit
/// ----------------
/// This must be called by a client before making any allocation
/// requests.  The function returns true if initialization was successful,
/// or false otherwise. The myinit function can be called to reset
/// the heap to an empty state. When running against a set of
/// of test scripts, our test harness calls myinit before starting
/// each new script.
bool myinit( void *heap_start, size_t heap_size );

/// @brief mymalloc
/// ------------------
/// Custom version of malloc.
void *mymalloc( size_t requested_size );

/// @brief myrealloc
/// -------------------
/// Custom version of realloc.
void *myrealloc( void *old_ptr, size_t new_size );

/// @brief myfree
/// ----------------
/// Custom version of free.
void myfree( void *ptr );

/// @brief validate_heap
/// -----------------------
/// This is the hook for your heap consistency checker. Returns true
/// if all is well, or false on any problem.  This function is
/// called periodically by the test harness to check the state of
/// the heap allocator.
bool validate_heap( void );

/// @brief get_free_total
/// ------------------------
/// Simple function to get the size of whatever data structure the allocator is
/// using to manage free nodes in the heap. Garunteed to be an O(1) operation.
size_t get_free_total( void );

/// @brief print_free_nodes
/// --------------------------
/// Prints a visual representation of the free nodes in the heap in the form of
/// the data structure being used to manage them. You can print the nodes in the
/// PLAIN or VERBOSE style. Plain will only show the sizes in bytes that the
/// blocks store, while VERBOSE will show their addresses in the heap and for the
/// tree allocators, the black height of the tree as well.
void print_free_nodes( enum print_style style );

size_t myheap_align( size_t request );

size_t myheap_capacity( void );

void myheap_state( const struct heap_block expected[], struct heap_block actual[], size_t len );

#endif

#ifdef __cplusplus
}
#endif
