/// Author: Alexander G. Lopez
/// Files: time_harness.c
/// ---------------------
/// Reads and interprets text-based script files containing a sequence of
/// allocator requests. Runs the allocator on a script and times the requested
/// sequence of requests from that script. Times user requested sequences of lines in the
/// script if requested.
/// When you compile using `make`, it will create different
/// compiled versions of this program, one using each type of
/// heap allocator.
/// Most safety measures deleted for speed. Helps view the runtime efficiency in the correct
/// time complexity without O(N) measures clouding accurate timing between calls. Do not use this
/// unless you know your allocator is correct. Please see test_harness.c if you want to see an
/// allocator handler more focussed on correctness.

#include "allocator.h"
#include "print_utility.h"
#include "script.h"
#include "segment.h"
#include <assert.h>
// NOLINTNEXTLINE(*include-cleaner)
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/// TYPE DECLARATION

// Create targeted scripts with intervals you want to time, no point in too many requests.
enum
{
    MAX_TIMER_REQUESTS = 100
};

// Requests to the heap are zero indexed, but we can allow users to enter line no. then subtract 1.
struct interval
{
    int start_req;
    int end_req;
};

struct interval_reqs
{
    struct interval intervals[MAX_TIMER_REQUESTS];
    double interval_averages[MAX_TIMER_REQUESTS];
    size_t num_intervals;
};

const long heap_size = 1L << 32;
const int base_10 = 10;

/// FUNCTION PROTOTYPE

static int time_script( char *script_name, struct interval_reqs *user_requests );
static size_t time_allocator( struct script *s, struct interval_reqs *user_requests, struct gnuplots *graphs );
static void report_interval_averages( struct interval_reqs *user_requests );
static void validate_intervals( struct script *s, struct interval_reqs *user_requests );

/// TIME EVALUATION IMPLEMENTATIO

// NOLINTBEGIN(*include-cleaner)

/// @brief main  parses command line arguments that request a range of lines to be timed for
///              performance. Arguments may take the following form:
///              ../bin/time_rbtree_clrs -s 10001 -e 15000 -s 15001 scripts/time-insertdelete-5k.script
/// @arg -s      the flag to start the timer on a certain line number. May be followed by -e flag.
///              If no -e flag follows, the program will time the remainder of lines to execute.
/// @arg -e      the flag to end the timer on a certain line number. Invalid if not preceeded by -s.
/// @warning     time intervals may not overlap and with no arguments the entire program execution
///              will be timed.
int main( int argc, char *argv[] )
{
    struct interval_reqs user_req = { 0 };
    // -s flag to start timer on line number, -e flag to end flag on line number.
    int opt = getopt( argc, argv, "s:" );
    while ( opt != -1 ) {
        struct interval intv = { 0 };
        char *ptr = NULL;

        // It's easier for the user to enter line numbers. We will convert to zero indexed request.
        intv.start_req = (int)strtol( optarg, &ptr, base_10 ) - 1;
        if ( user_req.num_intervals && user_req.intervals[user_req.num_intervals - 1].end_req >= intv.start_req ) {
            printf( "Timing intervals can't overlap. Revisit script line ranges.\n" );
            printf( "Example of Bad Input Flags: -s 1 -e 5 -s 2\n" );
            abort();
        }

        // Hide the end argument behind the start case to prevent ill formed args.
        opt = getopt( argc, argv, "e:" );
        if ( opt != -1 ) {
            intv.end_req = (int)strtol( optarg, &ptr, base_10 ) - 1;
        }
        user_req.intervals[user_req.num_intervals++] = intv;
        opt = getopt( argc, argv, "s:" );
    }
    // We will default to timing the entire script if the user does not enter arguments.
    if ( user_req.num_intervals == 0 ) {
        user_req.intervals[user_req.num_intervals++] = ( struct interval ){ 0 };
    }

    if ( optind >= argc ) {
        printf( "Missing argument. Please supply one or more script files." );
        abort();
    }

    // disable stdout buffering, all printfs display to terminal immediately
    (void)setvbuf( stdout, NULL, _IONBF, 0 );

    return time_script( argv[optind], &user_req );
}

// NOLINTEND(*include-cleaner)

