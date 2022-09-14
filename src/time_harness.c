/*
 * Files: time_harness.c
 * ---------------------
 * Reads and interprets text-based script files containing a sequence of
 * allocator requests. Runs the allocator on a script and times the requested
 * sequence of requests from that script. Outputs the time taken to complete
 * requests for the requested line numbers and total utilization percentage.
 *
 * When you compile using `make`, it will create different
 * compiled versions of this program, one using each type of
 * heap allocator.
 *
 * Written by jzelenski, updated by Nick Troccoli Winter 18-19 as a test harness to validate
 * code but adapted by Alex Lopez for timing and performance measurements, with all checks and
 * safety measures deleted for speed. This also helps view the runtime efficiency in the correct
 * time complexity without O(N) measures clouding accurate timing between calls. Please see
 * test_harness.c for the original implementation by course staff.
 */

#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "allocator.h"
#include "segment.h"


/* TYPE DECLARATIONS */

typedef unsigned char byte_t;

// enum and struct for a single allocator request
enum request_type {
    ALLOC = 1,
    FREE,
    REALLOC
};
typedef struct {
    enum request_type op;   // type of request
    int id;                 // id for free() to use later
    size_t size;            // num bytes for alloc/realloc request
    int lineno;             // which line in file
} request_t;

// struct for facts about a single malloc'ed block
typedef struct {
    void *ptr;
    size_t size;
} block_t;

// Requests to the heap are zero indexed, but we can allow users to enter line no. then subtract 1.
typedef struct {
    int start_req;
    int end_req;
} interval_t;

// struct for info for one script file
typedef struct {
    char name[128];                // short name of script
    request_t *ops;                // array of requests read from script
    int num_ops;                   // number of requests
    int num_ids;                   // number of distinct block ids
    block_t *blocks;               // array of memory blocks malloc returns when executing
    size_t peak_size;              // total payload bytes at peak in-use
} script_t;

// Create targeted scripts with intervals you want to time, no point in too many requests.
const short MAX_TIMER_REQUESTS = 100;

// Amount by which we resize ops when needed when reading in from file
const int OPS_RESIZE_AMOUNT = 500;

const int MAX_SCRIPT_LINE_LEN = 1024;

const long HEAP_SIZE = 1L << 32;


/* FUNCTION PROTOTYPES */


static int time_script(char *script_name, interval_t lines_to_time[], int num_lines_to_time);
static bool read_line(char buffer[], size_t buffer_size, FILE *fp, int *pnread);
static script_t parse_script(const char *filename);
static request_t parse_script_line(char *buffer, int lineno, char *script_name);
static size_t time_allocator(script_t *script, bool *success,
                        interval_t lines_to_time[], int num_lines_to_time);
static void *eval_malloc(int req, size_t requested_size, script_t *script, bool *failptr);
static void *eval_realloc(int req, size_t requested_size, script_t *script, bool *failptr);
static void allocator_error(script_t *script, int lineno, char* format, ...);


/* TIME EVALUATION IMPLEMENTATION */


/* @brief main  parses command line arguments that request a range of lines to be timed for
 *              performance. Arguments may take the following form:
 *
 *              ./time_rbtree_clrs -s 10001 -e 15000 -s 15001 scripts/time-insertdelete-5k.script
 *
 * @arg -s      the flag to start the timer on a certain line number. May be followed by -e flag.
 *              If no -e flag follows, the program will time the remainder of lines to execute.
 * @arg -e      the flag to end the timer on a certain line number. Invalid if not preceeded by -s.
 * @warning     time intervals may not overlap and with no arguments the entire program execution
 *              will be timed.
 */
