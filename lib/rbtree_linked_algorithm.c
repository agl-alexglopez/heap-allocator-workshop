/**
 * Author: Alexander Griffin Lopez
 *
 * File: rbtree_linked.c
 * ---------------------
 *  This file contains my implementation of an explicit heap allocator. This allocator uses a tree
 *  implementation to track the free space in the heap, relying on the properties of a red-black
 *  tree to remain balanced. This implementation also uses some interesting strategies to unify
 *  left and right cases for a red black tree and maintains a doubly linked list of duplicate nodes
 *  of the same size if a node in the tree has repeats of its size.
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
 *     bottom of the tree proved useful for the tree and the linked list of duplicate memory block
 *     sizes.
 *
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "allocator.h"
#include "rbtree_linked_utilities.h"


/* * * * * * * * * * * * * * * * * *  Static Heap Tracking  * * * * * * * * * * * * * * * * * * */


/* Red Black Free Tree:
 *  - Maintain a red black tree of free nodes.
 *  - Root is black
 *  - No red node has a red child
 *  - New insertions are red
 *  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
 *  - Every path from root to black_nil, root not included, has same number of black nodes.
 *  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
 *  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status for coalescing.
 *  - Use a *list_start pointer to a doubly linked list of duplicate nodes of the same size.
 *  - For more details on the types see the _utilities.h file.
 */
static struct free_nodes {
    rb_node *tree_root;
    rb_node *black_nil;
    duplicate_node *list_tail;
    size_t total;
}free_nodes;

// Mainly for debugging and verifying our heap.
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

    if (child->links[rotation] != free_nodes.black_nil) {
        child->links[rotation]->parent = current;
    }
    child->parent = current->parent;
    if (current->parent == free_nodes.black_nil) {
        free_nodes.tree_root = child;
    } else {
        // True == 1 == R, otherwise False == 0 == L
        current->parent->links[ current->parent->links[R] == current ] = child;
    }
    child->links[rotation] = current;
    current->parent = child;
}


/* * * * * * * * * * *     Static Red-Black Tree Insertion Helper Function   * * * * * * * * * * */


/* @brief add_duplicate  this implementation stores duplicate nodes in a linked list to prevent the
 *                       rotation of duplicates in the tree. This adds the duplicate node to the
 *                       linked list of the node already present.
 * @param *head          the node currently organized in the tree. We will add to its list.
 * @param *to_add        the node to add to the linked list.
 */
static void add_duplicate(rb_node *head, duplicate_node *to_add) {
    to_add->header = head->header;
    // These fields should not matter but we should initialize them to be safe.
    to_add->parent = NULL;
    to_add->list_start = NULL;
    // Get the first next node in the doubly linked list, invariant and correct its left field.
    head->list_start->links[P] = to_add;
    to_add->links[N] = head->list_start;
    to_add->links[P] = (duplicate_node *)head;
    head->list_start = to_add;
    free_nodes.total++;
}


/* * * * * * * * * * * *      Static Red-Black Tree Insertion Logic      * * * * * * * * * * * * */


/* @brief fix_rb_insert  implements a modified Cormen et.al. red black fixup after the insertion of
 *                       a new node. Unifies the symmetric left and right cases with the use of
 *                       an array and an enum tree_link.
 * @param *current       the current node that has just been added to the red black tree.
 */
