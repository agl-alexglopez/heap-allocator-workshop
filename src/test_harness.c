/// Files: test_harness.c
/// ---------------------
/// Reads and interprets text-based script files containing a sequence of
/// allocator requests. Runs the allocator on a script and validates
/// results for correctness.
/// When you compile using `make`, it will create different
/// compiled versions of this program, one using each type of
/// heap allocator.
/// Written by jzelenski, updated by Nick Troccoli Winter 18-19. Modified by Alexander G. Lopez to
/// only include the heap testing logic. Previously, script parsing was included here as well. I
/// decomposed the script parsing to the script.h library and left only the logic for testing the
/// scripts behind. I also changed quite a bit and added more information for overlapping heap
/// boundary errors and encapsulated alloc realloc logic to their own functions.
#include "allocator.h"
#include "script.h"
#include "segment.h"
// NOLINTNEXTLINE(*include-cleaner)
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// TYPE DECLARATIONS

// Amount by which we resize ops when needed when reading in from file
const long heap_size = 1L << 32;
const size_t scale_to_whole_num = 100;
const size_t lowest_byte = 0xFFUL;

/// FUNCTION PROTOTYPES

static int test_scripts( char *script_names[], int num_script_names, bool quiet );
static ssize_t eval_correctness( struct script *s, bool quiet, bool *success );
static bool eval_malloc( int req, size_t requested_size, struct script *s, void **heap_end );
static bool eval_realloc( int req, size_t requested_size, struct script *s, void **heap_end );
static bool verify_block( void *ptr, size_t size, struct script *s, int lineno );
static bool verify_payload( void *ptr, size_t size, size_t id, struct script *s, int lineno, char *op );

/// CORRECTNESS EVALUATION IMPLEMENTATION

// NOLINTBEGIN(*include-cleaner)

/// @brief main
/// --------------
/// The main function parses command-line arguments (currently only -q for quiet)
/// and any script files that follow and runs the heap allocator on the specified
/// script files.  It outputs statistics about the run of each script, such as
/// the number of successful runs, number of failures, and average utilization.
int main( int argc, char *argv[] )
{
    // Parse command line arguments
    int c = 0;
    bool quiet = false;
    while ( ( c = getopt( argc, argv, "q" ) ) != EOF ) {
        if ( c == 'q' ) {
            quiet = true;
        }
    }
    if ( optind >= argc ) {
        printf( "Missing argument. Please supply one or more script files." );
        abort();
    }

    // disable stdout buffering, all printfs display to terminal immediately
    (void)setvbuf( stdout, NULL, _IONBF, 0 );

    return test_scripts( argv + optind, argc - optind, quiet );
}

// NOLINTEND(*include-cleaner)

static int test_scripts( char *script_names[], int num_script_names, bool quiet )
{
    int nsuccesses = 0;
    int nfailures = 0;
    // Utilization summed across all successful script runs (each is % out of 100)
    size_t total_util = 0;
    for ( int i = 0; i < num_script_names; i++ ) {
        struct script s = parse_script( script_names[i] );
        // Evaluate this script and record the results
        printf( "\nEvaluating allocator on %s...", s.name );
        bool success = false;
        ssize_t used_segment = eval_correctness( &s, quiet, &success );
        if ( success ) {
            printf(
                "successfully serviced %d requests. (payload/segment = %zu/%zu)",
                s.num_ops,
                s.peak_size,
                used_segment
            );
            if ( used_segment > 0 ) {
                total_util += ( scale_to_whole_num * s.peak_size ) / used_segment;
            }
            nsuccesses++;
        } else {
            nfailures++;
        }
        free( s.ops );
        free( s.blocks );
    }
    if ( nsuccesses ) {
        printf( "\nUtilization averaged %zu%%\n", total_util / nsuccesses );
    }
    return nfailures;
}

