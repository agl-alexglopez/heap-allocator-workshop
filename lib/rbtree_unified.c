/**
 * Author: Alexander Griffin Lopez
 *
 * File: rbtree_unified.c
 * ---------------------
 *  This file contains my implementation of an explicit heap allocator. This allocator uses a tree
 *  implementation to track the free space in the heap, relying on the properties of a red-black
 *  tree to remain balanced.
 *
 *  Citations:
 *  -------------------
 *  1. Bryant and O'Hallaron, Computer Systems: A Programmer's Perspective, Chapter 9.
 *     I used the explicit free list outline from the textbook, specifically
 *     regarding how to implement left and right coalescing. I even used their suggested
 *     optimization of an extra control bit so that the footers to the left can be overwritten
 *     if the block is allocated so the user can have more space.
 *
 *  2. The text Introduction to Algorithms, by Cormen, Leiserson, Rivest, and Stein was central to
 *     my implementation of the red black tree portion of my allocator. Specifically, I took the
 *     the implementation from chapter 13. The placeholder black null node that always sits at the
 *     bottom of the tree proved useful for simplicity.
 *
 *  3. I took much of the ideas for the pretty printing of the tree and the checks for a valid tree
 *     from Seth Furman's red black tree implementation. Specifically, the tree print structure and
 *     colors came from Furman's implementation. https://github.com/sfurman3/red-black-tree-c
 *
 *  4. I took my function to verify black node paths of a red black tree from kraskevich on
 *     stackoverflow in the following answer:
 *          https://stackoverflow.com/questions/27731072/check-whether-a-tree-satisfies-the-black-height-property-of-red-black-tree
 *
 * The header stays as the first field of the rb_node and must remain accessible at all times.
 * The size of the block is a multiple of eight to leave the bottom three bits accessible for info.
 *
 *
 *   v--Most Significnat Bit        v--Least Significnat Bit
 *   0...00000    0         0       0
 *   +--------+--------+--------+--------+--------+--------+--------+--------+--------+
 *   |        |        |        |        |        |        |        |        |        |
 *   |        |red     |left    |free    |        |        |        |        |        |
 *   |size_t  |or      |neighbor|or      |*parent |links[L]|links[R]|...     |footer  |
 *   |bytes   |black   |status  |alloc   |        |        |        |        |        |
 *   |        |        |        |        |        |        |        |        |        |
 *   +--------+--------+--------+--------+--------+--------+--------+--------+--------+
 *   |___________________________________|____________________________________________|
 *                     |                                     |
 *               64-bit header              space available for user if allocated
 *
 *
 * The rest of the rb_node remains accessible for the user, even the footer. We only need the
 * information in the rest of the struct when it is free and either in our tree.
 */
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "allocator.h"
#include "rbtree_unified_utility.h"
#include "rbtree_unified_tests.h"
#include "rbtree_unified_printer.h"


/* * * * * * * * * * * * * * * * * *  Static Heap Tracking  * * * * * * * * * * * * * * * * * * */


static struct tree {
    rb_node *root;
    // All leaves point to the black_nil. Root parent is also black_nil.
    rb_node *black_nil;
    size_t total;
}tree;

static struct heap {
    void *client_start;
    void *client_end;
    size_t heap_size;
}heap;


/* * * * * * * * * * * * * * * * *   Static Helper Functions   * * * * * * * * * * * * * * * * * */


/* @brief rotate     a unified version of the traditional left and right rotation functions. The
 *                   rotation is either left or right and opposite is its opposite direction. We
 *                   take the current nodes child, and swap them and their arbitrary subtrees are
 *                   re-linked correctly depending on the direction of the rotation.
 * @param *current   the node around which we will rotate.
 * @param rotation   either left or right. Determines the rotation and its opposite direction.
 */
static void rotate(rb_node *current, tree_link rotation) {
    rb_node *child = current->links[!rotation];
    current->links[!rotation] = child->links[rotation];
    if (child->links[rotation] != tree.black_nil) {
        child->links[rotation]->parent = current;
    }
    child->parent = current->parent;
    if (current->parent == tree.black_nil) {
        tree.root = child;
    } else {
        // True == 1 == R, otherwise False == 0 == L
        current->parent->links[current->parent->links[R] == current] = child;
    }
    child->links[rotation] = current;
    current->parent = child;
}


