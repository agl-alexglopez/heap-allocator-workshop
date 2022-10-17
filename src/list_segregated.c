/**
 * File: list_segregated.c
 * ---------------------
 * This is a basic implementation of a segregated free list (aka fits) heap allocator. We
 * maintain 15 list sizes and do a first fit search on loosely sorted lists, approximating a best
 * fit search of the heap. We simply add new elements to a list class at the front to speed things
 * up rather than maintaining 15 sorted lists. This helps bring us closer to a O(lgN) runtime for
 * a list based allocator and only costs a small amount of utilization.
 *
 * Citations:
 * -------------------
 * 1. Bryant and O'Hallaron, Computer Systems: A Programmer's Perspective, Chapter 9.
 *    I used the explicit free fits outline from the textbook, specifically
 *    regarding how to implement left and right coalescing. I even used their suggested
 *    optimization of an extra control bit so that the footers to the left can be overwritten
 *    if the block is allocated so the user can have more space. I also took their basic outline
 *    for a segregated fits list to implement this allocator.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "./allocator.h"
#include "./debug_break.h"

typedef size_t header_t;
typedef unsigned char byte_t;

typedef enum header_status_t {
    FREE = 0x0UL,
    ALLOCATED = 0x1UL,
    LEFT_ALLOCATED = 0x2UL,
    LEFT_FREE = ~0x2UL
} header_status_t;

typedef struct free_node {
    struct free_node *next;
    struct free_node *prev;
}free_node;

/* Size Order Classes Maintained by an Array of segregated fits lists
 *     - Our size classes stand for the minimum size of a node in the list less than the next.
 *     - 15 Size Classes (in bytes):

            32,        40,         48,          56,          64-127,
            128-255,   256-511,    512-1023,    1024-2047,   2048-4095,
            4096-8191, 8192-16383, 16384-32767, 32768-65535, 65536+,

 *     - A first fit search will yeild approximately the best fit.
 *     - We will have one dummy node to serve as both the head and tail of all lists.
 *     - Be careful, last index is USHRT_MAX=65535!=65536. Mind the last index size.
 */
typedef struct seg_node {
    unsigned short size;
    free_node *start;
}seg_node;

/* Sacrifice one block on the heap for simple coalescing and freeing. It is both head and tail. We
 * never use it to know a nodes location, rather it helps us avoid checking the null case
 * and allows for invariant coding techniques. It can also tell us if we are the first node in a
 * certain segregated list, so we can fix the table in special cases.
 */
static struct fits {
    seg_node *table;
    free_node *nil;
    size_t total;
}fits;

static struct heap {
    void *client_start;
    void *client_end;
    size_t client_size;
}heap;

/* This is taken from Sean Eron Anderson's Bit Twiddling Hacks. See the find_index() function for
 * how it helps our implementation find the index of a node in a given list that is a base2 power
 * of 2. We want to jump straight to the index by finding the log2(block_size).
 * https://graphics.stanford.edu/~seander/bithacks.html
 */
static const char LogTable256[256] =
{
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
    LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
};

#define SIZE_MASK ~0x7UL
#define STATUS_CHECK 0x4UL
#define FREE_NODE_WIDTH (unsigned short)16
#define HEADER_AND_FREE_NODE (unsigned short)24
#define MIN_BLOCK_SIZE (unsigned short)32
#define TABLE_SIZE (unsigned short)15
#define SMALL_TABLE_SIZE (unsigned short)4
#define SMALL_TABLE_MAX (unsigned short)56
#define LARGE_TABLE_MIN (unsigned short)64
#define TABLE_BYTES (15 * sizeof(seg_node))
#define INDEX_0 (unsigned short)0
#define INDEX_0_SIZE (unsigned short)32
#define INDEX_1 (unsigned short)1
#define INDEX_1_SIZE (unsigned short)40
#define INDEX_2 (unsigned short)2
#define INDEX_2_SIZE (unsigned short)48
#define INDEX_3 (unsigned short)3
#define INDEX_3_SIZE (unsigned short)56
#define INDEX_OFFSET 2U