static void fix_rb_insert(rb_node *current) {
    while(get_color(current->parent->header) == RED) {
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
    paint_node(free_nodes.tree_root, BLACK);
}

/* @brief insert_rb_node  a modified insertion with additional logic to add duplicates if the
 *                        size in bytes of the block is already in the tree.
 * @param *current        we must insert to tree or add to a list as duplicate.
 */
static void insert_rb_node(rb_node *current) {
    rb_node *seeker = free_nodes.tree_root;
    rb_node *parent = free_nodes.black_nil;
    size_t current_key = get_size(current->header);
    while (seeker != free_nodes.black_nil) {
        parent = seeker;
        size_t seeker_size = get_size(seeker->header);

        // Duplicates with a linked list. No duplicates in tree while staying O(1) coalescing.
        if (current_key == seeker_size) {
            add_duplicate(seeker, (duplicate_node *)current);
            return;
        }
        // You may see this idiom throughout. L(0) if key fits in tree to left, R(1) if not.
        seeker = seeker->links[seeker_size < current_key];
    }
    current->parent = parent;
    if (parent == free_nodes.black_nil) {
        free_nodes.tree_root = current;
    } else {
        parent->links[ get_size(parent->header) < current_key ] = current;
    }
    current->links[L] = current->links[R] = free_nodes.black_nil;
    // Every node in the tree is a dummy head for a doubly linked list of duplicates.
    current->list_start = free_nodes.list_tail;
    paint_node(current, RED);
    fix_rb_insert(current);
    free_nodes.total++;
}


/* * * * * * * * * *     Static Red-Black Tree Deletion Helper Functions   * * * * * * * * * * * */


/* @brief rb_transplant  replaces node with the appropriate node to start balancing the tree.
 * @param *remove        the node we are removing from the tree.
 * @param *replacement   the node that will fill the remove position. It can be black_nil.
 */
static void rb_transplant(const rb_node *remove, rb_node *replacement) {
    if (remove->parent == free_nodes.black_nil) {
        free_nodes.tree_root = replacement;
    } else {
        remove->parent->links[ remove->parent->links[R] == remove ] = replacement;
    }
    replacement->parent = remove->parent;
}

/* @brief delete_duplicate  will remove a duplicate node from the tree when the request is coming
 *                          from a call from malloc. Address of duplicate does not matter so we
 *                          remove the first node from the linked list.
 * @param *head             We know this node has a next node and it must be removed for malloc.
 */
static rb_node *delete_duplicate(rb_node *head) {
    duplicate_node *next_node = head->list_start;
    next_node->links[N]->links[P] = (duplicate_node *)head;
    head->list_start = next_node->links[N];
    free_nodes.total--;
    return (rb_node *)next_node;
}


/* * * * * * * * * * * * *      Static Red-Black Tree Deletion Logic     * * * * * * * * * * * * */


/* @brief fix_rb_delete  completes a unified Cormen et.al. fixup function. Uses a direction enum
 *                       and an array to help unify code paths based on direction and opposites.
 * @param *extra_black   the extra_black node that was moved into place from the previous delete.
 *                       May have broken rules of the tree or thrown off balance.
 */
static void fix_rb_delete(rb_node *extra_black) {
    // The extra_black is "doubly black" if we enter the loop, requiring repairs.
    while (extra_black != free_nodes.tree_root && get_color(extra_black->header) == BLACK) {

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
            extra_black = free_nodes.tree_root;
        }
    }
    // The extra_black has reached a red node, making it "red-and-black", or the root. Paint BLACK.
    paint_node(extra_black, BLACK);
}

/* @brief delete_rb_node  performs the necessary steps to have a functional, balanced tree after
 *                        deletion of any node in the tree.
 * @param *remove         the node to remove from the tree from a call to malloc or coalesce.
 */
static rb_node *delete_rb_node(rb_node *remove) {
    rb_color fixup_color_check = get_color(remove->header);

    rb_node *extra_black = NULL;
    if (remove->links[L] == free_nodes.black_nil || remove->links[R] == free_nodes.black_nil) {
        tree_link nil_link = remove->links[L] != free_nodes.black_nil;
        rb_transplant(remove, extra_black = remove->links[!nil_link]);
    } else {
        rb_node *replacement = get_min(remove->links[R], free_nodes.black_nil);
        fixup_color_check = get_color(replacement->header);
        extra_black = replacement->links[R];
        if (replacement != remove->links[R]) {
            rb_transplant(replacement, extra_black);
            replacement->links[R] = remove->links[R];
            replacement->links[R]->parent = replacement;
        } else {
            extra_black->parent = replacement;
        }
        rb_transplant(remove, replacement);
        replacement->links[L] = remove->links[L];
        replacement->links[L]->parent = replacement;
        paint_node(replacement, get_color(remove->header));
    }
    if (fixup_color_check == BLACK) {
        fix_rb_delete(extra_black);
    }
    free_nodes.total--;
    return remove;
}