/* * * * * * * * * * * *     Static Red-Black Tree Insertion Logic     * * * * * * * * * * * * * */


/* @brief fix_rb_insert  implements a modified Cormen et.al. red black fixup after the insertion of
 *                       a new node. Unifies the symmetric left and right cases with the use of
 *                       an array and an enum tree_link.
 * @param *current       the current node that has just been added to the red black tree.
 */
static void fix_rb_insert(rb_node *current) {
    while(get_color(current->parent->header) == RED) {
        // Store the link from ancestor to parent. True == 1 == R, otherwise False == 0 == L
        tree_link symmetric_case = current->parent->parent->links[R] == current->parent;
        rb_node *aunt = current->parent->parent->links[!symmetric_case];
        if (get_color(aunt->header) == RED) {
            paint_node(aunt, BLACK);
            paint_node(current->parent, BLACK);
            paint_node(current->parent->parent, RED);
            current = current->parent->parent;
        } else {
            if (current == current->parent->links[!symmetric_case]) {
                current = current->parent;
                rotate(current, symmetric_case);
            }
            paint_node(current->parent, BLACK);
            paint_node(current->parent->parent, RED);
            rotate(current->parent->parent, !symmetric_case);
        }
    }
    paint_node(tree.root, BLACK);
}


/* @brief insert_rb_node  a standard binary insert with additional red black fixup logic.
 * @param *current        we must insert to tree or add to a list as duplicate.
 */
static void insert_rb_node(rb_node *current) {
    rb_node *seeker = tree.root;
    rb_node *parent = tree.black_nil;
    size_t current_key = get_size(current->header);
    size_t parent_size = 0;
    while (seeker != tree.black_nil) {
        parent = seeker;
        parent_size = get_size(seeker->header);
        // You may see this idiom throughout. L(0) if key fits in tree to left, R(1) if not.
        seeker = seeker->links[parent_size < current_key];
    }
    current->parent = parent;
    if (parent == tree.black_nil) {
        tree.root = current;
    } else {
        parent->links[parent_size < current_key] = current;
    }
    current->links[L] = tree.black_nil;
    current->links[R] = tree.black_nil;
    paint_node(current, RED);
    fix_rb_insert(current);
    tree.total++;
}


/* * * * * * * * * * * *     Static Red-Black Tree Deletion Helper     * * * * * * * * * * * * * */


/* @brief rb_transplant  replaces node with the appropriate node to start balancing the tree.
 * @param *remove        the node we are removing from the tree.
 * @param *replacement   the node that will fill the remove position. It can be tree.black_nil.
 */
static void rb_transplant(const rb_node *remove, rb_node *replacement) {
    if (remove->parent == tree.black_nil) {
        tree.root = replacement;
    } else {
        // Store the link from parent to child. True == 1 == R, otherwise False == 0 == L
        remove->parent->links[remove->parent->links[R] == remove] = replacement;
    }
    replacement->parent = remove->parent;
}


/* * * * * * * * * * * *     Static Red-Black Tree Deletion Logic      * * * * * * * * * * * * * */


/* @brief fix_rb_delete  completes a unified Cormen et.al. fixup function. Uses a direction enum
 *                       and an array to help unify code paths based on direction and opposites.
 * @param *extra_black   the current node that was moved into place from the previous delete. We
 *                       have given it an "extra black" it must get rid of or use to fix the tree.
 */
