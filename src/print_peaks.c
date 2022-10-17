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
#include <errno.h>
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

typedef struct {
    print_style style;
    breakpoint breakpoints[MAX_BREAKPOINTS];
    int num_breakpoints;
} user_breaks;

/* FUNCTION PROTOTYPES */


int print_peaks(char *script_name, user_breaks *user_reqs);
static size_t print_allocator(script_t *script, user_breaks *user_reqs, gnuplots *graphs);
static void handle_user_breakpoints(user_breaks *user_reqs, int curr_break, int script_end);
static int get_user_int(int min, int max);
static void validate_breakpoints(script_t *script, user_breaks *user_reqs);
static void binsert(const void *key, void *base, int *p_nelem, size_t width,
                    int (*compar)(const void *, const void *));
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
    user_breaks user_reqs = {.style = PLAIN, .breakpoints = {0}, .num_breakpoints = 0};
    char c;
    while ((c = getopt(argc, argv, "vb:")) != -1) {
        if (c == 'v') {
            user_reqs.style = VERBOSE;
        }
        if (c == 'b') {
            char *ptr;
            breakpoint line = strtol(optarg, &ptr, 10) - 1;
            binsert(&line, &user_reqs.breakpoints, &user_reqs.num_breakpoints,
                    sizeof(breakpoint), cmp_breakpoints);
        }
    }
    if (optind >= argc) {
        printf("Missing argument. Please supply one script file.");
        abort();
    }

    // disable stdout buffering, all printfs display to terminal immediately
    setvbuf(stdout, NULL, _IONBF, 0);

    return print_peaks(argv[optind], &user_reqs);
}

/* @brief print_peaks      prints the peak number of free nodes present in a heap allocator during
 *                         execution of a script. Prints the state of free nodes breakpoints
 *                         requested by the user as well.
 * @param script_name      the pointer to the script name we will execute.
 * @param *user_reqs       pointer to the struct containing user print style, and possible breaks.
 * @return                 0 upon successful execution 1 upon error.
 */
int print_peaks(char *script_name, user_breaks *user_reqs) {
    script_t script = parse_script(script_name);
    validate_breakpoints(&script, user_reqs);

    gnuplots graphs = {.util_percents = NULL, .free_nodes = NULL,
                       .request_times = NULL, .num_ops = script.num_ops};
    graphs.free_nodes = malloc(sizeof(size_t) * script.num_ops);
    assert(graphs.free_nodes);
    graphs.util_percents = malloc(sizeof(double) * script.num_ops);
    assert(graphs.util_percents);
    graphs.request_times = malloc(sizeof(double) * script.num_ops);
    assert(graphs.request_times);


    // Evaluate this script and record the results
    printf("\nEvaluating allocator on %s...\n", script.name);
    size_t used_segment = print_allocator(&script, user_reqs, &graphs);
    printf("...successfully serviced %d requests. (payload/segment = %zu/%zu)\n",
            script.num_ops, script.peak_size, used_segment);
    printf("Utilization averaged %.2lf%%\n", (100.0 * script.peak_size) / used_segment);

    print_gnuplots(&graphs);
    free(graphs.free_nodes);
    free(graphs.request_times);
    free(graphs.util_percents);
    free(script.ops);
    free(script.blocks);
    printf("^^^Scroll up to see the free nodes organized in their data structure at peak size.^^^\n");
    return 0;
}

/* @brief print_allocator  runs an allocator twice, gathering the peak number of free nodes and
 *                         printint any breakpoints the user requests from the script. The second
 *                         execution will print the peak size of the free nodes.
 * @param *script          the script_t with all info for the script file to execute.
 * @param *user_reqs       pointer to the struct containing user print style, and possible breaks.
 * @return                 the size of the heap overall as helpful utilization info.
 */
