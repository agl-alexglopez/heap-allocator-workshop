#ifndef PRINT_UTILITY_H
#define PRINT_UTILITY_H

/// Text coloring macros (ANSI character escapes) for printing function colorful
/// output. Consider changing to a more portable library like ncurses.h.
/// However, I don't want others to install ncurses just to explore the project.
/// They already must install gnuplot. Hope this works.
#define COLOR_BLK "\033[34;1m"
#define COLOR_BLU_BOLD "\033[38;5;12m"
#define COLOR_RED_BOLD "\033[38;5;9m"
#define COLOR_RED "\033[31;1m"
#define COLOR_CYN "\033[36;1m"
#define COLOR_GRN "\033[32;1m"
#define COLOR_NIL "\033[0m"
#define COLOR_ERR COLOR_RED "Error: " COLOR_NIL
#define PRINTER_INDENT (short)13

/// PLAIN prints free block sizes, VERBOSE shows address in the heap and black
/// height of tree.
enum print_style { PLAIN = 0, VERBOSE = 1 };

/// Printing enum for printing red black tree structure.
enum print_link {
    BRANCH = 0, // ├──
    LEAF = 1    // └──
};

#endif
