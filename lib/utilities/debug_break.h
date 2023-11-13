/* File: debug_break.h
 * -------------------
 * If running under the debugger, a call to breakpoint() will
 * behave as though execution hit a gdb breakpoint. If not
 * running under debugger, breakpoint() is a no-op. Call this
 * function in your validate_heap() to break when an error is detected.
 *
 *  Written by jzelenski, updated Spring 2018
 */

#ifndef DEBUG_BREAK_H
#define DEBUG_BREAK_H

#define breakpoint() printf( "Break. Line: %d File: %s", __LINE__, __FILE__ );

#endif
