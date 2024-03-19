/// File: segment.h
/// ---------------
/// An interface to the OS low-level allocator. Used to reserve a large
/// segment of memory to be used by a custom heap allocator.
#ifdef __cplusplus
extern "C" {
#endif
#ifndef SEGMENT_H
#define SEGMENT_H
#include <stddef.h> // for size_t

/// @brief init_heap_segment This function is called to initialize the heap
/// segment and allocate the segment to hold total_size bytes. If
/// init_heap_segment is called again, it discards the current heap segment and
/// re-configures. The function returns the base address of the heap segment if
/// successful or NULL if the initialization failed. The base address of the
/// heap segment is always aligned to start on a page boundary (page size is
/// 4096 bytes).
void *init_heap_segment(size_t total_size);

/// @brief heap_segment_start, heap_segment_size heap_segment_start
/// returns the base address of the current heap segment
/// (NULL if no segment has been initialized).
/// heap_segment_size returns the current segment size in bytes.
void *heap_segment_start();

/// @brief heap_segment_start, heap_segment_size heap_segment_start
/// returns the base address of the current heap segment
/// (NULL if no segment has been initialized).
/// heap_segment_size returns the current segment size in bytes.
size_t heap_segment_size();

#endif
#ifdef __cplusplus
}
#endif