/* * * * * * * * *   Static Minor Heap Methods   * * * * * * * * * * */


/* @brief extract_block_size  given a valid header find the total size of the header and block.
 * @param *header_location    the pointer to the current header block we are on.
 * @return                    the size in bytes of the header and memory block.
 */
static size_t extract_block_size(header_t header_location) {
    return header_location & SIZE_MASK;
}

/* @brief *get_right_header  advances the header_t pointer to the next header in the heap.
 * @param *header            the valid pointer to a heap header within the memory range.
 * @param block_size         the size of the current block.
 * @return                   a header_t pointer to the next block in the heap.
 */
static header_t *get_right_header(header_t *header, size_t block_size) {
    return (header_t *)((byte_t *)header + block_size);
}

/* @brief *get_left_header  uses the left block size gained from the footer to move to the header.
 * @param *header           the current header at which we reside.
 * @param left_block_size   the space of the left block as reported by its footer.
 * @return                  a header_t pointer to the header for the block to the left.
 */
static header_t *get_left_header(header_t *header) {
    header_t *left_footer = (header_t *)((byte_t *)header - ALIGNMENT);
    return (header_t *)((byte_t *)header - (*left_footer & SIZE_MASK));
}

/* @brief is_block_allocated  will determine if a block is marked as allocated.
 * @param *header_location    the valid header we will determine status for.
 * @return                    true if the block is allocated, false if not.
 */
static bool is_block_allocated(header_t header_val) {
    return header_val & ALLOCATED;
}

/* @brief *get_client_space  get the pointer to the start of the client available memory.
 * @param *header            the valid header to the current block of heap memory.
 * @return                   a pointer to the first available byte of client heap memory.
 */
static free_node *get_free_node(header_t *header) {
    return (free_node *)((byte_t *)header + ALIGNMENT);
}

/* @brief *get_block_header  steps to the left from the user-available space to get the pointer
 *                           to the header_t header.
 * @param *user_mem_space    the void pointer to the space available for the user.
 * @return                   the header immediately to the left associated with memory block.
 */
static header_t *get_block_header(free_node *user_mem_space) {
    return (header_t *)((byte_t *)user_mem_space - ALIGNMENT);
}


/* * * * * * * * *  Static Heap Helper Functions  * * * * * * * * * * */


/* @brief init_header  initializes the header in the header_t_header field to reflect the
 *                     specified status and that the left neighbor is allocated or unavailable.
 * @param *header      the header we will initialize.
 * @param block_size   the size, including the header, of the entire block.
 * @param status       FREE or ALLOCATED to reflect the status of the memory.
 */
static void init_header(header_t *header, size_t block_size, header_status_t header_status) {
    *header = LEFT_ALLOCATED | block_size | header_status;
}

/* @brief init_footer  initializes the footer to reflect that the associated block is now free. We
 *                     will only initialize footers on free blocks. We use the the control bits
 *                     in the right neighbor if the block is allocated and allow the user to have
 *                     the footer space.
 * @param *header      a pointer to the header_t that is now free and will have a footer.
 * @param block_size   the size to use to update the footer of the block.
 */
static void init_footer(header_t *header, size_t block_size) {
    header_t *footer = (header_t*)((byte_t *)header + block_size - ALIGNMENT);
    *footer = LEFT_ALLOCATED | block_size | FREE;
}

/* @brief is_left_space  checks the control bit in the second position to see if the left neighbor
 *                       is allocated or free to use for coalescing.
 * @param *header        the current block for which we are checking the left neighbor.
 * @return               true if there is space to the left, false if not.
 */
static bool is_left_space(header_t *header) {
    return !(*header & LEFT_ALLOCATED);
}

/* @brief find_index  finds the index in the lookup table that a given block size is stored in.
 * @param block_size  the current block we are trying to find table index for.
 * @return            the index in the lookup table.
 * @citation          the bit manipulation is taken from Sean Anderson's Bit Twiddling Hacks.
 *                    https://graphics.stanford.edu/~seander/bithacks.html
 */
