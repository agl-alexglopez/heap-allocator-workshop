/// File: debug_break.h
/// -------------------
/// Debugging helper for heap development.

#ifndef DEBUG_BREAK_H
#define DEBUG_BREAK_H
#include <signal.h>
#include <stdio.h>

/// @brief breakpoint  set this breakpoint on any line where you wish execution to stop. Under normal
///                    program runs the program will simply exit. If triggered in GDB execution will
///                    stop while able to explore the surrounding context, varialbes, and stack frames.
///                    Be sure to step "(gdb) up" out of the raise function to wherever it triggered.
#define breakpoint()                                                                                               \
    do {                                                                                                           \
        (void)fprintf( stderr, " !!Break. Line: %d File: %s, Func: %s\n ", __LINE__, __FILE__, __func__ );         \
        (void)raise( SIGTRAP );                                                                                    \
    } while ( 0 )

#endif
