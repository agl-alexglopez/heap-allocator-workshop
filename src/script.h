/* Author: Alex G. Lopez
 *
 * File script.h
 * -------------------
 * This file contains the utility functions for processing script files and executing them on
 * custom allocators. It also has utilities for timing individual requests to the heap and
 * plotting key data about a script.
 *
 * The script object that is created from a parsed script will contain all key data needed to run
 * the script with minimal checks to ensure success. These functions and related programs should
 * not be used until you are certain any heap allocators they are calling are correct in their
 * implementations. Use the test_harness.c program to verify correctness.
 *
 * In order to get the most out of the programs in this repository please install gnuplot on your
 * machine. This will allow the plotting functions to output in-terminal graphs in the form of
 * ascii art. They are not the most detailed, but on larger scripts they can be very informative.
 * If you would like to change the output of plotted graphs to an output window, explore the
 * implementations of the plot_ functions. However, I will not provide an interface for that
 * change because the pop up windows are slow and the in-terminal graphs serve their purpose well.
 */
#ifndef _SCRIPT_H_
#define _SCRIPT_H_
#include "allocator.h"

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

// struct for info for one script file
typedef struct {
    char name[128];         // short name of script
    request_t *ops;         // array of requests read from script
    int num_ops;            // number of requests
    int num_ids;            // number of distinct block ids
    block_t *blocks;        // array of memory blocks malloc returns when executing
    size_t peak_size;       // total payload bytes at peak in-use
} script_t;

// A struct for plotting helpful data about a heap run on a script.
typedef struct {
    // Arrays that should be malloc'd due to possible large scripts. Think 1million+ requests.
    double *util_percents;   // Running utilization average.
    size_t *free_nodes;      // Running count of free nodes.
    double *request_times;   // Running count of time per request.
    // All arrays will have the same size as the number of script operations.
    int num_ops;
} gnuplots;

/* @brief exec_request  a wrapper function to execute a single call to the heap allocator. It may
 *                      may call malloc(), realloc(), or free().
 * @param *script       the script_t with the information regarding the script file we execute.
 * @param req           the zero-based index of the request to the heap allocator.
 * @param *cur_size     the current size of the heap overall.
 * @param **heap_end    the pointer to the end of the heap, we will adjust if heap grows.
 * @return              0 if there are no errors, -1 if there is an error.
 */
int exec_request(script_t *script, int req, size_t *cur_size, void **heap_end);

/* @brief time_request  a wrapper function for timer functions that allows us to time a request to
 *                      the heap. Returns the cpu time of the request in milliseconds.
 * @param *script       the script object that holds data about our script we need to execute.
 * @param req           the current request to the heap we execute.
 * @param *cur_size     the current size of the heap.
 * @param **heap_end    the address of the end of our range of heap memory.
 * @return              the double representing the time to complete one request.
 */
double time_request(script_t *script, int req, size_t *cur_size, void **heap_end);

/* @breif parse_script  parses the script file at the specified path, and returns an object with
 *                      info about it.  It expects one request per line, and adds each request's
 *                      information to the ops array within the script.  This function throws an
 *                      error if the file can't be opened, if a line is malformed, or if the file
 *                      is too long to store each request on the heap.
 * @param *path         the path to the .script file to parse.
 * @return              a pointer to the script_t with information regarding the .script requests.
 */
script_t parse_script(const char *filename);

/* @brief print_gnuplots  a wrapper for the three gnuplot functions with helpful information in
 *                        case someone is waiting for large data. It can take time.
 * @brief *graphs         the gnuplots struct containing all the graphs to print.
 */
void print_gnuplots(gnuplots *graphs);

/* @brief plot_free_nodes      plots number of free nodes over the course of the heaps lifetime.
 *                             By default prints an ascii graph to the terminal. Can be edited
 *                             or adapted to output to popup window. Requires gnuplot.
 * @param *totals_per_request  the number of total free nodes after each line of script executes.
 * @param num_ops              size of the array of totals equal to number of lines in script.
 */
void plot_free_nodes(size_t *free_nodes, int num_ops);

/* @brief plot_util_percents  plots the heap utilization over its lifetime as a percentage.
 * @param *util_percents      the mallocd array of percentages.
 * @param num_ops             the size of the array.
 */
void plot_util_percents(double *util_percents, int num_ops);

/* @brief plot_request_times  plots the time to service heap requests over heap lifetime.
 * @param *request_times      the mallocd array of time measurements.
 * @param num_ops             the number of requests in the script corresponding to measurements.
 */
void plot_request_times(double *request_times, int num_ops);

/* @brief allocator_error  reports an error while running an allocator script.
 * @param *script          the script_t with information we track form the script file requests.
 * @param lineno           the line number where the error occured.
 * @param *format          the specified format string.
 */
void allocator_error(script_t *script, int lineno, char* format, ...);

#endif
