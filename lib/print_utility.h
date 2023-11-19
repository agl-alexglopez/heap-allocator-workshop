#ifndef PRINT_UTILITY_H
#define PRINT_UTILITY_H

#include <stddef.h>

/// Text coloring macros (ANSI character escapes) for printing function colorful
/// output. Consider changing to a more portable library like ncurses.h. However,
/// I don't want others to install ncurses just to explore the project. They
/// already must install gnuplot. Hope this works.
#define COLOR_BLK "\033[34;1m"
#define COLOR_RED "\033[31;1m"
#define COLOR_CYN "\033[36;1m"
#define COLOR_GRN "\033[32;1m"
#define COLOR_NIL "\033[0m"
#define COLOR_ERR COLOR_RED "Error: " COLOR_NIL
#define PRINTER_INDENT (short)13

/// PLAIN prints free block sizes, VERBOSE shows address in the heap and black height of tree.
enum print_style
{
    PLAIN = 0,
    VERBOSE = 1
};

/// Printing enum for printing red black tree structure.
enum print_link
{
    BRANCH = 0, // ├──
    LEAF = 1    // └──
};

/// A struct for plotting helpful data about a heap run on a script.
struct gnuplots
{
    // Arrays that should be malloc'd due to possible large scripts. Think 1million+ requests.
    double *util_percents; // Running utilization average.
    size_t *free_nodes;    // Running count of free nodes.
    double *request_times; // Running count of time per request.
    // All arrays will have the same size as the number of script operations.
    size_t num_ops;
};

///////////////////////////// Plot Desired Information about Allocator  ////////////////////

/// @brief print_gnuplots  a wrapper for the three gnuplot functions with helpful information
///                        in case someone is waiting for large data. It can take time.
/// @brief *graphs         the gnuplots struct containing all the graphs to print.
void print_gnuplots( struct gnuplots *graphs );

#endif