static unsigned int find_index(unsigned int block_size) {
    if (block_size <= SMALL_TABLE_MAX) {
        switch (block_size) {
            case INDEX_0_SIZE:
                return INDEX_0;
            break;
            case INDEX_1_SIZE:
                return INDEX_1;
            break;
            case INDEX_2_SIZE:
                return INDEX_2;
            break;
            case INDEX_3_SIZE:
                return INDEX_3;
            break;
            default:
                fprintf(stderr, "Error: Size %ubytes is out of alignment.", block_size);
                abort();
            break;
        }
    }
    // block_size = 32-bit word to find the log of
    unsigned int log_2;  // log_2 will be lg(block_size)
    register unsigned int temp_1, temp_2;
    if ((temp_2 = block_size >> 16)) {
      log_2 = (temp_1 = temp_2 >> 8) ? 24 + LogTable256[temp_1] : 16 + LogTable256[temp_2];
    } else {
      log_2 = (temp_1 = block_size >> 8) ? 8 + LogTable256[temp_1] : LogTable256[block_size];
    }
    /* After small sizes we double in base2 powers of 2 so we can predictably find our index
     * with a fixed offset. The log_2 of each size class increments linearly by 1.
     */
    return log_2 - INDEX_OFFSET;
}

/* @brief splice_free_node  removes a free node out of the free node list.
 * @param *to_splice        the heap node that we are either allocating or splitting.
 * @param *block_size       number of bytes that is used by the block we are splicing.
 */
static void splice_free_node(free_node *to_splice, size_t block_size) {
    // Catch if we are the first node pointed to by the lookup table.
    if (fits.nil == to_splice->prev) {
        int index = 0;
        // Block size may be larger than unsigned int, so handle here before find_index function.
        if (block_size > fits.table[TABLE_SIZE - 1].size) {
            index = TABLE_SIZE - 1;
        } else {
            // We have a few optimizations possible for finding to which list we belong.
            index = find_index(block_size);
        }
        fits.table[index].start = to_splice->next;
        to_splice->next->prev = fits.nil;
    } else {
        // Because we have a sentinel we don't need to worry about middle or last node or NULL.
        to_splice->next->prev = to_splice->prev;
        to_splice->prev->next = to_splice->next;
    }
    fits.total--;
}

/* @brief init_free_node  initializes the header and footer of a free node, informs the right
 *                        neighbor of its status and ads the node to the explicit free list.
 * @param to_add          the newly freed header for a heap_node to prepare for the free list.
 * @param block_size      the size of free memory that will now be added to the free list.
 */
static void init_free_node(header_t *to_add, size_t block_size) {
    *to_add = LEFT_ALLOCATED | block_size;
    header_t *footer = (header_t*)((byte_t *)to_add + block_size - ALIGNMENT);
    *footer = *to_add;
    header_t *neighbor = get_right_header(to_add, block_size);
    *neighbor &= LEFT_FREE;
    free_node *free_add = get_free_node(to_add);

    int index = 0;
    for ( ; index < TABLE_SIZE - 1 && block_size >= fits.table[index + 1].size; index++) {
    }
    // For speed push nodes to front of the list. We are loosely sorted by at most powers of 2.
    free_node *cur = fits.table[index].start;
    fits.table[index].start = free_add;
    free_add->prev = fits.nil;
    free_add->next = cur;
    cur->prev = free_add;
    fits.total++;
}

/* @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
 *                      split, it will add the newly freed split block to the free list.
 * @param *free_block   a pointer to the node for a free block in its entirety.
 * @param request       the user request for space.
 * @param block_space   the entire space that we have to work with.
 * @return              a void poiter to generic space that is now ready for the client.
 */
static void *split_alloc(header_t *free_block, size_t request, size_t block_space) {
    header_t *neighbor = NULL;
    if (block_space >= request + MIN_BLOCK_SIZE) {
        neighbor = get_right_header(free_block, request);
        // This takes care of the neighbor and ITS neighbor with appropriate updates.
        init_free_node(neighbor, block_space - request);
    } else {
        request = block_space;
        neighbor = get_right_header(free_block, block_space);
        *neighbor |= LEFT_ALLOCATED;
    }
    init_header(free_block, request, ALLOCATED);
    return get_free_node(free_block);
}

