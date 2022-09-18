/* Author: Alex G Lopez
 *
 * File: print_peaks.c
 * -------------------------
 * This program prints information regarding the peak size of the free node data structure used by
 * a custom heap allocator implementation. For example, if a heap allocator uses a list to manage
 * free nodes, it will print a visual representation of a list representing the greatest size the
 * free list acheived over the course of heap execution on a dedicated script file. This will also
 * print the tree structure of my red black tree heap allocator at its greatest size over the
 * course of its lifetime.
 *
 * This program will also act as a mini version of gdb allowing you to place breakpoints in the
 * execution of desired script file, printing the state of the free node data structure at that
 * time. This is done in order to show how I was working with my printing functions to help me
 * debug while in gdb.
 */
#include <assert.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "allocator.h"
#include "script.h"
#include "segment.h"


/* TYPE DECLARATIONS */


// Pick line numbers in the script to make breakpoints. Execution will stop and print there.
typedef int breakpoint;
typedef unsigned char byte_t;

#define MAX_BREAKPOINTS (unsigned short) 100
const long HEAP_SIZE = 1L << 32;

/* FUNCTION PROTOTYPES */


int print_peaks(char *script_name, breakpoint breakpoints[], int num_breakpoints,
                print_style style);
static size_t print_allocator(script_t *script, bool *success,
                              breakpoint breakpoints[], int num_breakpoints, print_style style);
static void plot_free_totals(size_t free_totals[], int num_free_totals);
static void plot_utilization(double *utilization_percents, int num_utilizations);
static int handle_user_input(int num_breakpoints);
static void validate_breakpoints(script_t *script, breakpoint breakpoints[], int num_breakpoints);
static int cmp_breakpoints(const void *a, const void *b);


/* @brief main  this program will illustrate the peak heap size of free nodes in an allocator. It
 *              can also create breakpoints in the script file and illustrate the state of free
 *              nodes after the requested line has executed. Finally you may print in VERBOSE mode
 *              with the -v flag. This will illustrate more detailed information regarding the
 *              heap including memory addresses and black heights of the tree if the allocator uses
 *              a red black tree.
 *
 *                  - print the peak number of free nodes managed by an allocator.
 *                  .././obj/print_peaks_rbtree_clrs ../scripts/pattern-mixed.script
 *
 *                  - print the peak number of free nodes managed by an allocator, verbose.
 *                  .././obj/print_peaks_rbtree_clrs -v ../scripts/pattern-mixed.script
 *
 *                  - include breakpoints on script line numbers and examine each.
 *                  .././obj/print_peaks_rbtree_clrs -v -b 10 -b 200../scripts/pattern-mixed.script
 *
 * @arg -v    verbose print. Includes memory addresses and black height of the tree.
 * @arg -b    break on line number of a script and print the state of the free nodes. Breakpoints
 *            can be entered in any order.
 */
int main(int argc, char *argv[]) {
    breakpoint breakpoints[MAX_BREAKPOINTS];
    int num_breakpoints = 0;

    print_style style = PLAIN;

    char c;
    while ((c = getopt(argc, argv, "vb:")) != -1) {
        if (c == 'v') {
            style = VERBOSE;
        }
        if (c == 'b') {
            char *ptr;
            breakpoint line = strtol(optarg, &ptr, 10) - 1;
            breakpoints[num_breakpoints++] = line;
        }
    }
    if (optind >= argc) {
        printf("Missing argument. Please supply one or more script files.");
        abort();
    }

    // disable stdout buffering, all printfs display to terminal immediately
    setvbuf(stdout, NULL, _IONBF, 0);

    return print_peaks(argv[optind], breakpoints, num_breakpoints, style);
}

/* @brief print_peaks      prints the peak number of free nodes present in a heap allocator during
 *                         execution of a script. Prints the state of free nodes breakpoints
 *                         requested by the user as well.
 * @param script_name      the pointer to the script name we will execute.
 * @param breakpoints      the array of breakpoint lines we will pause at to print the free nodes.
 * @param num_breakpoints  the number of breakpoint line numbers in the array.
 * @return                 0 upon successful execution -1 upon error.
 */
int print_peaks(char *script_name, breakpoint breakpoints[], int num_breakpoints,
                print_style style) {
    int nsuccesses = 0;
    int nfailures = 0;

    // Utilization summed across all successful script runs (each is % out of 100)
    int total_util = 0;

    script_t script = parse_script(script_name);
    validate_breakpoints(&script, breakpoints, num_breakpoints);


    // Evaluate this script and record the results
    printf("\nEvaluating allocator on %s...\n", script.name);
    bool success;
    // We will bring back useful utilization info while we time.
    size_t used_segment = print_allocator(&script, &success, breakpoints, num_breakpoints, style);
    if (success) {
        printf("...successfully serviced %d requests. (payload/segment = %zu/%zu)",
            script.num_ops, script.peak_size, used_segment);
        if (used_segment > 0) {
            total_util += (100 * script.peak_size) / used_segment;
        }
        nsuccesses++;
    } else {
        nfailures++;
    }

    free(script.ops);
    free(script.blocks);

    if (nsuccesses) {
        printf("\nUtilization averaged %d%%\n", total_util / nsuccesses);
    }
    return nfailures;
}

