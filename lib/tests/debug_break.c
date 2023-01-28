#include "debug_break.h"

void dummy(int signum) {
    // only called if debugger hasn't installed own handler (ignore)
    printf("Signal %d caught\n", signum);
}

