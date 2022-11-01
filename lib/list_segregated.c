/**
 * Author: Alexander G. Lopez
 *
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
#include "allocator.h"

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
static struct fits {
    seg_node *table;
    list_node *nil;
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


/* * * * * * * * *   Static Minor Heap Methods   * * * * * * * * * * */


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

/* @brief splice_list_node  removes a free node out of the free node list.
 * @param *to_splice        the heap node that we are either allocating or splitting.
 * @param *block_size       number of bytes that is used by the block we are splicing.
 */
static void splice_list_node(list_node *to_splice, size_t block_size) {
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

/* @brief init_list_node  initializes the header and footer of a free node, informs the right
 *                        neighbor of its status and ads the node to the explicit free list.
 * @param to_add          the newly freed header for a heap_node to prepare for the free list.
 * @param block_size      the size of free memory that will now be added to the free list.
 */
static void init_list_node(header *to_add, size_t block_size) {
    *to_add = LEFT_ALLOCATED | block_size;
    header *footer = (header*)((byte *)to_add + block_size - ALIGNMENT);
    *footer = *to_add;
    header *neighbor = get_right_header(to_add, block_size);
    *neighbor &= LEFT_FREE;
    list_node *free_add = get_list_node(to_add);

    int index = 0;
    for ( ; index < TABLE_SIZE - 1 && block_size >= fits.table[index + 1].size; index++) {
    }
    // For speed push nodes to front of the list. We are loosely sorted by at most powers of 2.
    list_node *cur = fits.table[index].start;
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
static void *split_alloc(header *free_block, size_t request, size_t block_space) {
    header *neighbor = NULL;
    if (block_space >= request + MIN_BLOCK_SIZE) {
        neighbor = get_right_header(free_block, request);
        // This takes care of the neighbor and ITS neighbor with appropriate updates.
        init_list_node(neighbor, block_space - request);
    } else {
        request = block_space;
        neighbor = get_right_header(free_block, block_space);
        *neighbor |= LEFT_ALLOCATED;
    }
    init_header(free_block, request, ALLOCATED);
    return get_list_node(free_block);
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
static header *coalesce(header *leftmost_header) {
    size_t coalesced_space = extract_block_size(*leftmost_header);
    header *right_space = get_right_header(leftmost_header, coalesced_space);
    if (right_space != heap.client_end && !is_block_allocated(*right_space)) {
        size_t block_size = extract_block_size(*right_space);
        coalesced_space += block_size;
        splice_list_node(get_list_node(right_space), block_size);
    }

    if (is_left_space(leftmost_header)) {
        leftmost_header = get_left_header(leftmost_header);
        size_t block_size = extract_block_size(*leftmost_header);
        coalesced_space += block_size;
        splice_list_node(get_list_node(leftmost_header), block_size);
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
    fits.nil = (list_node*)((byte*)heap_start + (heap.client_size - LIST_NODE_WIDTH));
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


    header *first_block = (header *)((byte *)heap_start + TABLE_BYTES);
    init_header(first_block, heap.client_size - TABLE_BYTES - LIST_NODE_WIDTH, FREE);
    init_footer(first_block, heap.client_size - TABLE_BYTES - LIST_NODE_WIDTH);

    list_node *first_free = (list_node *)((byte *)first_block + ALIGNMENT);
    first_free->next = fits.nil;
    first_free->prev = fits.nil;
    // Insert this first free into the appropriately sized list.
    init_list_node(first_block, heap.client_size - TABLE_BYTES - LIST_NODE_WIDTH);

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

    size_t rounded_request = roundup(requested_size + HEADER_AND_LIST_NODE, ALIGNMENT);
    for (int i = 0; i < TABLE_SIZE; i++) {
        // All lists hold advertised size and up to one byte less than next list.
        if (i == TABLE_SIZE - 1 || rounded_request < fits.table[i + 1].size) {
            for (list_node *node = fits.table[i].start; node != fits.nil; node = node->next) {

                header *header = get_block_header(node);
                size_t free_space = extract_block_size(*header);
                if (free_space >= rounded_request) {
                    splice_list_node(node, free_space);
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
    size_t size_needed = roundup(new_size + HEADER_AND_LIST_NODE, ALIGNMENT);
    header *old_header = get_block_header(old_ptr);
    size_t old_space = extract_block_size(*old_header);

    // Spec requires we coalesce as much as possible even if there were sufficient space in place.
    header *leftmost_header = coalesce(old_header);
    size_t coalesced_total = extract_block_size(*leftmost_header);
    void *client_block = get_list_node(leftmost_header);

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
        init_list_node(leftmost_header, coalesced_total);
    }
    // NULL or the space we found from in-place or malloc.
    return client_block;
}

/* @brief myfree  frees valid user memory from the heap.
 * @param *ptr    a pointer to previously allocated heap memory.
 */
void myfree(void *ptr) {
    if (ptr != NULL) {
        header *to_free = get_block_header(ptr);
        to_free = coalesce(to_free);
        init_list_node(to_free, extract_block_size(*to_free));
    }
}


/* * * * * * * * * * *   Shared Debugging  * * * * * * * * * * * * * */


/* @brief validate_heap  runs various checks to ensure that every block of the heap is well formed
 *                       with valid sizes, alignment, and initializations.
 * @return               true if the heap is valid and false if the heap is invalid.
 */
bool validate_heap() {
    if (!check_seg_list_init(fits.table, fits.nil, heap.client_size)) {
        return false;
    }
    size_t total_free_mem = 0;
    if (!is_list_balanced(&total_free_mem, heap.client_start, heap.client_end, heap.client_size,
                          fits.total)) {
        return false;
    }
    if (!is_seg_list_valid(total_free_mem, fits.table, fits.nil)) {
        return false;
    }
    return true;
}


/* * * * * * * * * * * *   Shared Printing Debugger   * * * * * * * * * * */


/* @brief print_free_nodes  a shared function across allocators requesting a printout of internal
 *                          data structure used for free nodes of the heap.
 * @param style             VERBOSE or PLAIN. Plain only includes byte size, while VERBOSE includes
 *                          memory addresses.
 */
void print_free_nodes(print_style style) {
    print_seg_list(style, fits.table, fits.nil);
}

/* @brief dump_heap  prints our the complete status of the heap, all of its blocks, and the sizes
 *                   the blocks occupy. Printing should be clean with no overlap of unique id's
 *                   between heap blocks or corrupted headers.
 */
void dump_heap() {
    header *header = heap.client_start;
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
        for (list_node *cur = fits.table[i].start; cur != fits.nil; cur = cur->next) {
            total_nodes++;
        }
        printf("(+%d)\n", total_nodes);
        printf(COLOR_NIL);
    }
    printf("%p: START OF HEAP. HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", heap.client_start);
    header *prev = header;
    while (header != heap.client_end) {
        size_t full_size = extract_block_size(*header);

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
    printf("%p: LAST ADDRESS\n", (byte *)fits.nil + LIST_NODE_WIDTH);
    printf("\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n");

    printf("\nSEGREGATED LIST OF FREE NODES AND BLOCK SIZES.\n");
    printf("HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n");
    print_seg_list(VERBOSE, fits.table, fits.nil);
}