/* @brief *coalesce           performs an in place coalesce to the right and left on free and
 *                            realloc. It will free any blocks that it coalesces, but it is the
 *                            caller's responsibility to add a footer or add the returned block to
 *                            the free list. This protects the caller from overwriting user data
 *                            with a footer in the case of a minor reallocation in place.
 * @param *leftmost_header    the block which may move left. It may also gain space to the right.
 * @return                    a pointer to the leftmost node after coalescing. It may move. It is
 *                            the caller's responsibility to ensure they do not overwrite user data
 *                            or forget to add this node to free list, whichever they are doing.
 */
static header_t *coalesce(header_t *leftmost_header) {
    size_t coalesced_space = extract_block_size(*leftmost_header);
    header_t *right_space = get_right_header(leftmost_header, coalesced_space);
    if (right_space != heap.client_end && !is_block_allocated(*right_space)) {
        size_t block_size = extract_block_size(*right_space);
        coalesced_space += block_size;
        splice_free_node(get_free_node(right_space), block_size);
    }

    if (is_left_space(leftmost_header)) {
        leftmost_header = get_left_header(leftmost_header);
        size_t block_size = extract_block_size(*leftmost_header);
        coalesced_space += block_size;
        splice_free_node(get_free_node(leftmost_header), block_size);
    }
    init_header(leftmost_header, coalesced_space, FREE);
    return leftmost_header;
}


/* * * * * * * * * * *   Shared Heap Functions   * * * * * * * * */


/* @brief roundup         rounds up a size to the nearest multiple of two to be aligned in the heap.
 * @param requested_size  size given to us by the client.
 * @param multiple        the nearest multiple to raise our number to.
 * @return                rounded number.
 */
size_t roundup(size_t requested_size, size_t multiple) {
    return (requested_size + multiple - 1) & ~(multiple - 1);
}

/* @brief get_free_total  returns the total number of free nodes in the heap.
 * @return                a size_t representing the total quantity of free nodes.
 */
size_t get_free_total() {
    return fits.total;
}

/* @brief myinit      initializes the heap space for use for the client.
 * @param *heap_start pointer to the space we will initialize as our heap.
 * @param heap_size   the desired space for the heap memory overall.
 * @return            true if the space if the space is successfully initialized false if not.
 */
bool myinit(void *heap_start, size_t heap_size) {
    if (heap_size < MIN_BLOCK_SIZE) {
        return false;
    }

    heap.client_size = roundup(heap_size, ALIGNMENT);
    // This costs some memory in exchange for ease of use and low instruction counts.
    fits.nil = (free_node*)((byte_t*)heap_start + (heap.client_size - FREE_NODE_WIDTH));
    fits.nil->prev = NULL;
    fits.nil->next = NULL;

    // Initialize array of free list sizes.
    heap_start = (seg_node (*)[TABLE_SIZE]) heap_start;
    fits.table = heap_start;
    // Small sizes go from 32 to 56 by increments of 8, and lists will only hold those sizes
    unsigned short size = MIN_BLOCK_SIZE;
    for (int index = 0; index < SMALL_TABLE_SIZE; index++, size += ALIGNMENT) {
        fits.table[index].size = size;
        fits.table[index].start = fits.nil;
    }
    // Large sizes double until end of array except last index needs special attention.
    size = LARGE_TABLE_MIN;
    for (int index = SMALL_TABLE_SIZE; index < TABLE_SIZE - 1; index++, size *= 2) {
        fits.table[index].size = size;
        fits.table[index].start = fits.nil;
    }
    // Be careful here. We can't double to get 14th index. USHRT_MAX=65535 not 65536.
    fits.table[TABLE_SIZE - 1].size = USHRT_MAX;
    fits.table[TABLE_SIZE - 1].start = fits.nil;


    header_t *first_block = (header_t *)((byte_t *)heap_start + TABLE_BYTES);
    init_header(first_block, heap.client_size - TABLE_BYTES - FREE_NODE_WIDTH, FREE);
    init_footer(first_block, heap.client_size - TABLE_BYTES - FREE_NODE_WIDTH);

    free_node *first_free = (free_node *)((byte_t *)first_block + ALIGNMENT);
    first_free->next = fits.nil;
    first_free->prev = fits.nil;
    // Insert this first free into the appropriately sized list.
    init_free_node(first_block, heap.client_size - TABLE_BYTES - FREE_NODE_WIDTH);

    heap.client_start = first_block;
    heap.client_end = fits.nil;
    fits.total = 1;
    return true;
}

