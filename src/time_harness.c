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
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/// TYPE DECLARATION

typedef unsigned char byte_t;
// Create targeted scripts with intervals you want to time, no point in too many requests.
#define MAX_TIMER_REQUESTS 100

// Requests to the heap are zero indexed, but we can allow users to enter line no. then subtract 1.
typedef struct
{
    int start_req;
    int end_req;
} interval_t;

typedef struct
{
    interval_t intervals[MAX_TIMER_REQUESTS];
    double interval_averages[MAX_TIMER_REQUESTS];
    size_t num_intervals;
} interval_reqs;

const long heap_size = 1L << 32;

/// FUNCTION PROTOTYPE

static int time_script( char *script_name, interval_reqs *user_requests );
static size_t time_allocator( script_t *script, interval_reqs *user_requests, gnuplots *graphs );
static void report_interval_averages( interval_reqs *user_requests );
static void validate_intervals( script_t *script, interval_reqs *user_requests );

/// TIME EVALUATION IMPLEMENTATIO

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
    interval_reqs user_req = { 0 };
    // -s flag to start timer on line number, -e flag to end flag on line number.
    int opt = getopt( argc, argv, "s:" );
    while ( opt != -1 ) {
        interval_t interval = { 0 };
        char *ptr = NULL;

        // It's easier for the user to enter line numbers. We will convert to zero indexed request.
        interval.start_req = (int)strtol( optarg, &ptr, 10 ) - 1;
        if ( user_req.num_intervals
             && user_req.intervals[user_req.num_intervals - 1].end_req >= interval.start_req ) {
            printf( "Timing intervals can't overlap. Revisit script line ranges.\n" );
            printf( "Example of Bad Input Flags: -s 1 -e 5 -s 2\n" );
            abort();
        }

        // Hide the end argument behind the start case to prevent ill formed args.
        opt = getopt( argc, argv, "e:" );
        if ( opt != -1 ) {
            interval.end_req = (int)strtol( optarg, &ptr, 10 ) - 1;
        }
        user_req.intervals[user_req.num_intervals++] = interval;
        opt = getopt( argc, argv, "s:" );
    }
    // We will default to timing the entire script if the user does not enter arguments.
    if ( user_req.num_intervals == 0 ) {
        user_req.intervals[user_req.num_intervals++] = ( interval_t ){ 0 };
    }

    if ( optind >= argc ) {
        printf( "Missing argument. Please supply one or more script files." );
        abort();
    }

    // disable stdout buffering, all printfs display to terminal immediately
    (void)setvbuf( stdout, NULL, _IONBF, 0 );

    return time_script( argv[optind], &user_req );
}

/// @brief time_script     completes a series of 1 or more time requests for a script file and
///                        outputs the times for the lines and overall utilization.
/// @param *script_name    the script we are tasked with timing.
/// @param *user_requests  the struct containing user requests for timings and how many.
static int time_script( char *script_name, interval_reqs *user_requests )
{
    script_t script = parse_script( script_name );
    validate_intervals( &script, user_requests );

    // We will do some graphing with helpful info at the end of program execution.
    gnuplots graphs
        = { .util_percents = NULL, .free_nodes = NULL, .request_times = NULL, .num_ops = script.num_ops };
    graphs.free_nodes = malloc( sizeof( size_t ) * script.num_ops );
    assert( graphs.free_nodes );
    graphs.util_percents = malloc( sizeof( double ) * script.num_ops );
    assert( graphs.util_percents );
    graphs.request_times = malloc( sizeof( double ) * script.num_ops );
    assert( graphs.request_times );

    // Evaluate this script and record the results
    printf( "\nEvaluating allocator on %s...\n", script.name );
    // We will bring back useful utilization info while we time.
    size_t used_segment = time_allocator( &script, user_requests, &graphs );
    printf( "...successfully serviced %d requests. (payload/segment = %zu/%zu)\n", script.num_ops, script.peak_size,
            used_segment );
    printf( "Utilization averaged %.2lf%%\n", ( 100.0 * (double)script.peak_size ) / (double)used_segment );

    print_gnuplots( &graphs );
    report_interval_averages( user_requests );
    free( graphs.util_percents );
    free( graphs.free_nodes );
    free( graphs.request_times );
    free( script.ops );
    free( script.blocks );
    return 0;
}