static int time_script( char *script_name, struct interval_reqs *user_requests )
{
    struct script s = parse_script( script_name );
    validate_intervals( &s, user_requests );

    // We will do some graphing with helpful info at the end of program execution.
    struct gnuplots graphs
        = { .util_percents = NULL, .free_nodes = NULL, .request_times = NULL, .num_ops = s.num_ops };
    graphs.free_nodes = malloc( sizeof( size_t ) * s.num_ops );
    assert( graphs.free_nodes );
    graphs.util_percents = malloc( sizeof( double ) * s.num_ops );
    assert( graphs.util_percents );
    graphs.request_times = malloc( sizeof( double ) * s.num_ops );
    assert( graphs.request_times );

    // Evaluate this script and record the results
    printf( "\nEvaluating allocator on %s...\n", s.name );
    // We will bring back useful utilization info while we time.
    size_t used_segment = time_allocator( &s, user_requests, &graphs );
    printf(
        "...successfully serviced %d requests. (payload/segment = %zu/%zu)\n", s.num_ops, s.peak_size, used_segment
    );
    printf( "Utilization averaged %.2lf%%\n", ( 100.0 * (double)s.peak_size ) / (double)used_segment );

    print_gnuplots( &graphs );
    report_interval_averages( user_requests );
    free( graphs.util_percents );
    free( graphs.free_nodes );
    free( graphs.request_times );
    free( s.ops );
    free( s.blocks );
    return 0;
}

static size_t time_allocator( struct script *s, struct interval_reqs *user_requests, struct gnuplots *graphs )
{
    init_heap_segment( heap_size );
    if ( !myinit( heap_segment_start(), heap_segment_size() ) ) {
        allocator_error( s, 0, "myinit() returned false" );
        return -1;
    }

    // Track the topmost address used by the heap for utilization purposes
    void *heap_end = heap_segment_start();
    // Track the current amount of memory allocated on the heap
    size_t cur_size = 0;
    int req = 0;
    size_t current_interval = 0;
    while ( req < s->num_ops ) {
        if ( current_interval < user_requests->num_intervals
             && user_requests->intervals[current_interval].start_req == req ) {

            struct interval sect = user_requests->intervals[current_interval];
            double total_request_time = 0;
            // Increment the outer loops request variable--------v
            for ( int sc = sect.start_req; sc < sect.end_req; sc++, req++ ) {
                // A helpful utility function fromt the script.h lib helps time one request.
                double request_time = time_request( s, req, &cur_size, &heap_end );
                total_request_time += request_time;

                graphs->request_times[req] = request_time;
                graphs->free_nodes[req] = get_free_total();
                graphs->util_percents[req] = ( 100.0 * (double)s->peak_size )
                                             / (double)( (uint8_t *)heap_end - (uint8_t *)heap_segment_start() );
            }
            printf(
                "Execution time for script lines %d-%d (milliseconds): %f\n",
                sect.start_req + 1,
                sect.end_req + 1,
                total_request_time
            );

            user_requests->interval_averages[current_interval]
                = total_request_time / (double)( sect.end_req - sect.start_req );
            current_interval++;
        } else {
            double request_time = time_request( s, req, &cur_size, &heap_end );

            graphs->request_times[req] = request_time;
            graphs->free_nodes[req] = get_free_total();
            graphs->util_percents[req] = ( 100.0 * (double)s->peak_size )
                                         / (double)( (uint8_t *)heap_end - (uint8_t *)heap_segment_start() );
            req++;
        }
    }
    return (uint8_t *)heap_end - (uint8_t *)heap_segment_start();
}

static void report_interval_averages( struct interval_reqs *user_requests )
{
    for ( size_t i = 0; i < user_requests->num_intervals; i++ ) {
        printf(
            "Average time (milliseconds) per request lines %d-%d: %lf\n",
            user_requests->intervals[i].start_req + 1,
            user_requests->intervals[i].end_req + 1,
            user_requests->interval_averages[i]
        );
    }
}

static void validate_intervals( struct script *s, struct interval_reqs *user_requests )
{
    // We can tidy up lazy user input by making sure the end of the time interval makes sense.
    for ( size_t req = 0; req < user_requests->num_intervals; req++ ) {
        // If the start is too large, the user may have mistaken the file they wish to time.
        if ( s->num_ops - 1 < user_requests->intervals[req].start_req ) {
            printf( "Interval start is outside of script range:\n" );
            printf( "Interval start: %d\n", user_requests->intervals[req].start_req );
            printf( "Script range: %d-%d\n", 1, s->num_ops );
            abort();
        }
        // Users might be familiar with python-like slices that take too large end ranges as valid.
        if ( s->num_ops - 1 < user_requests->intervals[req].end_req || !user_requests->intervals[req].end_req ) {
            user_requests->intervals[req].end_req = s->num_ops - 1;
        }
    }
}