static void fix_rb_delete(rb_node *extra_black) {
    while (extra_black != tree.root && get_color(extra_black->header) == BLACK) {

        // We can cover left and right cases in one with simple directional link and its opposite.
        tree_link symmetric_case = extra_black->parent->links[R] == extra_black;

        rb_node *sibling = extra_black->parent->links[!symmetric_case];
        if (get_color(sibling->header) == RED) {
            paint_node(sibling, BLACK);
            paint_node(extra_black->parent, RED);
            rotate(extra_black->parent, symmetric_case);
            sibling = extra_black->parent->links[!symmetric_case];
        }
        if (get_color(sibling->links[L]->header) == BLACK
                && get_color(sibling->links[R]->header) == BLACK) {
            paint_node(sibling, RED);
            extra_black = extra_black->parent;
        } else {
            if (get_color(sibling->links[!symmetric_case]->header) == BLACK) {
                paint_node(sibling->links[symmetric_case], BLACK);
                paint_node(sibling, RED);
                rotate(sibling, !symmetric_case);
                sibling = extra_black->parent->links[!symmetric_case];
            }
            paint_node(sibling, get_color(extra_black->parent->header));
            paint_node(extra_black->parent, BLACK);
            paint_node(sibling->links[!symmetric_case], BLACK);
            rotate(extra_black->parent, symmetric_case);
            extra_black = tree.root;
        }
    }
    paint_node(extra_black, BLACK);
}

/* @brief delete_rb_node  performs the necessary steps to have a functional, balanced tree after
 *                        deletion of any node in the tree.
 * @param *remove         the node to remove from the tree from a call to malloc or coalesce.
 */
static rb_node *delete_rb_node(rb_node *remove) {
    rb_color fixup_color_check = get_color(remove->header);

    rb_node *extra_black = NULL;
    if (remove->links[L] == tree.black_nil || remove->links[R] == tree.black_nil) {
        tree_link nil_link = remove->links[L] != tree.black_nil;
        rb_transplant(remove, (extra_black = remove->links[!nil_link]));
    } else {
        rb_node *right_min = get_min(remove->links[R], tree.black_nil);
        fixup_color_check = get_color(right_min->header);
        extra_black = right_min->links[R];
        if (right_min != remove->links[R]) {
            rb_transplant(right_min, right_min->links[R]);
            right_min->links[R] = remove->links[R];
            right_min->links[R]->parent = right_min;
        } else {
            extra_black->parent = right_min;
        }
        rb_transplant(remove, right_min);
        right_min->links[L] = remove->links[L];
        right_min->links[L]->parent = right_min;
        paint_node(right_min, get_color(remove->header));
    }
    // Nodes can only be red or black, so we need to get rid of "extra" black by fixing tree.
    if (fixup_color_check == BLACK) {
        fix_rb_delete(extra_black);
    }
    tree.total--;
    return remove;
}

/* @brief find_best_fit  a red black tree is well suited to best fit search in O(logN) time. We
 *                       will find the best fitting node possible given the options in our tree.
 * @param key            the size number of bytes we are searching for in our tree.
 * @return               the pointer to the rb_node that is the best fit for our need.
 */
static rb_node *find_best_fit(size_t key) {
    rb_node *seeker = tree.root;
    // We will use this sentinel to start our competition while we search for best fit.
    size_t best_fit_size = ULLONG_MAX;
    rb_node *remove = seeker;
    while (seeker != tree.black_nil) {
        size_t seeker_size = get_size(seeker->header);
        if (key == seeker_size) {
            remove = seeker;
            break;
        }
        tree_link search_direction = seeker_size < key;
        /* The key is less than the current found size but let's remember this size on the way down
         * as a candidate for the best fit. The closest fit will have won when we reach the bottom.
         */
        if (search_direction == L && seeker_size < best_fit_size) {
            remove = seeker;
            best_fit_size = seeker_size;
        }
        seeker = seeker->links[search_direction];
    }
    return delete_rb_node(remove);
}


/* * * * * * * * * * * * * * *    Static Heap Helper Functions     * * * * * * * * * * * * * * * */


/* @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
 * @param *to_free        the heap_node to add to the red black tree.
 * @param block_size      the size we use to initialize the node and find the right place in tree.
 */
static void init_free_node(rb_node *to_free, size_t block_size) {
    to_free->header = block_size | LEFT_ALLOCATED | RED_PAINT;
    init_footer(to_free, block_size);
    get_right_neighbor(to_free, block_size)->header &= LEFT_FREE;
    insert_rb_node(to_free);
}

/* @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
 *                      split, it will add the newly freed split block to the free red black tree.
 * @param *free_block   a pointer to the node for a free block in its entirety.
 * @param request       the user request for space.
 * @param block_space   the entire space that we have to work with.
 * @return              a void pointer to generic space that is now ready for the client.
 */