/// @brief time_allocator  times all requested interval line numbers from the script file.
/// @param *script         the script_t with all info for the script file to execute.
/// @param *user_requests  the struct containing user requested intervals and how many.
/// @param *graphs         the struct containing arrays we will fill with execution info to plot.
/// @return                the size of the heap overall.
static size_t time_allocator( script_t *script, interval_reqs *user_requests, gnuplots *graphs )
{
    init_heap_segment( heap_size );
    if ( !myinit( heap_segment_start(), heap_segment_size() ) ) {
        allocator_error( script, 0, "myinit() returned false" );
        return -1;
    }

    // Track the topmost address used by the heap for utilization purposes
    void *heap_end = heap_segment_start();
    // Track the current amount of memory allocated on the heap
    size_t cur_size = 0;
    int req = 0;
    size_t current_interval = 0;
    while ( req < script->num_ops ) {
        if ( current_interval < user_requests->num_intervals
             && user_requests->intervals[current_interval].start_req == req ) {

            interval_t sect = user_requests->intervals[current_interval];
            double total_request_time = 0;
            // Increment the outer loops request variable--------v
            for ( int s = sect.start_req; s < sect.end_req; s++, req++ ) {
                // A helpful utility function fromt the script.h lib helps time one request.
                double request_time = time_request( script, req, &cur_size, &heap_end );
                total_request_time += request_time;

                graphs->request_times[req] = request_time;
                graphs->free_nodes[req] = get_free_total();
                graphs->util_percents[req] = ( 100.0 * (double)script->peak_size )
                                             / (double)( (byte_t *)heap_end - (byte_t *)heap_segment_start() );
            }
            printf( "Execution time for script lines %d-%d (milliseconds): %f\n", sect.start_req + 1,
                    sect.end_req + 1, total_request_time );

            user_requests->interval_averages[current_interval]
                = total_request_time / (double)( sect.end_req - sect.start_req );
            current_interval++;
        } else {
            double request_time = time_request( script, req, &cur_size, &heap_end );

            graphs->request_times[req] = request_time;
            graphs->free_nodes[req] = get_free_total();
            graphs->util_percents[req] = ( 100.0 * (double)script->peak_size )
                                         / (double)( (byte_t *)heap_end - (byte_t *)heap_segment_start() );
            req++;
        }
    }
    return (byte_t *)heap_end - (byte_t *)heap_segment_start();
}

/// @brief report_interval_averages  prints the average time per request for a user requested
///                                  interval of line numbers.
/// @param *user_requests            a pointer to the struct containing user interval information.
static void report_interval_averages( interval_reqs *user_requests )
{
    for ( size_t i = 0; i < user_requests->num_intervals; i++ ) {
        printf( "Average time (milliseconds) per request lines %d-%d: %lf\n",
                user_requests->intervals[i].start_req + 1, user_requests->intervals[i].end_req + 1,
                user_requests->interval_averages[i] );
    }
}

/// @brief validate_intervals  checks the array of line intervals the user wants timed for validity.
///                            Valid intervals do not overlap and start within the file line range.
/// @param *script             the script_t passed in with information about the file we parsed.
/// @param intervals[]         the array of lines to time for the user. Check all O(N).
/// @param num_intervals       lenghth of the lines to time array.
static void validate_intervals( script_t *script, interval_reqs *user_requests )
{
    // We can tidy up lazy user input by making sure the end of the time interval makes sense.
    for ( size_t req = 0; req < user_requests->num_intervals; req++ ) {
        // If the start is too large, the user may have mistaken the file they wish to time.
        if ( script->num_ops - 1 < user_requests->intervals[req].start_req ) {
            printf( "Interval start is outside of script range:\n" );
            printf( "Interval start: %d\n", user_requests->intervals[req].start_req );
            printf( "Script range: %d-%d\n", 1, script->num_ops );
            abort();
        }
        // Users might be familiar with python-like slices that take too large end ranges as valid.
        if ( script->num_ops - 1 < user_requests->intervals[req].end_req
             || !user_requests->intervals[req].end_req ) {
            user_requests->intervals[req].end_req = script->num_ops - 1;
        }
    }
}