/* @brief find_best_fit  a red black tree is well suited to best fit search in O(logN) time. We
 *                       will find the best fitting node possible given the options in our tree.
 * @param key            the size_t number of bytes we are searching for in our tree.
 * @return               the pointer to the rb_node that is the best fit for our need.
 */
static rb_node *find_best_fit(size_t key) {
    rb_node *seeker = free_nodes.tree_root;
    size_t best_fit_size = ULLONG_MAX;
    rb_node *remove = seeker;
    while (seeker != free_nodes.black_nil) {
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

    if (remove->list_start != free_nodes.list_tail) {
        // We will keep remove in the tree and just get the first node in doubly linked list.
        return delete_duplicate(remove);
    }
    return delete_rb_node(remove);
}

/* @brief remove_head  frees the head of a doubly linked list of duplicates which is a node in a
 *                     red black tree. The next duplicate must become the new tree node and the
 *                     parent and children must be adjusted to track this new node.
 * @param head         the node in the tree that must now be coalesced.
 */
static void remove_head(rb_node *head) {
    rb_node *new_head = (rb_node *)head->list_start;
    new_head->header = head->header;
    // Make sure we set up new start of list correctly for linked list.
    new_head->list_start = head->list_start->links[N];

    // Now transition to thinking about this new_head as a node in a tree, not a list.
    new_head->links[L] = head->links[L];
    new_head->links[R] = head->links[R];
    head->links[L]->parent = new_head;
    head->links[R]->parent = new_head;
    new_head->parent = head->parent;
    if (head->parent == free_nodes.black_nil) {
        free_nodes.tree_root = new_head;
    } else {
        head->parent->links[head->parent->links[R] == head] = new_head;
    }
}

/* @brief free_coalesced_node  a specialized version of node freeing when we find a neighbor we
 *                             need to free from the tree before absorbing into our coalescing. If
 *                             this node is a duplicate we can splice it from a linked list.
 * @param *to_coalesce         the node we now must find by address in the tree.
 * @return                     the node we have now correctly freed given all cases to find it.
 */
static rb_node *free_coalesced_node(void *to_coalesce) {
    rb_node *tree_node = to_coalesce;
    // Quick return if we just have a standard deletion.
    if (tree_node->list_start == free_nodes.list_tail) {
       return delete_rb_node(tree_node);
    }

    duplicate_node *list_node = to_coalesce;
    // to_coalesce is the head of a doubly linked list. Remove and make a new head.
    if (tree_node->parent) {
        remove_head(tree_node);

    // to_coalesce is next after the head and needs special attention due to list_start field.
    } else if (list_node->links[P]->list_start == to_coalesce){
        tree_node = (rb_node *)list_node->links[P];
        tree_node->list_start = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

    // Finally the simple invariant case of the node being in middle or end of list.
    } else {
        list_node->links[P]->links[N] = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];
    }
    free_nodes.total--;
    return to_coalesce;
}


/* * * * * * * * * * * * * *    Static Heap Helper Functions     * * * * * * * * * * * * * * * * */


/* @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
 * @param *to_free        the heap_node to add to the red black tree.
 * @param block_size      the size we use to initialize the node and find the right place in tree.
 */