static ssize_t eval_correctness( struct script *s, bool quiet, bool *success )
{
    *success = false;
    init_heap_segment( heap_size );
    if ( !myinit( heap_segment_start(), heap_segment_size() ) ) {
        allocator_error( s, 0, "myinit() returned false" );
        return -1;
    }
    if ( !quiet && !validate_heap() ) {
        allocator_error( s, 0, "validate_heap() after myinit returned false" );
        return -1;
    }
    // Track the topmost address used by the heap for utilization purposes
    void *heap_end = heap_segment_start();
    // Track the current amount of memory allocated on the heap
    size_t cur_size = 0;
    // Send each request to the heap allocator and check the resulting behavior
    for ( int req = 0; req < s->num_ops; req++ ) {
        size_t id = s->ops[req].id;
        size_t requested_size = s->ops[req].size;

        if ( s->ops[req].op == ALLOC ) {
            cur_size += requested_size;
            if ( !eval_malloc( req, requested_size, s, &heap_end ) ) {
                return -1;
            }
        } else if ( s->ops[req].op == REALLOC ) {
            cur_size += ( requested_size - s->blocks[id].size );
            if ( !eval_realloc( req, requested_size, s, &heap_end ) ) {
                return -1;
            }
        } else if ( s->ops[req].op == FREE ) {
            size_t old_size = s->blocks[id].size;
            void *p = s->blocks[id].ptr;
            // verify payload intact before free
            if ( !verify_payload( p, old_size, id, s, s->ops[req].lineno, "freeing" ) ) {
                return -1;
            }
            s->blocks[id] = ( struct block ){ .ptr = NULL, .size = 0 };
            myfree( p );
            cur_size -= old_size;
        }
        // check heap consistency after each request and stop if any error
        if ( !quiet && !validate_heap() ) {
            allocator_error( s, s->ops[req].lineno, "validate_heap() returned false, called in-between requests" );
            return -1;
        }
        if ( cur_size > s->peak_size ) {
            s->peak_size = cur_size;
        }
    }
    // verify payload is still intact for any block still allocated
    for ( size_t id = 0; id < s->num_ids; id++ ) {
        if ( !verify_payload( s->blocks[id].ptr, s->blocks[id].size, id, s, -1, "at exit" ) ) {
            return -1;
        }
    }

    *success = true;
    return (uint8_t *)heap_end - (uint8_t *)heap_segment_start();
}

static bool eval_malloc( int req, size_t requested_size, struct script *s, void **heap_end )
{
    size_t id = s->ops[req].id;
    void *p = mymalloc( requested_size );
    if ( NULL == p && requested_size != 0 ) {
        allocator_error( s, s->ops[req].lineno, "heap exhausted, malloc returned NULL" );
        return false;
    }
    // Test new block for correctness: must be properly aligned and must not overlap any currently allocated block.
    if ( !verify_block( p, requested_size, s, s->ops[req].lineno ) ) {
        return false;
    }
    if ( (uint8_t *)p + requested_size > (uint8_t *)( *heap_end ) ) {
        ( *heap_end ) = (uint8_t *)p + requested_size;
    }
    // Fill new block with the low-order byte of new id can be used later to verify data copied when realloc'ing.
    memset( p, (int)( id & lowest_byte ), requested_size ); // NOLINT
    s->blocks[id] = ( struct block ){ .ptr = p, .size = requested_size };
    return true;
}

