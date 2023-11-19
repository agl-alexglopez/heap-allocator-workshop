/// Author: Alex G. Lopez
/// File script.h
/// -------------------
/// This file contains the utility functions for processing script files and executing them on
/// custom allocators. It also has utilities for timing individual requests to the heap and
/// plotting key data about a script.
/// The script object that is created from a parsed script will contain all key data needed to run
/// the script with minimal checks to ensure success. These functions and related programs should
/// not be used until you are certain any heap allocators they are calling are correct in their
/// implementations. Use the test_harness.c program to verify correctness.
/// In order to get the most out of the programs in this repository please install gnuplot on your
/// machine. This will allow the plotting functions to output in-terminal graphs in the form of
/// ascii art. They are not the most detailed, but on larger scripts they can be very informative.
/// If you would like to change the output of plotted graphs to an output window, explore the
/// implementations of the plot_ functions. However, I will not provide an interface for that
/// change because the pop up windows are slow and the in-terminal graphs serve their purpose well.
#ifndef SCRIPT_H
#define SCRIPT_H
#include "print_utility.h"
#include <stddef.h> // for size_t

#define NAME_LEN_LIMIT 128

// enum and struct for a single allocator request
enum request_category
{
    ALLOC = 1,
    FREE,
    REALLOC
};

struct request
{
    enum request_category op; // type of request
    size_t id;                // id for free() to use later
    size_t size;              // num bytes for alloc/realloc request
    int lineno;               // which line in file
};

// struct for facts about a single malloc'ed block
struct block
{
    void *ptr;
    size_t size;
};

// struct for info for one script file
struct script
{
    char name[NAME_LEN_LIMIT]; // short name of script
    struct request *ops;       // array of requests read from script
    int num_ops;               // number of requests
    size_t num_ids;            // number of distinct block ids
    struct block *blocks;      // array of memory blocks malloc returns when executing
    size_t peak_size;          // total payload bytes at peak in-use
};

/// @brief exec_request  a wrapper function to execute a single call to the heap allocator. It may
///                      may call malloc(), realloc(), or free().
/// @param *script       the script_t with the information regarding the script file we execute.
/// @param req           the zero-based index of the request to the heap allocator.
/// @param *cur_size     the current size of the heap overall.
/// @param **heap_end    the pointer to the end of the heap, we will adjust if heap grows.
/// @return              0 if there are no errors, -1 if there is an error.
int exec_request( struct script *script, int req, size_t *cur_size, void **heap_end );

/// @brief time_request  a wrapper function for timer functions that allows us to time a request to
///                      the heap. Returns the cpu time of the request in milliseconds.
/// @param *script       the script object that holds data about our script we need to execute.
/// @param req           the current request to the heap we execute.
/// @param *cur_size     the current size of the heap.
/// @param **heap_end    the address of the end of our range of heap memory.
/// @return              the double representing the time to complete one request.
double time_request( struct script *script, int req, size_t *cur_size, void **heap_end );

/// @breif parse_script  parses the script file at the specified path, and returns an object with
///                      info about it.  It expects one request per line, and adds each request's
///                      information to the ops array within the script.  This function throws an
///                      error if the file can't be opened, if a line is malformed, or if the file
///                      is too long to store each request on the heap.
/// @param *path         the path to the .script file to parse.
/// @return              a pointer to the script_t with information regarding the .script requests.
struct script parse_script( const char *path );

/// @brief allocator_error  reports an error while running an allocator script.
/// @param *script          the script_t with information we track form the script file requests.
/// @param lineno           the line number where the error occured.
/// @param *format          the specified format string.
void allocator_error( struct script *script, int lineno, char *format, ... );

#endif
