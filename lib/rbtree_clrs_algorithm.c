/**
 * Author: Alexander Griffin Lopez
 *
 * File: rbtree_clrs.c
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
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "allocator.h"
#include "rbtree_clrs_utilities.h"


/* * * * * * * * * * * * * * * * * *  Static Heap Tracking  * * * * * * * * * * * * * * * * * * */


/* Red Black Free Tree:
 *  - Maintain a red black tree of free nodes.
 *  - Root is black
 *  - No red node has a red child
 *  - New insertions are red
 *  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
 *  - Every path from root to tree.black_nil, root not included, has same number of black nodes.
 *  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
 *  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status for coalescing.
 *  - For more details, see the _utilities.h file.
 */
static struct tree {
    rb_node *root;
    rb_node *black_nil;
    size_t total;
}tree;

static struct heap {
    void *client_start;
    void *client_end;
    size_t heap_size;
}heap;


/* * * * * * * * * * * * * * * * *   Static Helper Functions   * * * * * * * * * * * * * * * * * */


/* @brief left_rotate  complete a left rotation to help repair a red-black tree. Assumes current is
 *                     not the tree.black_nil and that the right child is not black sentinel.
 * @param *current     current will move down the tree, it's right child will move up to replace.
 * @warning            this function assumes current and current->right are not tree.black_nil.
 */
static void left_rotate(rb_node *current) {
    rb_node *right_child = current->right;
    current->right = right_child->left;
    if (right_child->left != tree.black_nil) {
        right_child->left->parent = current;
    }
    right_child->parent = current->parent;
    if (current->parent == tree.black_nil) {
        tree.root = right_child;
    } else if (current == current->parent->left) {
        current->parent->left = right_child;
    } else {
        current->parent->right = right_child;
    }
    right_child->left = current;
    current->parent = right_child;
}

/* @brief right_rotate  completes a right rotation to help repair a red-black tree. Assumes current
 *                      is not the tree.black_nil and the right child is not tree.black_nil.
 * @param *current      the current node moves down the tree and the left child moves up.
 * @warning             this function assumes current and current->right are not tree.black_nil.
 */
static void right_rotate(rb_node *current) {
    rb_node *left_child = current->left;
    current->left = left_child->right;
    if (left_child->right != tree.black_nil) {
        left_child->right->parent = current;
    }
    left_child->parent = current->parent;
    if (current->parent == tree.black_nil) {
        tree.root = left_child;
    } else if (current == current->parent->right) {
        current->parent->right = left_child;
    } else {
        current->parent->left = left_child;
    }
    left_child->right = current;
    current->parent = left_child;
}


/* * * * * * * * * * * *     Static Red-Black Tree Insertion Logic     * * * * * * * * * * * * * */


/* @brief fix_rb_insert  implements Cormen et.al. red black fixup after the insertion of a node.
 *                       Ensures that the rules of a red-black tree are upheld after insertion.
 * @param *current       the current node that has just been added to the red black tree.
 */
static void fix_rb_insert(rb_node *current) {
    while(get_color(current->parent->header) == RED) {
        if (current->parent == current->parent->parent->left) {
            rb_node *uncle = current->parent->parent->right;
            if (get_color(uncle->header) == RED) {
                paint_node(current->parent, BLACK);
                paint_node(uncle, BLACK);
                paint_node(current->parent->parent, RED);
                current = current->parent->parent;
            } else {  // uncle is BLACK
                if (current == current->parent->right) {
                    current = current->parent;
                    left_rotate(current);
                }
                paint_node(current->parent, BLACK);
                paint_node(current->parent->parent, RED);
                right_rotate(current->parent->parent);
            }
        } else {
            rb_node *uncle = current->parent->parent->left;
            if (get_color(uncle->header) == RED) {
                paint_node(current->parent, BLACK);
                paint_node(uncle, BLACK);
                paint_node(current->parent->parent, RED);
                current = current->parent->parent;
            } else {  // uncle is BLACK
                if (current == current->parent->left) {
                    current = current->parent;
                    right_rotate(current);
                }
                paint_node(current->parent, BLACK);
                paint_node(current->parent->parent, RED);
                left_rotate(current->parent->parent);
            }
        }
    }
    paint_node(tree.root, BLACK);
}

/* @brief insert_rb_node  a simple binary tree insert with additional red black fixup logic.
 * @param *current        we must insert to tree or add to a list as duplicate.
 */
static void insert_rb_node(rb_node *current) {
    rb_node *child = tree.root;
    rb_node *parent = tree.black_nil;
    size_t current_key = get_size(current->header);
    while (child != tree.black_nil) {
        parent = child;
        size_t child_size = get_size(child->header);
        if (current_key < child_size) {
            child = child->left;
        } else {
            child = child->right;
        }
    }
    current->parent = parent;
    if (parent == tree.black_nil) {
        tree.root = current;
    } else if (current_key < get_size(parent->header)) {
        parent->left = current;
    } else {
        parent->right = current;
    }
    current->left = tree.black_nil;
    current->right = tree.black_nil;
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
    } else if (remove == remove->parent->left) {
        remove->parent->left = replacement;
    } else {
        remove->parent->right = replacement;
    }
    replacement->parent = remove->parent;
}


/* * * * * * * * * * * *     Static Red-Black Tree Deletion Logic      * * * * * * * * * * * * * */


/* @brief fix_rb_delete  completes repairs on a red black tree to ensure rules hold and balance.
 * @param *extra_black   the current node that was moved into place from the previous delete. It
 *                       holds an extra "black" it must get rid of by either pointing to a red node
 *                       or reaching the root. In either case it is then painted singly black.
 */