static bool eval_realloc( int req, size_t requested_size, struct script *s, void **heap_end )
{
    size_t id = s->ops[req].id;
    size_t old_size = s->blocks[id].size;
    void *oldp = s->blocks[id].ptr;
    if ( !verify_payload( oldp, old_size, id, s, s->ops[req].lineno, "pre-realloc-ing" ) ) {
        return false;
    }
    void *newp = myrealloc( oldp, requested_size );
    if ( NULL == newp && requested_size != 0 ) {
        allocator_error( s, s->ops[req].lineno, "heap exhausted, realloc returned NULL" );
        return false;
    }
    s->blocks[id].size = 0;
    if ( !verify_block( newp, requested_size, s, s->ops[req].lineno ) ) {
        return false;
    }
    // Verify new block contains the data from the old block
    if ( !verify_payload(
             newp,
             ( old_size < requested_size ? old_size : requested_size ),
             id,
             s,
             s->ops[req].lineno,
             "post-realloc-ing (preserving data)"
         ) ) {
        return false;
    }
    if ( (uint8_t *)newp + requested_size > (uint8_t *)( *heap_end ) ) {
        ( *heap_end ) = (uint8_t *)newp + requested_size;
    }
    // Fill new block with the low-order byte of new id
    memset( newp, (int)( id & lowest_byte ), requested_size ); // NOLINT
    s->blocks[id] = ( struct block ){ .ptr = newp, .size = requested_size };
    return true;
}

static bool verify_block( void *ptr, size_t size, struct script *s, int lineno )
{
    // address must be ALIGNMENT-byte aligned
    if ( ( (uintptr_t)ptr ) % ALIGNMENT != 0 ) {
        allocator_error( s, lineno, "New block (%p) not aligned to %d bytes", ptr, ALIGNMENT );
        return false;
    }
    if ( ptr == NULL && size == 0 ) {
        return true;
    }
    // block must lie within the extent of the heap
    void *end = (uint8_t *)ptr + size;
    void *heap_end = (uint8_t *)heap_segment_start() + heap_segment_size();
    if ( ptr < heap_segment_start() ) {
        allocator_error(
            s,
            lineno,
            "New block (%p:%p) not within heap segment (%p:%p)\n"
            "|----block-------|\n"
            "        |------heap-------...|\n",
            ptr,
            end,
            heap_segment_start(),
            heap_end
        );
        return false;
    }
    if ( end > heap_end ) {
        allocator_error(
            s,
            lineno,
            "New block (%p:%p) not within heap segment (%p:%p)\n"
            "               |----block-------|\n"
            "|...----heap-------|\n",
            ptr,
            end,
            heap_segment_start(),
            heap_end
        );
        return false;
    }
    // block must not overlap any other blocks
    for ( size_t i = 0; i < s->num_ids; i++ ) {
        if ( s->blocks[i].ptr == NULL || s->blocks[i].size == 0 ) {
            continue;
        }
        void *other_start = s->blocks[i].ptr;
        void *other_end = (uint8_t *)other_start + s->blocks[i].size;
        if ( ptr >= other_start && ptr < other_end ) {
            allocator_error(
                s,
                lineno,
                "New block (%p:%p) overlaps existing block (%p:%p)\n"
                "     |------current---------|\n"
                "|------other-------|\n",
                "or\n",
                "  |--current----|\n"
                "|------other-------|\n",
                ptr,
                end,
                other_start,
                other_end
            );
            return false;
        }
        if ( end > other_start && end < other_end ) {
            allocator_error(
                s,
                lineno,
                "New block (%p:%p) overlaps existing block (%p:%p)\n"
                "|---current---|\n"
                "         |------other-------|\n",
                "or\n",
                "  |--current----|\n"
                "|------other-------|\n",
                ptr,
                end,
                other_start,
                other_end
            );
            return false;
        }
        if ( ptr < other_start && end >= other_end ) {
            allocator_error(
                s,
                lineno,
                "New block (%p:%p) overlaps existing block (%p:%p)\n"
                "|---------current------------|\n"
                "    |------other-------|\n",
                ptr,
                end,
                other_start,
                other_end
            );
            return false;
        }
    }
    return true;
}

// NOLINTNEXTLINE (*-swappable-parameters)
static bool verify_payload( void *ptr, size_t size, size_t id, struct script *s, int lineno, char *op )
{
    for ( size_t i = 0; i < size; i++ ) {
        if ( *( (uint8_t *)ptr + i ) != ( id & lowest_byte ) ) {
            allocator_error( s, lineno, "invalid payload data detected when %s address %p", op, ptr );
            return false;
        }
    }
    return true;
}