/* @brief *mymalloc       finds space for the client from the segregated free node list.
 * @param requested_size  the user desired size that we will round up and align.
 * @return                a void pointer to the space ready for the user or NULL if the request
 *                        could not be serviced because it was invalid or there is no space.
 */
void *mymalloc(size_t requested_size) {
    if (requested_size == 0 || requested_size > MAX_REQUEST_SIZE) {
        return NULL;
    }

    size_t rounded_request = roundup(requested_size + HEADER_AND_FREE_NODE, ALIGNMENT);
    for (int i = 0; i < TABLE_SIZE; i++) {
        // All lists hold advertised size and up to one byte less than next list.
        if (i == TABLE_SIZE - 1 || rounded_request < fits.table[i + 1].size) {
            for (free_node *node = fits.table[i].start; node != fits.nil; node = node->next) {

                header_t *header = get_block_header(node);
                size_t free_space = extract_block_size(*header);
                if (free_space >= rounded_request) {
                    splice_free_node(node, free_space);
                    // Handoff decision to split or not to our helper, takes care of all details.
                    return split_alloc(header, rounded_request, free_space);
                }
            }
        }
    }
    return NULL;
}

/* @brief *myrealloc  reallocates space for the client. It uses right coalescing in place
 *                    reallocation. It will free memory on a zero request and a non-Null pointer.
 *                    If reallocation fails, the memory does not move and we return NULL.
 * @param *old_ptr    the old memory the client wants resized.
 * @param new_size    the client's newly desired size. May be larger or smaller.
 * @return            new space if the pointer is null, NULL on invalid request or inability to
 *                    find space.
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    if (new_size > MAX_REQUEST_SIZE) {
        return NULL;
    }
    if (old_ptr == NULL) {
        return mymalloc(new_size);
    }
    if (new_size == 0) {
        myfree(old_ptr);
        return NULL;
    }
    size_t size_needed = roundup(new_size + HEADER_AND_FREE_NODE, ALIGNMENT);
    header_t *old_header = get_block_header(old_ptr);
    size_t old_space = extract_block_size(*old_header);

    // Spec requires we coalesce as much as possible even if there were sufficient space in place.
    header_t *leftmost_header = coalesce(old_header);
    size_t coalesced_total = extract_block_size(*leftmost_header);
    void *client_block = get_free_node(leftmost_header);

    if (coalesced_total >= size_needed) {
        /* memmove seems bad but if we just coalesced right and did not find space, I would have to
         * search free list, update free list, split the block, memcpy, add a split block back
         * to the free list, update all headers, then come back to left coalesce the space we left
         * behind. This is fewer operations and I did not measure a significant time cost.
         */
        if (leftmost_header != old_header) {
            memmove(client_block, old_ptr, old_space);
        }
        client_block = split_alloc(leftmost_header, size_needed, coalesced_total);
    } else if ((client_block = mymalloc(size_needed))) {
        memcpy(client_block, old_ptr, old_space);
        init_free_node(leftmost_header, coalesced_total);
    }
    // NULL or the space we found from in-place or malloc.
    return client_block;
}

/* @brief myfree  frees valid user memory from the heap.
 * @param *ptr    a pointer to previously allocated heap memory.
 */
void myfree(void *ptr) {
    if (ptr != NULL) {
        header_t *to_free = get_block_header(ptr);
        to_free = coalesce(to_free);
        init_free_node(to_free, extract_block_size(*to_free));
    }
}


/* * * * * * * * *   Static Debugging Helpers   * * * * * * * * * * */


/* @brief is_header_corrupted  will determine if a block has the 3rd bit on, which is invalid.
 * @param *header_location     the valid header we will determine status for.
 * @return                     true if the block has the second or third bit on.
 */