/* @brief print_allocator  runs an allocator twice, gathering the peak number of free nodes and
 *                         printint any breakpoints the user requests from the script. The second
 *                         execution will print the peak size of the free nodes.
 * @param *script          the script_t with all info for the script file to execute.
 * @param *success         true if all calls to the allocator completed without error.
 * @param breakpoints[]    the array of any line number breakpoints the user wants.
 * @param num_breakpoints  the size of the breakpoint array
 * @return                 the size of the heap overall as helpful utilization info.
 */
static size_t print_allocator(script_t *script, bool *success,
                              breakpoint breakpoints[], int num_breakpoints, print_style style) {
    *success = false;
    init_heap_segment(HEAP_SIZE);
    if (!myinit(heap_segment_start(), heap_segment_size())) {
        allocator_error(script, 0, "myinit() returned false");
        return -1;
    }

    // Track the topmost address used by the heap for utilization purposes
    void *heap_end = heap_segment_start();
    // Track the current amount of memory allocated on the heap
    size_t cur_size = 0;

    // We will track the total free nodes over the course of the heap, and plot the data.
    size_t *free_totals = malloc(sizeof(size_t) * script->num_ops);
    assert(free_totals);
    // We will also show a graph of utilization over the course of the heap.
    double *utilation_percents = malloc(sizeof(double) * script->num_ops);
    assert(utilation_percents);

    // Send each request to the heap allocator and track largest free node structure.
    int peak_free_nodes_request = 0;
    size_t peak_free_node_count = 0;
    for (int req = 0; req < script->num_ops; req++) {

        if (exec_request(script, req, &cur_size, &heap_end) == -1) {
            return -1;
        }

        // I added this as an allocator.h function so all allocators can report free nodes.
        size_t total_free_nodes = get_free_total();

        // We will plot this at the end of program execution.
        free_totals[req] = total_free_nodes;
        // Avoid a loss of precision while tracking the utilization over heap lifetime.
        double peak_size = 100 * script->peak_size;
        utilation_percents[req] = peak_size /
                                  ((byte_t *) heap_end - (byte_t *) heap_segment_start());

        if (num_breakpoints && breakpoints[0] == req) {
            printf("There are %zu free nodes after executing command on line %d :\n",
                    total_free_nodes, req + 1);
            print_free_nodes(style);
            printf("\n");
            printf("There are %zu free nodes after executing command on line %d :\n",
                    get_free_total(), req + 1);
            num_breakpoints = handle_user_input(num_breakpoints);
            breakpoints++;
        }

        if (total_free_nodes > peak_free_node_count) {
            peak_free_node_count = total_free_nodes;
            peak_free_nodes_request = req;
        }
    }

    /* We could track a persistent dynamic set of the free data structure but that would be slow
     * and I don't want to expose heap internals to this program or make copies of the nodes. Just
     * run it twice and use the allocator's provided printer function to find max nodes.
     */
    *success = false;
    init_heap_segment(HEAP_SIZE);
    if (!myinit(heap_segment_start(), heap_segment_size())) {
        allocator_error(script, 0, "myinit() returned false");
        return -1;
    }
    heap_end = heap_segment_start();
    cur_size = 0;


    for (int req = 0; req < script->num_ops; req++) {
        // execute the request
        if (exec_request(script, req, &cur_size, &heap_end) == -1) {
            return -1;
        }
        if (req == peak_free_nodes_request) {
            printf("Line %d of script created peak number free blocks.\n", req + 1);
            printf("There were %zu free blocks of memory.\n", peak_free_node_count);
            printf("\n");
            print_free_nodes(style);
            printf("\n");
            printf("Line %d of script created peak free blocks.\n", req + 1);
            printf("There were %zu free blocks of memory.\n", peak_free_node_count);
        }
    }
    *success = true;

    plot_free_totals(free_totals, script->num_ops);
    plot_utilization(utilation_percents, script->num_ops);
    free(free_totals);
    free(utilation_percents);

    return (byte_t *)heap_end - (byte_t *)heap_segment_start();
}

/* @brief plot_free_totals  plots the number of free nodes over the course of the heaps lifetime.
 *                          By default it will print an ascii graph to the terminal. Can be edited
 *                          or adapted to output to popup window. Requires gnuplot.
 * @param *free_totals     the number of total free nodes after each line of script executes.
 * @param num_free_totals   size of the array of totals equal to number of lines in script.
 */