int main(int argc, char *argv[]) {
    interval_t lines_to_time[MAX_TIMER_REQUESTS];
    int num_lines_to_time = 0;
    // -s flag to start timer on line number, -e flag to end flag on line number.
    int opt = getopt(argc, argv, "s:");
    while (opt != -1) {
        interval_t interval = {0};
        char *ptr;

        // It's easier for the user to enter line numbers. We will convert to zero indexed request.
        interval.start_req = strtol(optarg, &ptr, 10) - 1;
        if (num_lines_to_time
                && lines_to_time[num_lines_to_time - 1].end_req >= interval.start_req) {
            printf("Timing intervals can't overlap. Revisit script line ranges.\n");
            printf("Example of Bad Input Flags: -s 1 -e 5 -s 2\n");
            abort();
        }

        // Hide the end argument behind the start case to prevent ill formed args.
        opt = getopt(argc, argv, "e:");
        if (opt != -1) {
            interval.end_req = strtol(optarg, &ptr, 10) - 1;
        }
        lines_to_time[num_lines_to_time++] = interval;
        opt = getopt(argc, argv, "s:");
    }
    // We will default to timing the entire script if the user does not enter arguments.
    if (num_lines_to_time == 0) {
        lines_to_time[num_lines_to_time++] = (interval_t){0};
    }

    if (optind >= argc) {
        printf("Missing argument. Please supply one or more script files.");
        abort();
    }

    // disable stdout buffering, all printfs display to terminal immediately
    setvbuf(stdout, NULL, _IONBF, 0);

    return time_script(argv[optind], lines_to_time, num_lines_to_time);
}

/* @brief validate_intervals  checks the array of line intervals the user wants timed for validity.
 *                            Valid intervals do not overlap and start within the file line range.
 * @param *script             the script_t passed in with information about the file we parsed.
 * @param lines_to_time[]     the array of lines to time for the user. Check all O(N).
 * @param num_lines_to_time   lenghth of the lines to time array.
 */
void validate_intervals(script_t *script, interval_t lines_to_time[], int num_lines_to_time) {
    // We can tidy up lazy user input by making sure the end of the time interval makes sense.
    for (int req = 0; req < num_lines_to_time; req++) {
        // If the start is too large, the user may have mistaken the file they wish to time.
        if (script->num_ops - 1 < lines_to_time[req].start_req) {
            printf("Interval start is outside of script range:\n");
            printf("Interval start: %d\n", lines_to_time[req].start_req);
            printf("Script range: %d-%d\n", 1, script->num_ops);
            abort();
        }
        // Users might be familiar with python-like slices that take too large end ranges as valid.
        if (script->num_ops - 1 < lines_to_time[req].end_req || !lines_to_time[req].end_req) {
            lines_to_time[req].end_req = script->num_ops - 1;
        }
    }
}

/* @brief time_script        completes a series of 1 or more time requests for a script file and
 *                           outputs the times for the lines and overall utilization.
 * @param *script_name       the script we are tasked with timing.
 * @param lines_to_time[]    the array of line ranges to time.
 * @param num_lines_to_time  the size of the array of lines to time.
 */