static void *split_alloc(rb_node *free_block, size_t request, size_t block_space) {
    rb_node *neighbor = NULL;
    if (block_space >= request + MIN_BLOCK_SIZE) {
        neighbor = get_right_neighbor(free_block, request);
        // This takes care of the neighbor and ITS neighbor with appropriate updates.
        init_free_node(neighbor, block_space - request - HEADERSIZE);
    } else {
        request = block_space;
        neighbor = get_right_neighbor(free_block, block_space);
        neighbor->header |= LEFT_ALLOCATED;
    }
    init_header_size(free_block, request);
    free_block->header |= ALLOCATED;
    return get_client_space(free_block);
}

/* @brief coalesce        attempts to coalesce left and right if the left and right rb_node
 *                        are free. Runs the search to free the specific free node in O(logN) + d
 *                        where d is the number of duplicate nodes of the same size.
 * @param *leftmost_node  the current node that will move left if left is free to coalesce.
 * @return                the leftmost node from attempts to coalesce left and right. The leftmost
 *                        node is initialized to reflect the correct size for the space it now has.
 * @warning               this function does not overwrite the data that may be in the middle if we
 *                        expand left and write. The user may wish to move elsewhere if reallocing.
 */
static rb_node *coalesce(rb_node *leftmost_node) {
    size_t coalesced_space = get_size(leftmost_node->header);
    rb_node *rightmost_node = get_right_neighbor(leftmost_node, coalesced_space);

    // The black_nil is the right boundary. We set it to always be allocated with size 0.
    if (!is_block_allocated(rightmost_node->header)) {
        coalesced_space += get_size(rightmost_node->header) + HEADERSIZE;
        rightmost_node = delete_rb_node(rightmost_node);
    }
    // We use our static struct for convenience here to tell where our segment start is.
    if (leftmost_node != heap.client_start && is_left_space(leftmost_node)) {
        leftmost_node = get_left_neighbor(leftmost_node);
        coalesced_space += get_size(leftmost_node->header) + HEADERSIZE;
        leftmost_node = delete_rb_node(leftmost_node);
    }

    // Do not initialize footer here because we may coalesce during realloc. Preserve user data.
    init_header_size(leftmost_node, coalesced_space);
    return leftmost_node;
}


/* * * * * * * * * * * * * * * * *   Shared Heap Functions   * * * * * * * * * * * * * * * * * * */


/* @brief roundup         rounds up size to the nearest multiple of two to be aligned in the heap.
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
    return tree.total;
}

/* @brief myinit      initializes the heap space for use for the client.
 * @param *heap_start pointer to the space we will initialize as our heap.
 * @param heap_size   the desired space for the heap memory overall.
 * @return            true if the space if the space is successfully initialized false if not.
 */
bool myinit(void *heap_start, size_t heap_size) {
    // Initialize the root of the tree and heap addresses.
    size_t client_request = roundup(heap_size, ALIGNMENT);
    if (client_request < MIN_BLOCK_SIZE) {
        return false;
    }
    heap.client_start = heap_start;
    heap.heap_size = client_request;
    heap.client_end = (byte *)heap.client_start + heap.heap_size - HEAP_NODE_WIDTH;
    // Set up the dummy base of the tree to which all leaves will point.
    tree.black_nil = heap.client_end;
    tree.black_nil->header = 1UL;
    tree.black_nil->parent = tree.black_nil->links[L] = tree.black_nil->links[R] = NULL;
    paint_node(tree.black_nil, BLACK);
    // Set up the root of the tree (top) that starts as our largest free block.
    tree.root = heap.client_start;
    init_header_size(tree.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE);
    paint_node(tree.root, BLACK);
    init_footer(tree.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE);
    tree.root->parent = tree.black_nil;
    tree.root->links[L] = tree.black_nil;
    tree.root->links[R] = tree.black_nil;
    tree.total = 1;
    return true;
}

/* @brief *mymalloc       finds space for the client from the red black tree.
 * @param requested_size  the user desired size that we will round up and align.
 * @return                a void pointer to the space ready for the user or NULL if the request
 *                        could not be serviced because it was invalid or there is no space.
 */