static bool is_header_corrupted(header_t header_val) {
    return header_val & STATUS_CHECK;
}

/* @breif check_init  checks the internal representation of our heap, especially the head and tail
 *                    nodes for any issues that would ruin our algorithms.
 * @return            true if everything is in order otherwise false.
 */
static bool check_init() {
    void *first_address = fits.table;
    void *last_address = (byte_t *)fits.nil + FREE_NODE_WIDTH;
    if ((byte_t *)last_address - (byte_t *)first_address != heap.client_size) {
        breakpoint();
        return false;
    }
    // Check our lookup table. Sizes should never be altered and pointers should never be NULL.
    unsigned short size = MIN_BLOCK_SIZE;
    for (int i = 0; i < SMALL_TABLE_SIZE; i++, size += ALIGNMENT) {
        if (fits.table[i].size != size) {
            breakpoint();
            return false;
        }
        // This should either be a valid node or the sentinel.
        if (NULL == fits.table[i].start) {
            breakpoint();
            return false;
        }
    }
    size = LARGE_TABLE_MIN;
    for (int i = SMALL_TABLE_SIZE; i < TABLE_SIZE - 1; i++, size *= 2) {
        if (fits.table[i].size != size) {
            breakpoint();
            return false;
        }
        // This should either be a valid node or the nil.
        if (NULL == fits.table[i].start) {
            breakpoint();
            return false;
        }
    }
    if (fits.table[TABLE_SIZE - 1].size != USHRT_MAX) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief is_valid_header  checks the header of a block of memory to make sure that is not an
 *                         unreasonable size or otherwise corrupted.
 * @param *header          the header to a block of memory
 * @param block_size       the reported size of this block of memory from its header.
 * @return                 true if the header is valid, false otherwise.
 */
static bool is_valid_header(header_t header, size_t block_size) {
    // Most definitely impossible and our header is corrupted. Pointer arithmetic would fail.
    if (block_size > heap.client_size) {
        return false;
    }
    if (is_header_corrupted(header)) {
        return false;
    }
    if (block_size % ALIGNMENT != 0) {
        return false;
    }
    return true;
}

/* @brief get_size_used    loops through all blocks of memory to verify that the sizes
 *                         reported match the global bookeeping in our struct.
 * @param *total_free_mem  the output parameter of the total size used as another check.
 * @return                 true if our tallying is correct and our totals match.
 */
static bool is_memory_balanced(size_t *total_free_mem) {
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    header_t *cur_header = heap.client_start;
    size_t size_used = FREE_NODE_WIDTH + TABLE_BYTES;
    size_t total_free_nodes = 0;
    while (cur_header != heap.client_end) {
        size_t block_size_check = extract_block_size(*cur_header);
        if (block_size_check == 0) {
            breakpoint();
            return false;
        }

        if (!is_valid_header(*cur_header, block_size_check)) {
            breakpoint();
            return false;
        }
        if (is_block_allocated(*cur_header)) {
            size_used += block_size_check;
        } else {
            total_free_nodes++;
            *total_free_mem += block_size_check;
        }
        cur_header = get_right_header(cur_header, block_size_check);
    }
    if (size_used + *total_free_mem != heap.client_size) {
        breakpoint();
        return false;
    }
    if (total_free_nodes != fits.total) {
        breakpoint();
        return false;
    }
    return true;
}


/* @brief are_fits_valid  loops through only the segregated fits list to make sure it matches
 *                        the loop we just completed by checking all blocks.
 * @param total_free_mem  the input from a previous loop that was completed by jumping block
 *                        by block over the entire heap.
 * @return                true if the segregated fits list totals correctly, false if not.
 */
static bool are_fits_valid(size_t total_free_mem) {
    size_t linked_free_mem = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        for (free_node *cur = fits.table[i].start; cur != fits.nil; cur = cur->next) {
            header_t *cur_header = get_block_header(cur);
            size_t cur_size = extract_block_size(*cur_header);
            if (i != TABLE_SIZE - 1 && cur_size >= fits.table[i + 1].size) {
                breakpoint();
                return false;
            }
            if (is_block_allocated(*cur_header)) {
                breakpoint();
                return false;
            }
            // This algorithm does not allow two free blocks to remain next to one another.
            if (is_left_space(get_block_header(cur))) {
                breakpoint();
                return false;
            }
            linked_free_mem += cur_size;
        }
    }
    if (total_free_mem != linked_free_mem) {
        breakpoint();
        return false;
    }
    return true;
}


