/* Author: Alex G. Lopez
 *
 * File script.c
 * -------------------
 * This file contains the utility functions for processing script files and executing them on
 * custom allocators. We also implement our timing functions and plotting functions for the
 * programs that use this interface. Examine the plot_ functions for more details about how the
 * graphs are formed.
 *
 * Script parsing was written by stanford professors jzelenski and ntroccoli. For the execution
 * and timing functions I took the test_harness.c implementation and stripped out all uneccessary
 * safety and correctness checks so the functions run faster and do not introduce O(n) work.
 */
#include <errno.h>
#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "allocator.h"
#include "script.h"


/* TYPE DECLARATIONS */


typedef unsigned char byte_t;
// Amount by which we resize ops when needed when reading in from file
const int OPS_RESIZE_AMOUNT = 500;
const int MAX_SCRIPT_LINE_LEN = 1024;


/* * * * * * * * * * * * *  Parse File and Create Script  * * * * * * * * * */


/* @brief read_script_line  reads one line from the specified file and stores at most buffer_size
 *                          characters from it in buffer, removing any trailing newline. It skips
 *                          lines that are whitespace or comments (begin with # as first
 *                          non-whitespace character).  When reading a line, it increments the
 *                          counter pointed to by `pnread` once for each line read/skipped.
 * @param buffer[]          the buffer in which we store the line from the .script line.
 * @param buffer_size       the allowable size for the buffer.
 * @param *fp               the file for which we are parsing requests.
 * @param *pnread           the pointer we use to progress past a line whether it is read or skipped.
 * @return                  true if did read a valid line eventually, or false otherwise.
 * @citation                 jzelenski and ntroccoli Stanford University.
 */