static void fix_rb_delete(rb_node *extra_black) {
    // If we enter the loop extra_black points to a black node making it "doubly black".
    while (extra_black != tree.root && get_color(extra_black->header) == BLACK) {
        if (extra_black == extra_black->parent->left) {
            rb_node *right_sibling = extra_black->parent->right;
            if (get_color(right_sibling->header) == RED) {
                paint_node(right_sibling, BLACK);
                paint_node(extra_black->parent, RED);
                left_rotate(extra_black->parent);
                right_sibling = extra_black->parent->right;
            }
            // The previous left rotation may have made the right sibling black null.
            if (get_color(right_sibling->left->header) == BLACK
                    && get_color(right_sibling->right->header) == BLACK) {
                paint_node(right_sibling, RED);
                extra_black = extra_black->parent;
            } else {
                if (get_color(right_sibling->right->header) == BLACK) {
                    paint_node(right_sibling->left, BLACK);
                    paint_node(right_sibling, RED);
                    right_rotate(right_sibling);
                    right_sibling = extra_black->parent->right;
                }
                paint_node(right_sibling, get_color(extra_black->parent->header));
                paint_node(extra_black->parent, BLACK);
                paint_node(right_sibling->right, BLACK);
                left_rotate(extra_black->parent);
                extra_black = tree.root;
            }
        } else {
            // This is a symmetric case, so it is identical with left and right switched.
            rb_node *left_sibling = extra_black->parent->left;
            if (get_color(left_sibling->header) == RED) {
                paint_node(left_sibling, BLACK);
                paint_node(extra_black->parent, RED);
                right_rotate(extra_black->parent);
                left_sibling = extra_black->parent->left;
            }
            // The previous left rotation may have made the right sibling black null.
            if (get_color(left_sibling->right->header) == BLACK
                    && get_color(left_sibling->left->header) == BLACK) {
                paint_node(left_sibling, RED);
                extra_black = extra_black->parent;
            } else {
                if (get_color(left_sibling->left->header) == BLACK) {
                    paint_node(left_sibling->right, BLACK);
                    paint_node(left_sibling, RED);
                    left_rotate(left_sibling);
                    left_sibling = extra_black->parent->left;
                }
                paint_node(left_sibling, get_color(extra_black->parent->header));
                paint_node(extra_black->parent, BLACK);
                paint_node(left_sibling->left, BLACK);
                right_rotate(extra_black->parent);
                extra_black = tree.root;
            }
        }
    }
    // Node has either become "red-and-black" by pointing to a red node or is root. Paint black.
    paint_node(extra_black, BLACK);
}

/* @brief delete_rb_node  performs the necessary steps to have a functional, balanced tree after
 *                        deletion of any node in the tree.
 * @param *remove         the node to remove from the tree from a call to malloc or coalesce.
 */
static rb_node *delete_rb_node(rb_node *remove) {
    rb_color fixup_color_check = get_color(remove->header);

    // We will give the replacement of the replacement an "extra" black color.
    rb_node *extra_black = NULL;
    if (remove->left == tree.black_nil) {
        rb_transplant(remove, (extra_black = remove->right));
    } else if (remove->right == tree.black_nil) {
        rb_transplant(remove, (extra_black = remove->left));
    } else {
        // The node to remove is internal with two children of unkown size subtrees.
        rb_node *right_min = get_min(remove->right, tree.black_nil);
        fixup_color_check = get_color(right_min->header);

        // Possible this is black_nil and that's ok.
        extra_black = right_min->right;
        if (right_min != remove->right) {
            rb_transplant(right_min, right_min->right);
            right_min->right = remove->right;
            right_min->right->parent = right_min;
        } else {
            extra_black->parent = right_min;
        }
        rb_transplant(remove, right_min);
        right_min->left = remove->left;
        right_min->left->parent = right_min;
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
 * @param key            the size_t number of bytes we are searching for in our tree.
 * @return               the pointer to the rb_node that is the best fit for our need.
 */
static rb_node *find_best_fit(size_t key) {
    rb_node *seeker = tree.root;
    size_t best_fit_size = ULLONG_MAX;
    rb_node *remove = seeker;
    while (seeker != tree.black_nil) {
        size_t seeker_size = get_size(seeker->header);
        if (key == seeker_size) {
            remove = seeker;
            break;
        } else if (key < seeker_size) {
            if(seeker_size < best_fit_size) {
                remove = seeker;
                best_fit_size = seeker_size;
            }
            seeker = seeker->left;
        } else {
            seeker = seeker->right;
        }
    }
    // We can decompose the Cormen et.al. deletion logic because coalesce and malloc use it.
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
    tree.black_nil->parent = NULL;
    tree.black_nil->left = NULL;
    tree.black_nil->right = NULL;
    paint_node(tree.black_nil, BLACK);
    // Set up the root of the tree (top) that starts as our largest free block.
    tree.root = heap.client_start;
    init_header_size(tree.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE);
    paint_node(tree.root, BLACK);
    init_footer(tree.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE);
    tree.root->parent = tree.black_nil;
    tree.root->left = tree.black_nil;
    tree.root->right = tree.black_nil;
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
    printf("\n");
    print_rb_tree(tree.root, tree.black_nil, style);
}

/* @brief dump_heap  prints out the complete status of the heap, all of its blocks, and the sizes
 *                   the blocks occupy. Printing should be clean with no overlap of unique id's
 *                   between heap blocks or corrupted headers.
 */
void dump_heap() {
    print_all(heap.client_start, heap.client_end, heap.heap_size, tree.root, tree.black_nil);
}