/* * * * * * * * * * *   Shared Debugging  * * * * * * * * * * * * * */


/* @brief validate_heap  runs various checks to ensure that every block of the heap is well formed
 *                       with valid sizes, alignment, and initializations.
 * @return               true if the heap is valid and false if the heap is invalid.
 */
bool validate_heap() {
    if (!check_init()) {
        breakpoint();
        return false;
    }
    size_t total_free_mem = 0;
    if (!is_memory_balanced(&total_free_mem)) {
        breakpoint();
        return false;
    }
    if (!are_fits_valid(total_free_mem)) {
        breakpoint();
        return false;
    }
    return true;
}


/* * * * * * * * *   Static Printing Helpers   * * * * * * * * * * */


/* @brief print_fits  prints the segregated fits free list in order to check if splicing and
 *                    adding is progressing correctly.
 */
static void print_fits(print_style style) {
    bool alternate = false;
    for (int i = 0; i < TABLE_SIZE; i++) {
        printf(COLOR_GRN);
        if (i == TABLE_SIZE - 1) {
            printf("[CLASS:%ubytes+]=>", fits.table[i].size);
        } else if (i >= SMALL_TABLE_SIZE) {
            printf("[CLASS:%u-%ubytes]=>", fits.table[i].size, fits.table[i + 1].size - 1U);
        } else {
            printf("[CLASS:%ubytes]=>", fits.table[i].size);
        }
        printf(COLOR_NIL);
        if (alternate) {
            printf(COLOR_RED);
        } else {
            printf(COLOR_CYN);
        }
        alternate = !alternate;
        for (free_node *cur = fits.table[i].start; cur != fits.nil; cur = cur->next) {
            if (cur) {
                header_t *cur_header = get_block_header(cur);
                printf("<=>[");
                if (style == VERBOSE) {
                    printf("%p:", get_block_header(cur));
                }
                printf("(%zubytes)]", extract_block_size(*cur_header));
            } else {
                printf("Something went wrong. NULL free fits node.\n");
                break;
            }
        }
        printf("<=>[%p]\n", fits.nil);
        printf(COLOR_NIL);
    }
}

/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *header            a valid header to a block of allocated memory.
 */
static void print_alloc_block(header_t *header) {
    size_t block_size = extract_block_size(*header) - ALIGNMENT;
    printf(COLOR_GRN);
    // We will see from what direction our header is messed up by printing 16 digits.
    printf("%p: HEADER->0x%016zX->[ALOC-%zubytes]\n", header, *header, block_size);
    printf(COLOR_NIL);
}

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *header            a valid header to a block of allocated memory.
 */
static void print_free_block(header_t *header) {
    size_t full_size = extract_block_size(*header);
    size_t block_size = full_size - ALIGNMENT;
    header_t *footer = (header_t *)((byte_t *)header + full_size - ALIGNMENT);
    /* We should be able to see the header is the same as the footer. If they are not the same
     * we will face subtle bugs that are very hard to notice.
     */
    if (*footer != *header) {
        *footer = ULONG_MAX;
    }
    printf(COLOR_RED);
    printf("%p: HEADER->0x%016zX->[FREE-%zubytes->FOOTER->%016zX]\n",
            header, *header, block_size, *footer);
    printf(COLOR_NIL);
}

/* @brief print_error_block  prints a helpful error message if a block is corrupted.
 * @param *header            a header to a block of memory.
 * @param full_size          the full size of a block of memory, not just the user block size.
 */
