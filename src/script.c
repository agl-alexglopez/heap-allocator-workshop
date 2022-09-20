/* Author: Alex G. Lopez
 *
 * File script.c
 * -------------------
 * This file contains the utility functions for processing script files and executing them on
 * custom allocators.
 */
#include <stdarg.h>
#include <stdbool.h>
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


/* SCRIPT PARSING IMPLEMENTATION */


/* * * * * * * * * * * * *  Parse File and Create Script  * * * * * * * * * */


/* @brief read_script_line  reads one line from the specified file and stores at most buffer_size
 *                          characters from it in buffer, removing any trailing newline. It skips lines
 *                          that are all-whitespace or that contain comments (begin with # as first
 *                          non-whitespace character).  When reading a line, it increments the
 *                          counter pointed to by `pnread` once for each line read/skipped.
 * @param buffer[]          the buffer in which we store the line from the .script line.
 * @param buffer_size       the allowable size for the buffer.
 * @param *fp               the file for which we are parsing requests.
 * @param *pnread           the pointer we use to progress past a line whether it is read or skipped.
 * @return                  true if did read a valid line eventually, or false otherwise.
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
 * @param *failptr        a pointer to indicate if the request to malloc failed.
 * @return                the generic memory provided by malloc for the client. NULL on failure.
 */
static void *exec_malloc(int req, size_t requested_size, script_t *script, bool *failptr) {

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

/* @brief exec_realloc    executes a call to myrealloc of the given size.
 * @param req             the request zero indexed within the script.
 * @param requested_size  the block size requested from the client.
 * @param *script         the script_t with information we track from the script file requests.
 * @param *failptr        a pointer to indicate if the request to malloc failed.
 * @return                the generic memory provided by realloc for the client. NULL on failure.
 */
static void *exec_realloc(int req, size_t requested_size, script_t *script, bool *failptr) {

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
        bool fail = false;
        void *p = exec_malloc(req, requested_size, script, &fail);
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
        void *p = exec_realloc(req, requested_size, script, &fail);
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

/* @brief time_request  a wrapper function for exec_request that allows us to time one request to
 *                      the heap. Returns the time of the request in milliseconds.
 * @param *script       the script object that holds data about our script we need to execute.
 * @param req           the current request to the heap we execute.
 * @param *cur_size     the current size of the heap.
 * @param **heap_end    the address of the end of our range of heap memory.
 * @return              the double representing the time to complete one request.
 */
double time_request(script_t *script, int req, size_t *cur_size, void **heap_end) {
    clock_t request_start = 0;
    clock_t request_end = 0;
    request_start = clock();
    if (exec_request(script, req, cur_size, heap_end) == -1) {
        printf("Timer failed. Heap error, possible exhaustion.");
        exit(1);
    }
    request_end = clock();
    return (((double) (request_end - request_start)) / CLOCKS_PER_SEC) * 1000;
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


/* @brief plot_free_totals     plots number of free nodes over the course of the heaps lifetime.
 *                             By default prints an ascii graph to the terminal. Can be edited
 *                             or adapted to output to popup window. Requires gnuplot.
 * @param *totals_per_request  the number of total free nodes after each line of script executes.
 * @param num_requests         size of the array of totals equal to number of lines in script.
 */
void plot_free_totals(size_t *totals_per_request, int num_requests) {
    printf("Gnuplot is working. This may take a moment for large data sets...\n");
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
                         // '-'/notitle prevents title inside graph. Set the point to desired char.
                         "plot '-' with points pt '#' lc rgb 'red' notitle\n");
    double total_frees = 0;
    for (int req = 0; req < num_requests; req++) {
        total_frees += totals_per_request[req];
        fprintf(gnuplotPipe, "%d %zu \n", req + 1, totals_per_request[req]);
    }

    fprintf(gnuplotPipe, "e\n");
    pclose(gnuplotPipe);
    printf("Average free nodes: %.1lf\n", total_frees / num_requests);
}

/* @brief plot_utilization          plots the utilization of the heap over its lifetime as a percentage.
 * @param *utilization_per_request  the mallocd array of percentages.
 * @param num_requests              the size of the array.
 */
void plot_utilization(double *utilization_per_request, int num_requests) {
    printf("Gnuplot is working. This may take a moment for large data sets...\n");
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
                         // '-'/notitle prevents title inside graph. Set the point to desired char.
                         "plot '-' pt '#' lc rgb 'green' notitle\n");

    double total = 0;
    for (int req = 0; req < num_requests; req++) {
        total += utilization_per_request[req];
        fprintf(gnuplotPipe, "%d %lf \n", req + 1, utilization_per_request[req]);
    }

    fprintf(gnuplotPipe, "e\n");
    pclose(gnuplotPipe);
    printf("Average utilization: %.2f%%\n", total / num_requests);
}

/* @brief plot_request_speed  plots the time to service heap requests over heap lifetime.
 * @param *time_per_request   the mallocd array of time measurements.
 * @param num_requests        the number of requests in the script corresponding to measurements.
 */
void plot_request_speed(double *time_per_request, int num_requests) {
    printf("Gnuplot is working. This may take a moment for large data sets...\n");
    FILE *gnuplotPipe = popen("gnuplot -persistent", "w");

                         // Comment this line out if you want output to window that looks better.
    fprintf(gnuplotPipe, "set terminal dumb ansirgb;"
                         // This helps with compatibility on dumb terminals.
                         "set colorsequence classic;"
                         // I don't want to manage window dimensions, let gnuplot do it.
                         "set autoscale;"
                         // Sits above the graph.
                         "set title 'Time (milliseconds) to Service Heap Request';"
                         // Makes it clear x label number corresponds to script lines=lifetime.
                         "set xlabel 'Script Line Number';"
                         // '-'/notitle prevents title inside graph. Set the point to desired char.
                         "plot '-' pt '#' lc rgb 'cyan' notitle\n");

    double total_time = 0;
    for (int req = 0; req < num_requests; req++) {
        total_time += time_per_request[req];
        fprintf(gnuplotPipe, "%d %lf \n", req + 1, time_per_request[req]);
    }

    fprintf(gnuplotPipe, "e\n");
    pclose(gnuplotPipe);
    printf("Average time (milliseconds) per request overall: %lfms\n", total_time / num_requests);
}