static void plot_free_totals(size_t *free_totals, int num_free_totals) {

    FILE *gnuplotPipe = popen("gnuplot -persistent", "w");

                         // Comment this line out if you want output to window that looks better.
    fprintf(gnuplotPipe, "set terminal dumb ansirgb;"
                         // This helps with compatibility on dumb terminals.
                         "set colorsequence classic;"
                         // I don't want to manage window dimensions, let gnuplot do it.
                         "set autoscale;"
                         // Sits above the graph.
                         "set title 'Number of Free Nodes over Heap Lifetime';"
                         // Makes it clear x label number corresponds to script lines=lifetime.
                         "set xlabel 'Script Line Number';"
                         // '-' and notitle prevents title inside graph. 'with lines' makes the
                         // points astericks. 'linespoints <INTEGER>' can set different pointstyles
                         "plot '-' pt '#' lc rgb 'red' notitle\n");

    for (int req = 0; req < num_free_totals; req++) {
        fprintf(gnuplotPipe, "%d %zu \n", req + 1, free_totals[req]);
    }

    fprintf(gnuplotPipe, "e\n");
    pclose(gnuplotPipe);
}

/* @brief plot_utilization     plots the utilization of the heap over its lifetime as a percentage.
 * @param *utilation_percents  the mallocd array of percentages.
 * @param num_utilizations     the size of the array.
 */
static void plot_utilization(double *utilization_percents, int num_utilizations) {

    FILE *gnuplotPipe = popen("gnuplot -persistent", "w");

                         // Comment this line out if you want output to window that looks better.
    fprintf(gnuplotPipe, "set terminal dumb ansirgb;"
                         // This helps with compatibility on dumb terminals.
                         "set colorsequence classic;"
                         // I don't want to manage window dimensions, let gnuplot do it.
                         "set autoscale;"
                         // Sits above the graph.
                         "set title 'Utilization %% over Heap Lifetime';"
                         // Makes it clear x label number corresponds to script lines=lifetime.
                         "set xlabel 'Script Line Number';"
                         // '-' and notitle prevents title inside graph. 'with lines' makes the
                         // points astericks. 'linespoints <INTEGER>' can set different pointstyles
                         "plot '-' pt '#' lc rgb 'green' notitle\n");

    for (int req = 0; req < num_utilizations; req++) {
        fprintf(gnuplotPipe, "%d %lf \n", req + 1, utilization_percents[req]);
    }

    fprintf(gnuplotPipe, "e\n");
    pclose(gnuplotPipe);
}

/* @brief handle_user_input  interacts with the user regarding the breakpoints they have requested.
 *                           and will step with the user through their requests and print nodes.
 *                           The user may continue to next breakpoint or skip remaining.
 * @param num_breakpoints    the number of breakpoints we have remaining from the user.
 * @return                   the current number of breakpoints after advancing user request.
 */
static int handle_user_input(int num_breakpoints) {
    // We have just hit a requested breakpoint so we decrement, invariant.
    num_breakpoints--;
    while (true) {
        int c;
        if (num_breakpoints) {
            fputs("Enter the character <C> to continue to next breakpoint or <ENTER> to exit: ",
                    stdout);
        } else {
            fputs("No breakpoints remain. Press <ENTER> to continue: ", stdout);
        }
        c = getchar();

        // User generated end of file or hit ENTER. We won't look at anymore breakpoints.
        if (c == EOF || c == '\n') {
            num_breakpoints = 0;
            break;
        }

        if (c > 'C' || c < 'C') {
            fprintf(stderr, "  ERROR: You entered: '%c'\n", c);
            // Clear out the input so they can try again.
            while ((c = getchar()) != '\n' && c != EOF) {
            }
        } else {
            // We have the right input but we will double check for extra chars to be safe.
            while ((c = getchar()) != '\n' && c != EOF) {
            }
            break;
        }

    }
    return num_breakpoints;
}

/* @brief validate_breakpoints  checks any requested breakpoints to make sure they are in range.
 * @param script                the parsed script with information we need to verify ranges.
 * @param breakpoints[]         the array of breakpoint line numbers from the user.
 * @param num_breakpoints       the size of the breakpoints array.
 */
static void validate_breakpoints(script_t *script, breakpoint breakpoints[], int num_breakpoints) {
    // It's easier if the breakpoints are in order and can run along with our script execution.
    qsort(breakpoints, num_breakpoints, sizeof(breakpoint), cmp_breakpoints);
    for (int brk = 0; brk < num_breakpoints; brk++) {
        // If the start is too large, the user may have mistaken the file they wish to time.
        if (script->num_ops - 1 < breakpoints[brk] || breakpoints[brk] <= 0) {
            printf("Breakpoint is outside of script range:\n");
            printf("Breakpoint line number: %d\n", breakpoints[brk]);
            printf("Script range: %d-%d\n", 1, script->num_ops);
            abort();
        }
    }
}

/* @brief cmp_breakpoints  the compare function for our breakpoint line numbers for qsort.
 * @param a,b              breakpoints to compare.
 * @return                 >0 if a is larger than b, <0 if b is larger than a, =0 if same.
 */
static int cmp_breakpoints(const void *a, const void *b) {
    return (*(breakpoint *)a) - (*(breakpoint *) b);
}

