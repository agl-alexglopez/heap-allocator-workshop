/*
 * Files: time_harness.c
 * ---------------------
 * Reads and interprets text-based script files containing a sequence of
 * allocator requests. Runs the allocator on a script and times the requested
 * sequence of requests from that script. Outputs the time taken to complete
 * requests for the requested line numbers.
 *
 * When you compile using `make`, it will create 3 different
 * compiled versions of this program, one using each type of
 * heap allocator.
 *
 * Written by jzelenski, updated by Nick Troccoli Winter 18-19 as a test harness to validate
 * code but adapted by Alex Lopez for timing and performance measurements. Please see
 * test_harness.c for the original implementation.
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

// Create targeted scripts with sections you want to time, no point in too many requests.
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
static size_t eval_time(script_t *script, bool *success,
                        interval_t lines_to_time[], int num_lines_to_time);
static void *eval_malloc(int req, size_t requested_size, script_t *script, bool *failptr);
static void *eval_realloc(int req, size_t requested_size, script_t *script, bool *failptr);
static void allocator_error(script_t *script, int lineno, char* format, ...);


/* TIME EVALUATION IMPLEMENTATION */


/* Function: main
 * --------------
 * The main function parses command-line arguments (currently only -q for quiet)
 * and any script files that follow and runs the heap allocator on the specified
 * script files.  It outputs statistics about the run of each script, such as
 * the number of successful runs, number of failures, and average utilization.
 */
int main(int argc, char *argv[]) {
    // Parse command line arguments

    interval_t lines_to_time[MAX_TIMER_REQUESTS];
    int num_lines_to_time = 0;
    // -s flag to start timer on line number, -e flag to end flag on line number.
    int opt = getopt(argc, argv, "s:");
    while (opt != -1) {
        interval_t section = { .start_req = 0, .end_req = 0 };
        char *ptr;

        // It's easier for the user to enter line numbers. We will convert to zero indexed request.
        section.start_req = strtol(optarg, &ptr, 10) - 1;
        if (num_lines_to_time
                && lines_to_time[num_lines_to_time - 1].end_req >= section.start_req) {
            printf("Timing intervals can't overlap (e.g. -e 5 -s 2). Revisit script line ranges.");
            abort();
        }

        // Hide the end argument behind the start case to prevent ill formed args.
        opt = getopt(argc, argv, "e:");
        if (opt != -1) {
            section.end_req = strtol(optarg, &ptr, 10) - 1;
        }
        lines_to_time[num_lines_to_time++] = section;
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

void validate_intervals(script_t *script, interval_t lines_to_time[], int num_lines_to_time) {
    // We can tidy up lazy user input by making sure the end of the time interval makes sense.
    for (int req = 0; req < num_lines_to_time; req++) {
        if (script->num_ops - 1 < lines_to_time[req].start_req) {
            printf("Interval start is outside of script range:\n");
            printf("Interval start: %d", lines_to_time[req].start_req);
            printf("Script range: %d-%d", 1, script->num_ops);
            abort();
        }
        if (script->num_ops - 1 < lines_to_time[req].end_req || !lines_to_time[req].end_req) {
            lines_to_time[req].end_req = script->num_ops;
        }
    }
}

/* Function: time_script
 * ----------------------
 * Runs the scripts with names in the specified array, with more or less output
 * depending on the value of `quiet`.  Returns the number of failures during all
 * the tests.
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
    size_t used_segment = eval_time(&script, &success, lines_to_time, num_lines_to_time);
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
        if ((char *)p + requested_size > (char *)(*heap_end)) {
            *heap_end = (char *)p + requested_size;
        }
    } else if (script->ops[req].op == REALLOC) {
        size_t old_size = script->blocks[id].size;
        bool fail = false;
        void *p = eval_realloc(req, requested_size, script, &fail);
        if (fail) {
            return -1;
        }

        *cur_size += (requested_size - old_size);
        if ((char *)p + requested_size > (char *)(*heap_end)) {
            *heap_end = (char *)p + requested_size;
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

/* Function: eval_correctness
 * --------------------------
 * Check the allocator for correctness on given script. Interprets the
 * script operation-by-operation and reports if it detects any "obvious"
 * errors (returning blocks outside the heap, unaligned,
 * overlapping blocks, etc.)
 */
static size_t eval_time(script_t *script, bool *success,
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
            // Do a few final boundary checks for the user requests.
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
            // This way we don't have to interrupt or think too much about outer loop.
            lines_to_time++;
            num_lines_to_time--;
        } else {
            eval_request(script, req, &cur_size, &heap_end);
            req++;
        }
    }
    *success = true;
    return (char *)heap_end - (char *)heap_segment_start();
}

/* Function: eval_malloc
 * ---------------------
 * Performs a test of a call to mymalloc of the given size.  The req number
 * specifies the operation's index within the script.  This function verifies
 * the entire malloc'ed block and fills in the payload with a low-order byte
 * of the request id.  If the request fails, the boolean pointed to by
 * failptr is set to true - otherwise, it is set to false.  If it is set to
 * true this function returns NULL; otherwise, it returns what was returned
 * by mymalloc.
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

/* Function: eval_realloc
 * ---------------------
 * Performs a test of a call to myrealloc of the given size.  The req number
 * specifies the operation's index within the script.  This function verifies
 * the entire realloc'ed block and fills in the payload with a low-order byte
 * of the request id.  If the request fails, the boolean pointed to by
 * failptr is set to true - otherwise, it is set to false.  If it is set to true
 * this function returns NULL; otherwise, it returns what was returned by
 * myrealloc.
 */
static void *eval_realloc(int req, size_t requested_size, script_t *script,
    bool *failptr) {

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

/* Function: allocator_error
 * ------------------------
 * Report an error while running an allocator script.  Prints out the script
 * name and line number where the error occured, and the specified format
 * string, including any additional arguments as part of that format string.
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


/* Fuction: parse_script
 * ---------------------
 * This function parses the script file at the specified path, and returns an
 * object with info about it.  It expects one request per line, and adds each
 * request's information to the ops array within the script.  This function
 * throws an error if the file can't be opened, if a line is malformed, or if
 * the file is too long to store each request on the heap.
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

/* Function: read_line
 * --------------------
 * This function reads one line from the specified file and stores at most
 * buffer_size characters from it in buffer, removing any trailing newline.
 * It skips lines that are all-whitespace or that contain comments (begin with
 * # as first non-whitespace character).  When reading a line, it increments the
 * counter pointed to by `pnread` once for each line read/skipped. This function
 * returns true if did read a valid line eventually, or false otherwise.
 */
static bool read_line(char buffer[], size_t buffer_size, FILE *fp,
    int *pnread) {

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

/* Function: parse_script_line
 * ---------------------------
 * This function parses the provided line from the script and returns info
 * about it as a request_t object filled in with the type of the request,
 * the size, the ID, and the line number.  If the line is malformed, this
 * function throws an error.
 */
static request_t parse_script_line(char *buffer, int lineno,
    char *script_name) {

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

