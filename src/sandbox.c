/// File: sandbox.c
/// ---------------------------
/// Write whatever program you would like.

#include "allocator.h"
#include "segment.h"

#include <stdbool.h>

#define HEAP_SIZE 1ULL << 32

static bool
initialize_heap_allocator() {
    init_heap_segment(HEAP_SIZE);
    return winit(heap_segment_start(), heap_segment_size());
}

int
main(int argc, char *argv[]) {
    if (!initialize_heap_allocator()) {
        return 1;
    }
    (void)argc;
    (void)argv;
    return 0;
}