static void print_error_block(header_t *header, size_t full_size) {
    size_t block_size = full_size - ALIGNMENT;
    printf(COLOR_CYN);
    printf("\n%p: HEADER->0x%016zX->%zubytes\n",
            header, *header, block_size);
    printf("Block size is too large and header is corrupted.\n");
    printf(COLOR_NIL);
}

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 */
static void print_bad_jump(header_t *current, header_t *prev) {
    size_t prev_size = extract_block_size(*prev);
    size_t cur_size = extract_block_size(*current);
    printf(COLOR_CYN);
    printf("A bad jump from the value of a header has occured. Bad distance to next header.\n");
    printf("The previous address: %p:\n", prev);
    printf("\tHeader Hex Value: %016zX:\n", *prev);
    printf("\tBlock Byte Value: %zubytes:\n", prev_size);
    printf("\nJump by %zubytes...\n", prev_size);
    printf("The current address: %p:\n", current);
    printf("\tHeader Hex Value: %016zX:\n", *current);
    printf("\tBlock Byte Value: %zubytes:\n", cur_size);
    printf("\nJump by %zubytes...\n", cur_size);
    printf("Current state of the free list:\n");
    printf(COLOR_NIL);
    print_fits(VERBOSE);
}


/* * * * * * * * * * * *   Shared Printing Debugger   * * * * * * * * * * */


/* @brief print_free_nodes  a shared function across allocators requesting a printout of internal
 *                          data structure used for free nodes of the heap.
 * @param style             VERBOSE or PLAIN. Plain only includes byte size, while VERBOSE includes
 *                          memory addresses.
 */
void print_free_nodes(print_style style) {
    print_fits(style);
}

/* @brief dump_heap  prints our the complete status of the heap, all of its blocks, and the sizes
 *                   the blocks occupy. Printing should be clean with no overlap of unique id's
 *                   between heap blocks or corrupted headers.
 */
void dump_heap() {
    header_t *header = heap.client_start;
    printf("Heap client segment starts at address %p, ends %p. %zu total bytes currently used.\n",
            header, heap.client_end, heap.client_size);
    printf("A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n\n");

    printf("%p: FIRST ADDRESS\n", fits.table);
    bool alternate = false;
    for (int i = 0; i < TABLE_SIZE; i++) {
        printf(COLOR_GRN);
        if (i == TABLE_SIZE - 1) {
            printf("[CLASS:%ubytes+]=>", fits.table[i].size);
        } else if (i >= SMALL_TABLE_SIZE) {
            printf("[CLASS:%u-%ubytes]=>", fits.table[i].size, fits.table[i + 1].size - 1U);
        } else {
            printf("[CLASS:%ubytes]=>", fits.table[i].size);
        }
        printf(COLOR_NIL);
        if (alternate) {
            printf(COLOR_RED);
        } else {
            printf(COLOR_CYN);
        }
        alternate = !alternate;
        int total_nodes = 0;
        for (free_node *cur = fits.table[i].start; cur != fits.nil; cur = cur->next) {
            total_nodes++;
        }
        printf("(+%d)\n", total_nodes);
        printf(COLOR_NIL);
    }
    printf("%p: START OF HEAP. HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", heap.client_start);
    header_t *prev = header;
    while (header != heap.client_end) {
        size_t full_size = extract_block_size(*header);

        if (full_size == 0) {
            print_bad_jump(header, prev);
            printf("Last known pointer before jump: %p", prev);
            return;
        }
        if (!is_valid_header(*header, full_size)) {
            print_error_block(header, full_size);
            return;
        }
        if (is_block_allocated(*header)) {
            print_alloc_block(header);
        } else {
            print_free_block(header);
        }
        prev = header;
        header = get_right_header(header, full_size);
    }
    printf("%p: END OF HEAP\n", heap.client_end);
    printf(COLOR_RED);
    printf("<-%pSENTINEL->\n", fits.nil);
    printf(COLOR_NIL);
    printf("%p: LAST ADDRESS\n", (byte_t *)fits.nil + FREE_NODE_WIDTH);
    printf("\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n");

    printf("\nSEGREGATED LIST OF FREE NODES AND BLOCK SIZES.\n");
    printf("HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n");
    print_fits(VERBOSE);
}