static int time_script(char *script_name, interval_t lines_to_time[], int num_lines_to_time) {
    int nsuccesses = 0;
    int nfailures = 0;

    // Utilization summed across all successful script runs (each is % out of 100)
    int total_util = 0;

    script_t script = parse_script(script_name);
    validate_intervals(&script, lines_to_time, num_lines_to_time);


    // Evaluate this script and record the results
    printf("\nEvaluating allocator on %s...\n", script.name);
    bool success;
    // We will bring back useful utilization info while we time.
    size_t used_segment = time_allocator(&script, &success, lines_to_time, num_lines_to_time);
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

/* @brief eval_request  a helper function to complete a single call to the heap allocator. It may
 *                      may call malloc(), realloc(), or free().
 * @param *script       the script_t with the information regarding the script file we execute.
 * @param req           the zero-based index of the request to the heap allocator.
 * @param *cur_size     the current size of the heap overall.
 * @param **heap_end    the pointer to the end of the heap, we will adjust if heap grows.
 * @return              0 if there are no errors, -1 if there is an error.
 */
int eval_request(script_t *script, int req, size_t *cur_size, void **heap_end) {

    int id = script->ops[req].id;
    size_t requested_size = script->ops[req].size;

    if (script->ops[req].op == ALLOC) {
        bool fail = false;
        void *p = eval_malloc(req, requested_size, script, &fail);
        if (fail) {
            return -1;
        }

        *cur_size += requested_size;
        if ((byte_t *)p + requested_size > (byte_t *)(*heap_end)) {
            *heap_end = (byte_t *)p + requested_size;
        }
    } else if (script->ops[req].op == REALLOC) {
        size_t old_size = script->blocks[id].size;
        bool fail = false;
        void *p = eval_realloc(req, requested_size, script, &fail);
        if (fail) {
            return -1;
        }

        *cur_size += (requested_size - old_size);
        if ((byte_t *)p + requested_size > (byte_t *)(*heap_end)) {
            *heap_end = (byte_t *)p + requested_size;
        }
    } else if (script->ops[req].op == FREE) {
        size_t old_size = script->blocks[id].size;
        void *p = script->blocks[id].ptr;
        script->blocks[id] = (block_t){.ptr = NULL, .size = 0};
        myfree(p);
        *cur_size -= old_size;
    }


    if (*cur_size > script->peak_size) {
        script->peak_size = *cur_size;
    }
    return 0;
}

/* @brief time_allocator     times all requested interval line numbers from the script file.
 * @param *script            the script_t with all info for the script file to execute.
 * @param *success           true if all calls to the allocator completed without error.
 * @param lines_to_time[]    the array of all lines to time.
 * @param num_lines_to_time  the size of the array of lines to time.
 * @return                   the size of the heap overall.
 */
static size_t time_allocator(script_t *script, bool *success,
                             interval_t lines_to_time[], int num_lines_to_time) {
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

    // Send each request to the heap allocator and check the resulting behavior
    int req = 0;
    while (req < script->num_ops) {
        if (num_lines_to_time && lines_to_time[0].start_req == req) {
            interval_t sect = lines_to_time[0];
            clock_t start = 0;
            clock_t end = 0;
            double cpu_time = 0;
            start = clock();
            // Increment the outer loops request variable--------v
            for (int s = sect.start_req; s < sect.end_req; s++, req++) {
                eval_request(script, s, &cur_size, &heap_end);
            }
            end = clock();
            cpu_time = ((double) (end - start)) / CLOCKS_PER_SEC;
            printf("Execution time for script lines %d-%d (seconds): %f\n",
                    sect.start_req + 1, sect.end_req + 1, cpu_time);

            lines_to_time++;
            num_lines_to_time--;
        } else {
            eval_request(script, req, &cur_size, &heap_end);
            req++;
        }
    }
    *success = true;
    return (byte_t *)heap_end - (byte_t *)heap_segment_start();
}

/* @breif eval_malloc     performs a test of a call to mymalloc of the given size.
 * @param req             the request zero indexed within the script.
 * @param requested_size  the block size requested from the client.
 * @param *script         the script_t with information we track from the script file requests.
 * @param *failptr        a pointer to indicate if the request to malloc failed.
 * @return                the generic memory provided by malloc for the client. NULL on failure.
 */
static void *eval_malloc(int req, size_t requested_size, script_t *script, bool *failptr) {

    int id = script->ops[req].id;

    void *p;
    if ((p = mymalloc(requested_size)) == NULL && requested_size != 0) {
        allocator_error(script, script->ops[req].lineno,
            "heap exhausted, malloc returned NULL");
        *failptr = true;
        return NULL;
    }

    script->blocks[id] = (block_t){.ptr = p, .size = requested_size};
    *failptr = false;
    return p;
}

/* @brief eval_realloc  performs a test of a call to myrealloc of the given size.
 * @param req             the request zero indexed within the script.
 * @param requested_size  the block size requested from the client.
 * @param *script         the script_t with information we track from the script file requests.
 * @param *failptr        a pointer to indicate if the request to malloc failed.
 * @return                the generic memory provided by realloc for the client. NULL on failure.
 */
static void *eval_realloc(int req, size_t requested_size, script_t *script, bool *failptr) {

    int id = script->ops[req].id;
    void *oldp = script->blocks[id].ptr;
    void *newp;
    if ((newp = myrealloc(oldp, requested_size)) == NULL && requested_size != 0) {
        allocator_error(script, script->ops[req].lineno,
            "heap exhausted, realloc returned NULL");
        *failptr = true;
        return NULL;
    }

    script->blocks[id].size = 0;
    script->blocks[id] = (block_t){.ptr = newp, .size = requested_size};
    *failptr = false;
    return newp;
}

/* @brief allocator_error  reports an error while running an allocator script.
 * @param *script          the script_t with information we track form the script file requests.
 * @param lineno           the line number where the error occured.
 * @param *format          the specified format string.
 */
static void allocator_error(script_t *script, int lineno, char* format, ...) {
    va_list args;
    fprintf(stdout, "\nALLOCATOR FAILURE [%s, line %d]: ",
        script->name, lineno);
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    fprintf(stdout,"\n");
}


/* SCRIPT PARSING IMPLEMENTATION */


/* @breif parse_script  parses the script file at the specified path, and returns an object with
 *                      info about it.  It expects one request per line, and adds each request's
 *                      information to the ops array within the script.  This function throws an
 *                      error if the file can't be opened, if a line is malformed, or if the file
 *                      is too long to store each request on the heap.
 * @param *path         the path to the .script file to parse.
 * @return              a pointer to the script_t with information regarding the .script requests.
 */
static script_t parse_script(const char *path) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        printf("Could not open script file \"%s\".", path);
    }

    // Initialize a script object to store the information about this script
    script_t script = { .ops = NULL, .blocks = NULL, .num_ops = 0, .peak_size = 0};
    const char *basename = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    strncpy(script.name, basename, sizeof(script.name) - 1);
    script.name[sizeof(script.name) - 1] = '\0';

    int lineno = 0;
    int nallocated = 0;
    int maxid = 0;
    char buffer[MAX_SCRIPT_LINE_LEN];

    for (int i = 0; read_line(buffer, sizeof(buffer), fp, &lineno); i++) {

        // Resize script->ops if we need more space for lines
        if (i == nallocated) {
            nallocated += OPS_RESIZE_AMOUNT;
            void *new_memory = realloc(script.ops,
                nallocated * sizeof(request_t));
            if (!new_memory) {
                free(script.ops);
                printf("Libc heap exhausted. Cannot continue.");
                abort();
            }
            script.ops = new_memory;
        }

        script.ops[i] = parse_script_line(buffer, lineno, script.name);

        if (script.ops[i].id > maxid) {
            maxid = script.ops[i].id;
        }

        script.num_ops = i + 1;
    }

    fclose(fp);
    script.num_ids = maxid + 1;

    script.blocks = calloc(script.num_ids, sizeof(block_t));
    if (!script.blocks) {
        printf("Libc heap exhausted. Cannot continue.");
        abort();
    }

    return script;
}