static size_t print_allocator(script_t *script, user_breaks *user_reqs, gnuplots *graphs) {
    init_heap_segment(HEAP_SIZE);
    if (!myinit(heap_segment_start(), heap_segment_size())) {
        allocator_error(script, 0, "myinit() returned false");
        return -1;
    }

    // Track the topmost address used by the heap for utilization purposes
    void *heap_end = heap_segment_start();
    // Track the current amount of memory allocated on the heap
    size_t cur_size = 0;

    // Send each request to the heap allocator and track largest free node structure.
    int peak_free_nodes_request = 0;
    size_t peak_free_node_count = 0;
    int curr_break = 0;
    for (int req = 0; req < script->num_ops; req++) {

        graphs->request_times[req] = time_request(script, req, &cur_size, &heap_end);

        // I added this as an allocator.h function so all allocators can report free nodes.
        size_t total_free_nodes = get_free_total();

        // We will plot this at the end of program execution.
        graphs->free_nodes[req] = total_free_nodes;
        // Avoid a loss of precision while tracking the utilization over heap lifetime.
        double peak_size = 100 * script->peak_size;
        graphs->util_percents[req] = peak_size /
                                    ((byte_t *) heap_end - (byte_t *) heap_segment_start());

        if (curr_break < user_reqs->num_breakpoints && user_reqs->breakpoints[curr_break] == req) {
            printf("There are %zu free nodes after executing command on line %d :\n",
                    total_free_nodes, req + 1);
            printf("\n");
            print_free_nodes(user_reqs->style);
            printf("\n");
            printf("There are %zu free nodes after executing command on line %d :\n",
                    get_free_total(), req + 1);

            // The user may enter more breakpoints.
            handle_user_breakpoints(user_reqs, curr_break, script->num_ops - 1);
            curr_break++;
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
            print_free_nodes(user_reqs->style);
            printf("\n");
            printf("Line %d of script created peak free blocks.\n", req + 1);
            printf("There were %zu free blocks of memory.\n", peak_free_node_count);
        }
    }
    return (byte_t *)heap_end - (byte_t *)heap_segment_start();
}

/* @brief handle_user_breakpoints  interacts with user regarding breakpoints they have requested.
 *                                 and will step with the user through requests and print nodes.
 *                                 The user may continue to next breakpoint, add another
 *                                 breakpoint, or skip remaining.
 * @param *user_reqs               pointer to the struct with user style, and possible breaks.
 * @param max                      the upper limit of user input and script range.
 */
static void handle_user_breakpoints(user_breaks *user_reqs, int curr_break, int max) {
    int min = user_reqs->breakpoints[curr_break] + 1;
    while (true) {
        int c;
        if (user_reqs->breakpoints[curr_break] == max) {
            fputs("Script complete.\n"
                  "Enter <ENTER> to exit: ", stdout);
            while ((c = getchar()) != '\n' && c != EOF) {
            }
            break;
        } else if (curr_break < user_reqs->num_breakpoints) {
            fputs("Enter the character <C> to continue to next breakpoint.\n"
                  "Enter the character <B> to add a new breakpoints.\n"
                  "Enter <ENTER> to exit: ", stdout);
        } else {
            fputs("No breakpoints remain.\n"
                  "Enter the character <B> to add a new breakpoints.\n"
                  "Enter <ENTER> to exit: ", stdout);
        }
        c = getchar();

        // User generated end of file or hit ENTER. We won't look at anymore breakpoints.
        if (c == EOF || c == '\n') {
            user_reqs->num_breakpoints = 0;
            break;
        }

        if (c < 'B' || c > 'C') {
            fprintf(stderr, "  ERROR: You entered: '%c'\n", c);
            // Clear out the input so they can try again.
            while ((c = getchar()) != '\n' && c != EOF) {
            }
        } else if (c == 'B') {
            while ((c = getchar()) != '\n' && c != EOF) {
            }

            int new_breakpoint = get_user_int(min, max);

            // We will insert the new breakpoint but also do nothing if it is already there.
            binsert(&new_breakpoint, user_reqs->breakpoints, &user_reqs->num_breakpoints,
                    sizeof(breakpoint), cmp_breakpoints);
        } else {
            // We have the right input but we will double check for extra chars to be safe.
            while ((c = getchar()) != '\n' && c != EOF) {
            }
            break;
        }

    }
}

/* @breif get_user_int  retreives an int from the user if they wish to add another breakpoint.
 * @param min           the lower range allowable for the int.
 * @param max           the upper range for the int.
 * @return              the valid integer entered by the user.
 */