static void init_free_node(rb_node *to_free, size_t block_size) {
    to_free->header = block_size | LEFT_ALLOCATED | RED_PAINT;
    to_free->list_start = free_nodes.list_tail;
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
rb_node *coalesce(rb_node *leftmost_node) {
    size_t coalesced_space = get_size(leftmost_node->header);
    rb_node *rightmost_node = get_right_neighbor(leftmost_node, coalesced_space);

    // The black_nil is the right boundary. We set it to always be allocated with size 0.
    if (!is_block_allocated(rightmost_node->header)) {
        coalesced_space += get_size(rightmost_node->header) + HEADERSIZE;
        rightmost_node = free_coalesced_node(rightmost_node);
    }
    // We use our static struct for convenience here to tell where our segment start is.
    if (leftmost_node != heap.client_start && is_left_space(leftmost_node)) {
        leftmost_node = get_left_neighbor(leftmost_node);
        coalesced_space += get_size(leftmost_node->header) + HEADERSIZE;
        leftmost_node = free_coalesced_node(leftmost_node);
    }

    // Do not initialize footer here because we may coalesce during realloc. Preserve user data.
    init_header_size(leftmost_node, coalesced_space);
    return leftmost_node;
}


/* * * * * * * * * * * * * *          Shared Heap Functions        * * * * * * * * * * * * * * * */


/* @brief get_free_total  returns the total number of free nodes in the heap.
 * @return                a size_t representing the total quantity of free nodes.
 */
size_t get_free_total() {
    return free_nodes.total;
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
    free_nodes.black_nil = heap.client_end;
    free_nodes.list_tail = heap.client_end;
    free_nodes.black_nil->header = 1UL;
    paint_node(free_nodes.black_nil, BLACK);

    // Set up the root of the tree (top) that starts as our largest free block.
    free_nodes.tree_root = heap.client_start;
    init_header_size(free_nodes.tree_root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE);
    paint_node(free_nodes.tree_root, BLACK);
    init_footer(free_nodes.tree_root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE);
    free_nodes.tree_root->parent = free_nodes.black_nil;
    free_nodes.tree_root->links[L] = free_nodes.black_nil;
    free_nodes.tree_root->links[R] = free_nodes.black_nil;
    free_nodes.tree_root->list_start = free_nodes.list_tail;
    free_nodes.total = 1;
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


/* * * * * * * * * * * * * * *       Shared Debugging        * * * * * * * * * * * * * * * * * * */


/* @brief validate_heap  runs various checks to ensure that every block of the heap is well formed
 *                       with valid sizes, alignment, and initializations.
 * @return               true if the heap is valid and false if the heap is invalid.
 */
bool validate_heap() {
    if (!check_init(heap.client_start, heap.client_end, heap.heap_size)) {
        return false;
    }
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    size_t total_free_mem = 0;
    if (!is_memory_balanced(&total_free_mem, heap.client_start, heap.client_end,
                            heap.heap_size, free_nodes.total)) {
        return false;
    }
    // Does a tree search for all memory match the linear heap search for totals?
    if (!is_rbtree_mem_valid(free_nodes.tree_root, free_nodes.black_nil, total_free_mem)) {
        return false;
    }
    // Two red nodes in a row are invalid for the table.
    if (is_red_red(free_nodes.tree_root, free_nodes.black_nil)) {
        return false;
    }
    // Does every path from a node to the black sentinel contain the same number of black nodes.
    if (!is_bheight_valid(free_nodes.tree_root, free_nodes.black_nil)) {
        return false;
    }
    // This comes from a more official write up on red black trees so I included it.
    if (!is_bheight_valid_V2(free_nodes.tree_root, free_nodes.black_nil)) {
        return false;
    }
    // Check that the parents and children are updated correctly if duplicates are deleted.
    if (!is_parent_valid(free_nodes.tree_root, free_nodes.black_nil)) {
        return false;
    }
    if (!is_binary_tree(free_nodes.tree_root, free_nodes.black_nil)) {
        return false;
    }
    return true;
}


/* * * * * * * * * * * * * * * *         Shared Printer            * * * * * * * * * * * * * * * */


/* @brief print_free_nodes  a shared function across allocators requesting a printout of internal
 *                          data structure used for free nodes of the heap.
 * @param style             VERBOSE or PLAIN. Plain only includes byte size, while VERBOSE includes
 *                          memory addresses and black heights of the tree.
 */
void print_free_nodes(print_style style) {
    printf(COLOR_CYN "(+X)" COLOR_NIL);
    printf(" Indicates duplicate nodes in the tree linked by a doubly-linked list.\n");
    print_rb_tree(free_nodes.tree_root, free_nodes.black_nil, style);
}

/* @brief dump_heap  prints out the complete status of the heap, all of its blocks, and the sizes
 *                   the blocks occupy. Printing should be clean with no overlap of unique id's
 *                   between heap blocks or corrupted headers.
 */
void dump_heap() {
    print_all(heap.client_start, heap.client_end, heap.heap_size,
              free_nodes.tree_root, free_nodes.black_nil);
}

