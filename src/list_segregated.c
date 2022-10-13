/*
 * File: list_segregated.c
 * ---------------------
 * This is a basic implementation of a segregated free list heap allocator. We maintain 20 list
 * sizes and do a first fit search on sorted lists, approximating a best fit search of the heap.
 *
 * Citations:
 * -------------------
 * 1. Bryant and O'Hallaron, Computer Systems: A Programmer's Perspective, Chapter 9.
 *    I used the explicit free free_list outline from the textbook, specifically
 *    regarding how to implement left and right coalescing. I even used their suggested
 *    optimization of an extra control bit so that the footers to the left can be overwritten
 *    if the block is allocated so the user can have more space.
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "./allocator.h"
#include "./debug_break.h"

typedef size_t header_t;
typedef unsigned char byte_t;

typedef struct free_node {
    struct free_node *next;
    struct free_node *prev;
}free_node;

/* Size Order Classes Maintained by an Array of doubly linked lists
 *     - Our size classes stand for the maximum size of a node in the list.
 *     - 20 Size Classes: 1,2,3,4,5,6,7,8,16,32,64,128,256,512,1024,2048,4096,8192,16384-infty...
 *     - A first fit search will yeild approximately the best fit.
 */
typedef struct class_node {
    short class_size;
    free_node *list_start;
}class_node;

typedef enum header_status_t {
    FREE = 0x0UL,
    ALLOCATED = 0x1UL,
    LEFT_ALLOCATED = 0x2UL,
    LEFT_FREE = ~0x2UL
} header_status_t;

/* Sacrifice two blocks on the heap for a head and tail to get our instruction count low and have
 * very simple free_list management logic. In total, we lose 32 initial bytes with this method.
 */
static struct free_list {
    free_node *head;
    free_node *tail;
    size_t total;
}free_list;

static struct heap {
    void *client_start;
    void *client_end;
    size_t client_size;
}heap;

#define CLASS_MAX (unsigned short)20
static class_node size_classes[CLASS_MAX];


#define SIZE_MASK ~0x7UL
#define STATUS_CHECK 0x4UL
#define BYTES_PER_LINE (unsigned short)32
#define FREE_NODE_WIDTH (unsigned short)16
#define HEADER_AND_FREE_NODE (unsigned short)24
#define MIN_BLOCK_SIZE (unsigned short)32
#define SMALL_CLASS_MAX (unsigned short)8
#define POWER_2_CLASS_MAX (unsigned short)16384


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

/* @brief splice_free_list  removes a free node out of the free node list.
 * @param *to_splice        the heap node that we are either allocating or splitting.
 */