void *mymalloc(size_t requested_size) {
    if (requested_size != 0 && requested_size <= MAX_REQUEST_SIZE) {
        size_t client_request = roundup(requested_size + HEAP_NODE_WIDTH, ALIGNMENT);
        // Search the tree for the best possible fitting node.
        rb_node *found_node = find_best_fit(client_request);
        return split_alloc(found_node, client_request, get_size(found_node->header));
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
    size_t request = roundup(new_size + HEAP_NODE_WIDTH, ALIGNMENT);
    rb_node *old_node = get_rb_node(old_ptr);
    size_t old_size = get_size(old_node->header);

    rb_node *leftmost_node = coalesce(old_node);
    size_t coalesced_space = get_size(leftmost_node->header);
    void *client_space = get_client_space(leftmost_node);

    if (coalesced_space >= request) {
        // A simple memmove is better than giving up on the space to the left to search for more.
        if (leftmost_node != old_node) {
            memmove(client_space, old_ptr, old_size);
        }
        client_space = split_alloc(leftmost_node, request, coalesced_space);
    } else if ((client_space = mymalloc(request))) {
        memcpy(client_space, old_ptr, old_size);
        init_free_node(leftmost_node, coalesced_space);
    }
    return client_space;
}

/* @brief myfree  frees valid user memory from the heap.
 * @param *ptr    a pointer to previously allocated heap memory.
 */
void myfree(void *ptr) {
    if (ptr != NULL) {
        rb_node *to_insert = get_rb_node(ptr);
        to_insert = coalesce(to_insert);
        init_free_node(to_insert, get_size(to_insert->header));
    }
}


/* * * * * * * * * * * * * * * * *     Shared Debugger       * * * * * * * * * * * * * * * * * * */


/* @brief validate_heap  runs various checks to ensure that every block of the heap is well formed
 *                       with valid sizes, alignment, and initializations.
 * @return               true if the heap is valid and false if the heap is invalid.
 */
bool validate_heap() {
    if (!check_init(heap.client_start, heap.client_end, heap.heap_size)) {
        return false;
    }
    size_t total_free_mem = 0;
    if (!is_memory_balanced(&total_free_mem, heap.client_start, heap.client_end,
                            heap.heap_size, tree.total)) {
        return false;
    }
    // Does a tree search for all memory match the linear heap search for totals?
    if (!is_rbtree_mem_valid(tree.root, tree.black_nil, total_free_mem)) {
        return false;
    }
    // Two red nodes in a row are invalid for the tree.
    if (is_red_red(tree.root, tree.black_nil)) {
        return false;
    }
    // Does every path from a node to the black sentinel contain the same number of black nodes.
    if (!is_bheight_valid(tree.root, tree.black_nil)) {
        return false;
    }
    // Check that the parents and children are updated correctly if duplicates are deleted.
    if (!is_parent_valid(tree.root, tree.black_nil)) {
        return false;
    }
    // This comes from a more official write up on red black trees so I included it.
    if (!is_bheight_valid_V2(tree.root, tree.black_nil)) {
        return false;
    }
    if (!is_binary_tree(tree.root, tree.black_nil)) {
        return false;
    }
    return true;
}


/* * * * * * * * * * * * * * * * *     Shared Printer        * * * * * * * * * * * * * * * * * * */


/* @brief print_free_nodes  a shared function across allocators requesting a printout of internal
 *                          data structure used for free nodes of the heap.
 * @param style             VERBOSE or PLAIN. Plain only includes byte size, while VERBOSE includes
 *                          memory addresses and black heights of the tree.
 */
void print_free_nodes(print_style style) {
    print_rb_tree(tree.root, tree.black_nil, style);
}

/* @brief dump_heap  prints out the complete status of the heap, all of its blocks, and the sizes
 *                   the blocks occupy. Printing should be clean with no overlap of unique id's
 *                   between heap blocks or corrupted headers.
 */
void dump_heap() {
    print_all(heap.client_start, heap.client_end, heap.heap_size, tree.root, tree.black_nil);
}

