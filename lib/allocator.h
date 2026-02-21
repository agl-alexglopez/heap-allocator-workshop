/// File: allocator.h
/// -----------------
/// Interface file for the custom heap allocator. When we detect we are being
/// included in C++ code we we make this extern "C" code. This helps with the
/// unit testing framework and the correctness tester. If you are implementing
/// your own allocator, read the following function descriptions or use your
/// LSP/IDE while in the .c file to read the docs for the functions.
#ifdef __cplusplus
extern "C" {
#endif
#ifndef ALLOCATOR_H
#    define ALLOCATOR_H

#    include "print_utility.h"
#    include <stdbool.h>
#    include <stddef.h>

enum {
    /// Alignment requirement for all blocks
    ALIGNMENT = 8,
    /// maximum size of block that must be accommodated
    MAX_REQUEST_SIZE = (1 << 30)
};

/// The preproccesor will keep our error enum and strings in sync for printing
/// from the testing suite.
#    define FOREACH_ERR(ERR) /*NOLINT*/                                        \
        ERR(OK)                                                                \
        ERR(ER)                                                                \
        ERR(HEAP_CONTINUES)                                                    \
        ERR(OUT_OF_BOUNDS)
#    define GENERATE_ENUM(ENUM) ENUM,        /*NOLINT*/
#    define GENERATE_STRING(STRING) #STRING, /*NOLINT*/

enum status_error {
    FOREACH_ERR(GENERATE_ENUM)
};

static char const *const err_string[] = { // NOLINT
    FOREACH_ERR(GENERATE_STRING)};

enum ignore_bytes {
    /// NA=Not Applicable. If you are running some testing checks and don't care
    /// about payload bytes insert this value as the payload. It is impossible
    /// to have a payload of zero. This is helpful when you want to unit test
    /// general coalescing behavior and don't care about the specific byte
    /// overhead of a heap allocator. Instead you can focus just on whether the
    /// correct blocks are allocated or free.
    NA = 0,
};

/// This is used for verifying internal state of the heap for small unit testing
/// cases. See the wheap_diff function for further details. Use this as a
/// transactional type to check against the heap in unit tests. The address
/// should be the address of the user maintained malloc'd memory or NULL if the
/// user claims the block to be free. The payload bytes contains the amount or
/// NA if we are not concerned with the specific size during our check. The
/// status_error can indicate what error we encountered. An ER indicates a
/// mismatch between expected and actual address or payload, HEAP_CONTINUES
/// indicates the heap has more blocks than expected and the OUT_OF_BOUNDS error
/// indicates that the heap has fewer than expected.
struct heap_block {
    void *address;
    size_t payload_bytes;
    enum status_error err;
};

/// @brief winit       initializes the mapped memory segment provided by the OS
///                    to our heap allocator. See the
///                    segment.h file for how this segment is obtained and how
///                    to aquire the heap_start pointer. This function should
///                    prepare any internal static data structures of the heap
///                    and prepare the the memory pool according to the design
///                    of the internal implementation. After this function
///                    completes the allocator should be able to pass the
///                    wvalidate_heap function.
/// @param heap_start  the pointer provided by the segment.h mapping function.
/// @param heap_size   the size we want to use for this memory pool. Total bytes
///                    available in the entire heap.
/// @return            true if initialization was successful false if not.
bool winit(void *heap_start, size_t heap_size);

/// @brief wmalloc         obtains a free block from the heap and marks it as
///                        allocated for the user. The returned
///                        memory will be AT LEAST as large as the user
///                        requests.
/// @param requested_size  the size in bytes of the requested memory block.
/// @return                a block of memory at least as large as the user
///                        requested or NULL if impossible.
void *wmalloc(size_t requested_size);

/// @brief wrealloc   reallocates the old block of memory to at least as many
///                   bytes as the user requests. If the
///                   pointer provided is NULL, this function is the same as
///                   malloc. If reallocation fails old memory is not altered or
///                   moved and the old_ptr will still be valid; however, NULL
///                   will be returned. If memory is found and the realloc is
///                   serviced assume the old pointer to be invalid.
/// @param old_ptr    the previous location of memory we must move. Valid if
///                   realloc fails, invalid on success.
/// @param new_size   the newly requested bytes. This may be larger or smaller
///                   than the previous size.
/// @return           NULL if the operation failed, a new memory location if the
///                   request succeeded.
void *wrealloc(void *old_ptr, size_t new_size);

/// @brief wfree   frees a block of memory previously allocated, adding it back
///                to the heap for internal
///                management. It is undefined to free an invalid address.
/// @param ptr     the location of the previously provided memory that should
///                now be freed for management.
void wfree(void *ptr);

/// @brief wvalidate_heap  runs the implementation defined internal consistency
///                        checks. This is a VERY IMPORTANT
///                        function. In the .c file include debug_break.h file
///                        and make use of the BREAKPOINT(); macro along with
///                        this function. To use it write your validation logic
///                        normally with helper functions as required. When a
///                        failure happens and you wish to return false, place
///                        a BREAKPOINT(); before the return false statement and
///                        the program will stop execution at that location if
///                        in GDB. So, when debugging rerun the program and when
///                        this function executes step up through the stack
///                        frames to the current wvalidate_heap location and
///                        examine internal heap state with other printing
///                        functions.
/// @return                true if all checks pass false if not.
bool wvalidate_heap(void);

/// @brief wget_free_total  returns the total number of free nodes the heap is
///                         managing currently. The expectaion
///                         is this function runs in O(1) time as it may be
///                         called in hot timing loops targeted at big O
///                         analysis and we do not want it interfering with
///                         runtime trends.
/// @return                 the number of free blocks of memory in our internal
///                         data structure.
size_t wget_free_total(void);

/// @brief wprint_free_nodes  Prints a visual representation of the free nodes
///                           in the heap in the form of
///                           the data structure being used to manage them. You
///                           can print the nodes in the PLAIN or VERBOSE style.
///                           Plain will only show the sizes in bytes that the
///                           blocks store, while VERBOSE will show their
///                           addresses in the heap and for the tree allocators,
///                           the black height of the tree as well. This
///                           function will vary depending on the
///                           implementation. See other allocator examples for
///                           ideas.
/// @param style              VERBOSE or PLAIN depending on how many internal
///                           details are desired.
void wprint_free_nodes(enum print_style style);

/// @brief wheap_align   each heap allocator may align blocks differently. This
///                      function should return the internal
///                      block size used for an allocator when rounding up a
///                      request. For example, a request of 8 bytes will need to
///                      be rounded up to some larger amount to accomodate
///                      internal heap data structures in most implementations.
///                      This function should return that rounded figure.
/// @param request       the client request unaware of any alignment
///                      requirements.
/// @return              the total payload bytes available to the user after
///                      rounding operations.
size_t wheap_align(size_t request);

/// @brief wheap_capacity   returns the total number of free bytes available in
///                         the heap. This could be a slow
///                         operation depending on the number of free nodes
///                         managed. It is required to run in in O(N) time but
///                         implementers could choose to make it a constant time
///                         operation.
/// @return                 the number of bytes in the heap available to the
///                         client currently.
size_t wheap_capacity(void);

/// @brief wheap_diff      this is a transactional function targeted at the unit
///                        testing framework. For small unit
///                        testing cases we can open up our allocators to verify
///                        certain expectations about the heap state after or
///                        before malloc, realloc, and free. This is helpful for
///                        verifying correctness in our coalescing
///                        implementation and reallocation logic. Given an array
///                        of expected heap blocks, read each block and compare
///                        the expectation to the actual internal state of the
///                        heap. The checks should use the following logic.
///                         - Expected free blocks have NULL as their address.
///                         - Expected allocated blocks have the user maintained
///                         address.
///                         - If NA is provided as the payload bytes, we don't
///                         care about payload. Respond with NA.
///                         - If everything matches provide the OK status in the
///                         final field
///                         - If there is a mismatch use the ER status to
///                         indicate a mismatch occurred.
///                         - If the expected number of blocks is greater than
///                         actual indicate OUT_OF_BOUNDS.
///                         - If the heap continues past expected, fill the
///                         final actual slot with HEAP_CONTINUES
/// @param [\in] expected  the user provided expected state of all blocks in the
///                        heap.
/// @param [\out] actual   the implementation defined state of the heap at the
///                        time of the request.
/// @param len             the length of the expected array.
void wheap_diff(const struct heap_block expected[], struct heap_block actual[],
                size_t len);

/// @brief wheap_dump prints the contents of the heap as the implementer sees
///                   fit. It is highly recommended to develop this function
///                   immediately for faster development and debugging of the
///                   heap allocator. Left as a interface function to encourage
///                   implementation for new users.
void wheap_dump(void);

#endif
#ifdef __cplusplus
}
#endif