static int get_user_int(int min, int max) {
    char *buff = malloc(sizeof(char) * 9);
    memset(buff, 0, 9);
    char *input = NULL;
    int input_int = 0;
    while(input == NULL) {
        fputs("Enter the new script line breakpoint: ", stdout);
        input = fgets(buff, 9, stdin);

        if (buff[strlen(input) - 1] != '\n') {
            fprintf(stderr, " ERROR: Breakpoint out of script range %d-%d.\n", min + 1, max + 1);
            for (char c = 0; (c = getchar()) != '\n' && c != EOF;) {
            }
            input = NULL;
            continue;
        }

        errno = 0;
        char *endptr = NULL;
        input_int = strtol(input, &endptr, 10) - 1;

        if (input == endptr) {
            input[strcspn(input, "\n")] = 0;
            printf(" ERROR: Invalid integer input: %s\n", input);
            input = NULL;
        } else if (errno != 0) {
            fprintf(stderr, " ERROR: Not and integer.\n");
            input = NULL;
        } else if (input_int < min || input_int > max) {
            fprintf(stderr, " ERROR: Breakpoint out of script range %d-%d.\n", min + 1, max + 1);
            input = NULL;
        }
    }
    free(buff);
    return input_int;

}

/* @brief validate_breakpoints  checks any requested breakpoints to make sure they are in range.
 * @param script                the parsed script with information we need to verify ranges.
 * @param *user_reqs            struct containing user print style, and possible breaks.
 */
static void validate_breakpoints(script_t *script, user_breaks *user_reqs) {
    // It's easier if the breakpoints are in order and can run along with our script execution.
    for (int brk = 0; brk < user_reqs->num_breakpoints; brk++) {
        // If the start is too large, the user may have mistaken the file they wish to time.
        if (script->num_ops - 1 < user_reqs->breakpoints[brk]
                || user_reqs->breakpoints[brk] <= 0) {
            printf("Breakpoint is outside of script range:\n");
            printf("Breakpoint line number: %d\n", user_reqs->breakpoints[brk]);
            printf("Script range: %d-%d\n", 1, script->num_ops);
            abort();
        }
    }
}

/* @brief *binsert  performs binary insertion sort on an array, finding an element if it already
 *                  present or inserting it in sorted place if it not yet present. It will update
 *                  the size of the array if it inserts an element.
 * @param *key      the element we are searching for in the array.
 * @param *p_nelem  the number of elements present in the array.
 * @param width     the size in bytes of each entry in the array.
 * @param *compar   the comparison function used to determine if an element matches our key.
 * @warning         this function will update the array size as an output parameter and assumes the
 *                  user has provided enough space for one additional element in the array.
 */
static void binsert(const void *key, void *base, int *p_nelem, size_t width,
                    int (*compar)(const void *, const void *)) {
    // We will use the address at the end of the array to help move bytes if we don't find elem.
    void *end = (byte_t *)base + (*p_nelem * width);
    for (size_t nremain = *p_nelem; nremain != 0; nremain >>= 1) {
        void *elem = (byte_t *)base + (nremain >> 1) * width;
        int sign = compar(key, elem);
        if (sign == 0) {
            return;
        }
        if (sign > 0) {  // key > elem
            // base settles where key belongs if we cannot find it. Steps right in this case.
            base = (byte_t *)elem + width;
            nremain--;
        }
        // key < elem and the base does not move in this case.
    }
    // First move does nothing, 0bytes, if we are adding first elem or new greatest value.
    memmove((byte_t *)base + width, base, (byte_t *)end - (byte_t *)base);
    // Now, wherever the base settled is where our key belongs.
    memcpy(base, key, width);
    ++*p_nelem;
}

/* @brief cmp_breakpoints  the compare function for our breakpoint line numbers for qsort.
 * @param a,b              breakpoints to compare.
 * @return                 >0 if a is larger than b, <0 if b is larger than a, =0 if same.
 */
static int cmp_breakpoints(const void *a, const void *b) {
    return (*(breakpoint *)a) - (*(breakpoint *) b);
}