static bool read_script_line(char buffer[], size_t buffer_size, FILE *fp, int *pnread) {
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
 * @citation                 jzelenski and ntroccoli Stanford University.
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

/* @breif parse_script  parses the script file at the specified path, and returns an object with
 *                      info about it.  It expects one request per line, and adds each request's
 *                      information to the ops array within the script.  This function throws an
 *                      error if the file can't be opened, if a line is malformed, or if the file
 *                      is too long to store each request on the heap.
 * @param *path         the path to the .script file to parse.
 * @return              a pointer to the script_t with information regarding the .script requests.
 * @citation            ntroccoli and jzelenski, Stanford University.
 */
script_t parse_script(const char *path) {
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

    for (int i = 0; read_script_line(buffer, sizeof(buffer), fp, &lineno); i++) {

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


/* * * * * * * * * * * * *  Execute Commands in Script Struct  * * * * * * * * * */


/* @breif exec_malloc     executes a call to mymalloc of the given size.
 * @param req             the request zero indexed within the script.
 * @param requested_size  the block size requested from the client.
 * @param *script         the script_t with information we track from the script file requests.
 * @return                the generic memory provided by malloc for the client. NULL on failure.
 */
static void *exec_malloc(int req, size_t requested_size, script_t *script) {
    int id = script->ops[req].id;
    void *p;
    if ((p = mymalloc(requested_size)) == NULL && requested_size != 0) {
        allocator_error(script, script->ops[req].lineno,
            "heap exhausted, malloc returned NULL. Script too large or allocator error.\n");
        abort();
    }

    script->blocks[id] = (block_t){.ptr = p, .size = requested_size};
    return p;
}

/* @brief exec_realloc    executes a call to myrealloc of the given size.
 * @param req             the request zero indexed within the script.
 * @param requested_size  the block size requested from the client.
 * @param *script         the script_t with information we track from the script file requests.
 * @return                the generic memory provided by realloc for the client. NULL on failure.
 */
static void *exec_realloc(int req, size_t requested_size, script_t *script) {
    int id = script->ops[req].id;
    void *oldp = script->blocks[id].ptr;
    void *newp;
    if ((newp = myrealloc(oldp, requested_size)) == NULL && requested_size != 0) {
        allocator_error(script, script->ops[req].lineno,
            "heap exhausted, realloc returned NULL. Script too large or allocator error.\n");
        abort();
    }

    script->blocks[id].size = 0;
    script->blocks[id] = (block_t){.ptr = newp, .size = requested_size};
    return newp;
}

/* @brief exec_request  a helper function to execute a single call to the heap allocator. It may
 *                      may call malloc(), realloc(), or free().
 * @param *script       the script_t with the information regarding the script file we execute.
 * @param req           the zero-based index of the request to the heap allocator.
 * @param *cur_size     the current size of the heap overall.
 * @param **heap_end    the pointer to the end of the heap, we will adjust if heap grows.
 * @return              0 if there are no errors, -1 if there is an error.
 */
int exec_request(script_t *script, int req, size_t *cur_size, void **heap_end) {

    int id = script->ops[req].id;
    size_t requested_size = script->ops[req].size;

    if (script->ops[req].op == ALLOC) {
        void *p = exec_malloc(req, requested_size, script);

        *cur_size += requested_size;
        if ((byte_t *)p + requested_size > (byte_t *)(*heap_end)) {
            *heap_end = (byte_t *)p + requested_size;
        }
    } else if (script->ops[req].op == REALLOC) {
        size_t old_size = script->blocks[id].size;
        void *p = exec_realloc(req, requested_size, script);

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


/* * * * * * * * * * * * *  Time Commands in Script Struct  * * * * * * * * * */


/* @brief time_malloc     a function that times the speed of one request to malloc on my heap.
 * @param req             the current request we are operating on in the script.
 * @param requested_size  the size in bytes from the script line.
 * @param *script         the script object we are working through for our requests.
 * @param **p             the generic pointer we will use to determine a successfull malloc.
 */
static double time_malloc(int req, size_t requested_size, script_t *script, void **p) {
    int id = script->ops[req].id;

    // When measurement times are very low gnuplot points have trouble marking terminal graphs.
    clock_t request_start = 0;
    clock_t request_end = 0;
    request_start = clock();
    *p = mymalloc(requested_size);
    request_end = clock();

    if (*p == NULL && requested_size != 0) {
        allocator_error(script, script->ops[req].lineno,
            "heap exhausted, malloc returned NULL. Script too large or allocator error.\n");
        abort();
    }

    script->blocks[id] = (block_t){.ptr = *p, .size = requested_size};
    return (((double) (request_end - request_start)) / CLOCKS_PER_SEC) * 1000;
}

/* @brief time_realloc    a function that times the speed of one request to realloc on my heap.
 * @param req             the current request we are operating on in the script.
 * @param requested_size  the size in bytes from the script line.
 * @param *script         the script object we are working through for our requests.
 * @param **newp          the generic pointer we will use to determine a successfull realloc
 */
static double time_realloc(int req, size_t requested_size, script_t *script, void **newp) {
    int id = script->ops[req].id;
    void *oldp = script->blocks[id].ptr;

    // When measurement times are very low gnuplot points have trouble marking terminal graphs.
    clock_t request_start = 0;
    clock_t request_end = 0;
    request_start = clock();
    *newp = myrealloc(oldp, requested_size);
    request_end = clock();

    if (*newp == NULL && requested_size != 0) {
        allocator_error(script, script->ops[req].lineno,
            "heap exhausted, realloc returned NULL. Script too large or allocator error.\n");
        abort();
    }

    script->blocks[id].size = 0;
    script->blocks[id] = (block_t){.ptr = *newp, .size = requested_size};
    return (((double) (request_end - request_start)) / CLOCKS_PER_SEC) * 1000;
}

/* @brief time_request  a wrapper function for timer functions that allows us to time a request to
 *                      the heap. Returns the cpu time of the request in milliseconds.
 * @param *script       the script object that holds data about our script we need to execute.
 * @param req           the current request to the heap we execute.
 * @param *cur_size     the current size of the heap.
 * @param **heap_end    the address of the end of our range of heap memory.
 * @return              the double representing the time to complete one request.
 */
double time_request(script_t *script, int req, size_t *cur_size, void **heap_end) {
    int id = script->ops[req].id;
    size_t requested_size = script->ops[req].size;

    double cpu_time = 0;
    if (script->ops[req].op == ALLOC) {
        void *p = NULL;
        cpu_time = time_malloc(req, requested_size, script, &p);

        *cur_size += requested_size;
        if ((byte_t *)p + requested_size > (byte_t *)(*heap_end)) {
            *heap_end = (byte_t *)p + requested_size;
        }
    } else if (script->ops[req].op == REALLOC) {
        size_t old_size = script->blocks[id].size;
        void *p = NULL;
        cpu_time = time_realloc(req, requested_size, script, &p);

        *cur_size += (requested_size - old_size);
        if ((byte_t *)p + requested_size > (byte_t *)(*heap_end)) {
            *heap_end = (byte_t *)p + requested_size;
        }
    } else if (script->ops[req].op == FREE) {
        size_t old_size = script->blocks[id].size;
        void *p = script->blocks[id].ptr;
        script->blocks[id] = (block_t){.ptr = NULL, .size = 0};

        // When measurement times are very low gnuplot points have trouble marking terminal graphs.
        clock_t request_start = 0;
        clock_t request_end = 0;
        request_start = clock();
        myfree(p);
        request_end = clock();
        cpu_time = (((double) (request_end - request_start)) / CLOCKS_PER_SEC) * 1000;

        *cur_size -= old_size;
    }

    if (*cur_size > script->peak_size) {
        script->peak_size = *cur_size;
    }
    return cpu_time;
}

/* @brief allocator_error  reports an error while running an allocator script.
 * @param *script          the script_t with information we track form the script file requests.
 * @param lineno           the line number where the error occured.
 * @param *format          the specified format string.
 */
void allocator_error(script_t *script, int lineno, char* format, ...) {
    va_list args;
    fprintf(stdout, "\nALLOCATOR FAILURE [%s, line %d]: ",
        script->name, lineno);
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    fprintf(stdout,"\n");
}


/* * * * * * * * * * * * * *  Plot Desired Information about Allocator  * * * * * * * * */


/* @brief print_gnuplots  a wrapper for the three gnuplot functions with helpful information in
 *                        case someone is waiting for large data. It can take time.
 * @brief *graphs         the gnuplots struct containing all the graphs to print.
 */
void print_gnuplots(gnuplots *graphs) {
    // We rely alot on Unix like system help. Redirect which output and disgard. Not portable.
    if (system("which gnuplot > /dev/null 2>&1")) {
        printf("Gnuplot not installed. For graph output, install gnuplot...\n");
    } else {
        printf("Gnuplot printing "COLOR_CYN"3"COLOR_NIL" graphs. This may take a moment for large data sets...\n");
        FILE *util_pipe = popen("gnuplot -persist", "w");
        FILE *free_pipe = popen("gnuplot -persist", "w");
        FILE *time_pipe = popen("gnuplot -persist", "w");
                            // This helps us see nice colors with our graphs.
        fprintf(util_pipe, "set terminal dumb ansi256;"
                            // This helps with compatibility on dumb terminals.
                           "set colorsequence classic;"
                           // Adds a nice backing grid of dots.
                           "set grid;"
                           // I don't want to manage dimensions and ticks, let gnuplot do it.
                           "set autoscale;"
                           // Sits above the graph.
                           "set title 'Utilization %% over Heap Lifetime';"
                           // Makes it clear x label number corresponds to script lines=lifetime.
                           "set xlabel 'Script Line Number';"
                           // '-'/notitle prevents title inside graph. Set the point to desired char.
                          "plot '-' pt '#' lc rgb 'green' notitle\n");

        // We have no significant changes in settings, except color, for the free nodes.
        fprintf(free_pipe, "set terminal dumb ansi256;set colorsequence classic;set grid;"
                           "set autoscale;set title 'Number of Free Nodes over Heap Lifetime';"
                           "set xlabel 'Script Line Number';plot '-' pt '#' lc rgb 'red' notitle\n");

        // Time graph is the same, but we set zero to be more sensitive to small values---v
        fprintf(time_pipe, "set terminal dumb ansi256;set colorsequence classic;set zero 1e-20;set grid;"
                           "set autoscale;set title 'Time (milliseconds) to Service a Heap Request';"
                           "set xlabel 'Script Line Number';plot '-' pt '#' lc rgb 'cyan' notitle\n");

        double total_time = 0;
        double total_util = 0;
        size_t total_free = 0;

        // Getting all of the pipes plotting in one O(n) loop helped speed. Still slow though.
        for (int req = 0; req < graphs->num_ops; req++) {
            total_time += graphs->request_times[req];
            total_util += graphs->util_percents[req];
            total_free += graphs->free_nodes[req];
            fprintf(util_pipe, "%d %lf \n", req + 1, graphs->util_percents[req]);
            fprintf(free_pipe, "%d %zu \n", req + 1, graphs->free_nodes[req]);
            fprintf(time_pipe, "%d %lf \n", req + 1, graphs->request_times[req]);
        }

        fprintf(util_pipe, "e\n");
        fprintf(free_pipe, "e\n");
        fprintf(time_pipe, "e\n");
        pclose(util_pipe);
        printf("Average utilization: %.2f%%\n", total_util / graphs->num_ops);
        pclose(free_pipe);
        printf("Average free nodes: %zu\n", total_free / graphs->num_ops);
        pclose(time_pipe);
        printf("Average time (milliseconds) per request overall: %lfms\n", total_time / graphs->num_ops);
    }
}