static void splice_free_list(free_node *to_splice) {
    // Because we have head and tail we don't need to worry about where the first or last node is.
    to_splice->next->prev = to_splice->prev;
    to_splice->prev->next = to_splice->next;
    free_list.total--;
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
    free_node *cur = free_list.head->next;
    // We are maintaining ascending size order, so compare pointers.
    for (; cur != free_list.tail
            && extract_block_size(*get_block_header(cur)) < block_size; cur = cur->next) {
    }
    free_add->next = cur;
    free_add->prev = cur->prev;
    cur->prev->next = free_add;
    cur->prev = free_add;
    free_list.total++;
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
    // Here is the edgecase mentioned in the heap struct. Need to know when to stop right.
    if (right_space != heap.client_end && !is_block_allocated(*right_space)) {
        coalesced_space += extract_block_size(*right_space);
        splice_free_list(get_free_node(right_space));
    }

    if (is_left_space(leftmost_header)) {
        leftmost_header = get_left_header(leftmost_header);
        coalesced_space += extract_block_size(*leftmost_header);
        splice_free_list(get_free_node(leftmost_header));
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
    return free_list.total;
}

/* @brief myinit      initializes the heap space for use for the client.
 * @param *heap_start pointer to the space we will initialize as our heap.
 * @param heap_size   the desired space for the heap memory overall.
 * @return            true if the space if the space is successfully initialized false if not.
 */
bool myinit(void *heap_start, size_t heap_size) {
    heap.client_size = roundup(heap_size, ALIGNMENT);
    if (heap_size < MIN_BLOCK_SIZE) {
        return false;
    }

    // This costs some memory in exchange for ease of use and low instruction counts.
    free_list.head = heap_start;
    free_list.tail = (free_node*)((byte_t*)free_list.head + (heap.client_size - FREE_NODE_WIDTH));
    free_list.head->prev = NULL;
    free_list.tail->next = NULL;

    header_t *first_block = (header_t *)((byte_t *)free_list.head + FREE_NODE_WIDTH);
    init_header(first_block, heap.client_size - (FREE_NODE_WIDTH * 2), FREE);
    init_footer(first_block, heap.client_size - (FREE_NODE_WIDTH * 2));

    // Free nodes are seperate from header so that free_list.head and tail only take 32 heap bytes.
    free_node *first_free = (free_node *)((byte_t *)first_block + ALIGNMENT);
    first_free->next = free_list.tail;
    first_free->prev = free_list.head;

    free_list.head->next = first_free;
    free_list.tail->prev = first_free;
    heap.client_start = first_block;
    heap.client_end = free_list.tail;
    free_list.total = 1;
    return true;
}

/* @brief *mymalloc       finds space for the client from the doubly linked free node list.
 * @param requested_size  the user desired size that we will round up and align.
 * @return                a void pointer to the space ready for the user or NULL if the request
 *                        could not be serviced because it was invalid or there is no space.
 */
void *mymalloc(size_t requested_size) {
    if (requested_size != 0 && requested_size <= MAX_REQUEST_SIZE) {
        size_t rounded_request = roundup(requested_size + HEADER_AND_FREE_NODE, ALIGNMENT);

        for (free_node *node = free_list.head->next; node != free_list.tail; node = node->next) {
            header_t *header = get_block_header(node);
            size_t free_space = extract_block_size(*header);
            if (free_space >= rounded_request) {
                splice_free_list(node);
                // Handoff decision to split or not to our helper, takes care of all details.
                return split_alloc(header, rounded_request, free_space);
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
    // There are some subtleties between these edgcases.
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
    // We also need to make sure the leftmost header always says there is no space to the left.
    if (is_left_space((heap.client_start))) {
        breakpoint();
        return false;
    }
    void *first_address = free_list.head;
    void *last_address = (byte_t *)free_list.tail + FREE_NODE_WIDTH;
    if ((byte_t *)last_address - (byte_t *)first_address != heap.client_size) {
        breakpoint();
        return false;
    }
    // There is one very rare edgecase that may affect the next field of the list tail. This
    // is acceptable because we never use that field and do not need it to remain NULL.
    if (free_list.head->prev != NULL) {
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
    // Some bits are overlapping into our lower three control bits in the headers.
    if (is_header_corrupted(header)) {
        return false;
    }
    // We are not staying divisible by 8
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
    size_t size_used = FREE_NODE_WIDTH * 2;
    size_t total_free_nodes = 0;
    while (cur_header != heap.client_end) {
        size_t block_size_check = extract_block_size(*cur_header);
        if (block_size_check == 0) {
            // Bad jump check the previous node address compared to this one.
            breakpoint();
            return false;
        }

        if (!is_valid_header(*cur_header, block_size_check)) {
            breakpoint();
            return false;
        }
        // Now tally valid size into total.
        if (is_block_allocated(*cur_header)) {
            size_used += block_size_check;
        } else {
            total_free_nodes++;
            *total_free_mem += block_size_check;
        }
        cur_header = get_right_header(cur_header, block_size_check);
    }
    return (size_used + *total_free_mem == heap.client_size)
            && (total_free_nodes == free_list.total);
}


/* @brief is_free_list_valid  loops through only the doubly linked list to make sure it matches
 *                            the loop we just completed by checking all blocks.
 * @param total_free_mem      the input from a previous loop that was completed by jumping block
 *                            by block over the entire heap.
 * @return                    true if the doubly linked list totals correctly, false if not.
 */
static bool is_free_list_valid(size_t total_free_mem) {
    size_t linked_free_mem = 0;
    size_t prev_size = 0;
    for (free_node *cur = free_list.head->next; cur != free_list.tail; cur = cur->next) {
        header_t *cur_header = get_block_header(cur);
        size_t cur_size = extract_block_size(*cur_header);
        // Make sure our ascending address order is valid.
        if (prev_size >  cur_size) {
            return false;
        }
        if (is_block_allocated(*cur_header)) {
            return false;
        }
        // This algorithm does not allow two free blocks to remain next to one another.
        if (is_left_space(get_block_header(cur))) {
            return false;
        }
        linked_free_mem += cur_size;
        prev_size = cur_size;
    }
    return total_free_mem == linked_free_mem;
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
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    size_t total_free_mem = 0;
    if (!is_memory_balanced(&total_free_mem)) {
        breakpoint();
        return false;
    }
    if (!is_free_list_valid(total_free_mem)) {
        breakpoint();
        return false;
    }
    return true;
}


/* * * * * * * * *   Static Printing Helpers   * * * * * * * * * * */


/* @brief print_linked_free  prints the doubly linked free list in order to check if splicing and
 *                           adding is progressing correctly.
 */
static void print_linked_free(print_style style) {
    printf(COLOR_RED);
    printf("[");
    if (style == VERBOSE) {
        printf("%p:", free_list.head);
    }
    printf("(HEAD)]");
    for (free_node *cur = free_list.head->next; cur != free_list.tail; cur = cur->next) {
        if (cur) {
            header_t *cur_header = get_block_header(cur);
            printf("<=>[");
            if (style == VERBOSE) {
                printf("%p:", cur);
            }
            printf("(%zubytes)]", extract_block_size(*cur_header) - ALIGNMENT);
        } else {
            printf("Something went wrong. NULL free free_list node.\n");
            break;
        }
    }
    printf("<=>[");
    if (style == VERBOSE) {
        printf("%p:", free_list.tail);
    }
    printf("(TAIL)]\n");
    printf(COLOR_NIL);
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
    // We should be able to see the header is the same as the footer. If they are not the same
    // we will face subtle bugs that are very hard to notice.
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
    // The doubly linked free_list may be messed up as well.
    print_linked_free(VERBOSE);
}


/* * * * * * * * * * * *   Shared Printing Debugger   * * * * * * * * * * */


/* @brief print_free_nodes  a shared function across allocators requesting a printout of internal
 *                          data structure used for free nodes of the heap.
 * @param style             VERBOSE or PLAIN. Plain only includes byte size, while VERBOSE includes
 *                          memory addresses.
 */
void print_free_nodes(print_style style) {
    print_linked_free(style);
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

    printf("%p: FIRST ADDRESS\n", free_list.head);
    printf(COLOR_RED);
    printf("%p: NULL<-DUMMY HEAD NODE->%p\n", free_list.head, free_list.head->next);
    printf(COLOR_NIL);
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
    printf("%p: %p<-DUMMY TAIL NODE->NULL\n", free_list.tail, free_list.tail->prev);
    printf(COLOR_NIL);
    printf("%p: LAST ADDRESS\n", (byte_t *)free_list.tail + FREE_NODE_WIDTH);
    printf("\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n");

    printf("\nDOUBLY LINKED LIST OF FREE NODES AND BLOCK SIZES.\n");
    printf("HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n");
    print_linked_free(VERBOSE);
}