/* @brief read_line    reads one line from the specified file and stores at most buffer_size
 *                     characters from it in buffer, removing any trailing newline. It skips lines
 *                     that are all-whitespace or that contain comments (begin with # as first
 *                     non-whitespace character).  When reading a line, it increments the
 *                     counter pointed to by `pnread` once for each line read/skipped.
 * @param buffer[]     the buffer in which we store the line from the .script line.
 * @param buffer_size  the allowable size for the buffer.
 * @param *fp          the file for which we are parsing requests.
 * @param *pnread      the pointer we use to progress past a line whether it is read or skipped.
 * @return             true if did read a valid line eventually, or false otherwise.
 */
static bool read_line(char buffer[], size_t buffer_size, FILE *fp, int *pnread) {

    while (true) {
        if (fgets(buffer, buffer_size, fp) == NULL) {
            return false;
        }

        (*pnread)++;

        // remove any trailing newline
        if (buffer[strlen(buffer)-1] == '\n') {
            buffer[strlen(buffer)-1] ='\0';
        }

        /* Stop only if this line is not a comment line (comment lines start
         * with # as first non-whitespace character)
         */
        char ch;
        if (sscanf(buffer, " %c", &ch) == 1 && ch != '#') {
            return true;
        }
    }
}

/* @brief parse_script_line  parses the provided line from the script and returns info about it
 *                           as a request_t object filled in with the type of the request, the
 *                           size, the ID, and the line number.
 * @param *buffer            the individual line we are parsing for a heap request.
 * @param lineno             the line in the file we are parsing.
 * @param *script_name       the name of the current script we can output if an error occurs.
 * @return                   the request_t for the individual line parsed.
 * @warning                  if the line is malformed, this function throws an error.
 */
static request_t parse_script_line(char *buffer, int lineno, char *script_name) {

    request_t request = { .lineno = lineno, .op = 0, .size = 0};

    char request_char;
    int nscanned = sscanf(buffer, " %c %d %zu", &request_char,
        &request.id, &request.size);
    if (request_char == 'a' && nscanned == 3) {
        request.op = ALLOC;
    } else if (request_char == 'r' && nscanned == 3) {
        request.op = REALLOC;
    } else if (request_char == 'f' && nscanned == 2) {
        request.op = FREE;
    }

    if (!request.op || request.id < 0 || request.size > MAX_REQUEST_SIZE) {
        printf("Line %d of script file '%s' is malformed.",
            lineno, script_name);
        abort();
    }

    return request;
}

