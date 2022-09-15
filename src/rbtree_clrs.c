/* Author: Alexander Griffin Lopez
 *
 * File: rbtree_clrs.c
 * ---------------------
 *  This file contains my implementation of an explicit heap allocator. This allocator uses a tree
 *  implementation to track the free space in the heap, relying on the properties of a red-black
 *  tree to remain balanced.
 *
 *  Citations:
 *  -------------------
 *  1. B&O Chapter 9. I used the explicit free list outline from the textbook, specifically
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
 *   |size_t  |or      |neighbor|or      |*parent |*left   |*right  |...     |footer  |
 *   |bytes   |black   |status  |alloc   |        |        |        |        |        |
 *   |        |        |        |        |        |        |        |        |        |
 *   +--------+--------+--------+--------+--------+--------+--------+--------+--------+
 *   |___________________________________|____________________________________________|
 *                     |                                     |
 *               64-bit header              space available for user if allocated
 *
 *
 * The rest of the rb_node remains accessible for the user, even the footer. We only need the
 * information in the rest of the struct when it is free in our tree.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./allocator.h"
#include "./debug_break.h"


/* * * * * * * * * * * * *   Type Declarations   * * * * * * * * * * * * */


typedef size_t header;
typedef unsigned char byte;

/* Red Black Free Tree:
 *  - Maintain a red black tree of free nodes.
 *  - Root is black
 *  - No red node has a red child
 *  - New insertions are red
 *  - Every path to a non-branching node has same number of black nodes.
 *  - NULL is considered black. We use a black sentinel instead.
 *  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
 *  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status.
 */
typedef struct rb_node {
    // The header will store block size, allocation status, left neighbor status, and color status.
    header header;
    // Consider a stack implementation if you want to get rid of the parent field.
    struct rb_node *parent;
    struct rb_node *left;
    struct rb_node *right;
    // ...User data...
    // header footer; which can also be overwritten.
}rb_node;

static struct tree {
    rb_node *root;
    rb_node *black_nil;
}tree;

typedef enum header_status {
    FREE = 0x0UL,
    ALLOCATED = 0x1UL,
    LEFT_ALLOCATED = 0x2UL,
    RED_PAINT = 0x4UL,
    BLK_PAINT = ~0x4UL,
    LEFT_FREE = ~0x2UL
} header_status;

typedef enum rb_color {
    BLACK = 0,
    RED = 1
}rb_color;

static struct heap {
    void *client_start;
    void *client_end;
    size_t heap_size;
}heap;

#define SIZE_MASK ~0x7UL
#define COLOR_MASK 0x4UL
#define HEAP_NODE_WIDTH (unsigned short)32
#define MIN_BLOCK_SIZE (unsigned short)40
#define HEADERSIZE sizeof(size_t)


/* * * * * * * * * *    Red-Black Tree Helper Functions   * * * * * * * * * * */


/* @brief paint_node  flips the third least significant bit to reflect the color of the node.
 * @param *node       the node we need to paint.
 * @param color       the color the user wants to paint the node.
 */
void paint_node(rb_node *node, rb_color color) {
    color == RED ? (node->header |= RED_PAINT) : (node->header &= BLK_PAINT);
}

/* @brief get_color   returns the color of a node from the value of its header.
 * @param header_val  the value of the node in question passed by value.
 * @return            RED or BLACK
 */
rb_color get_color(header header_val) {
    return (header_val & COLOR_MASK) == RED_PAINT;
}

/* @brief get_size    returns size in bytes as a size_t from the value of node's header.
 * @param header_val  the value of the node in question passed by value.
 * @return            the size in bytes as a size_t of the node.
 */
size_t get_size(header header_val) {
    return SIZE_MASK & header_val;
}

/* @brief get_min  returns the smallest node in a valid binary search tree.
 * @param *root    the root of any valid binary search tree.
 * @return         a pointer to the minimum node in a valid binary search tree.
 */
rb_node *get_min(rb_node *root) {
    for (; root->left != tree.black_nil; root = root->left) {
    }
    return root;
}

/* @brief left_rotate  complete a left rotation to help repair a red-black tree. Assumes current is
 *                     not the tree.black_nil and that the right child is not black sentinel.
 * @param *current     current will move down the tree, it's right child will move up to replace.
 * @warning            this function assumes current and current->right are not tree.black_nil.
 */
void left_rotate(rb_node *current) {
    rb_node *right_child = current->right;
    current->right = right_child->left;
    if (right_child->left != tree.black_nil) {
        right_child->left->parent = current;
    }
    right_child->parent = current->parent;
    // Take care of the root edgecase and find where the parent is in relation to current.
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
void right_rotate(rb_node *current) {
    rb_node *left_child = current->left;
    current->left = left_child->right;
    if (left_child->right != tree.black_nil) {
        left_child->right->parent = current;
    }
    left_child->parent = current->parent;
    // Take care of the root edgecase and find where the parent is in relation to current.
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


/* * * * * * * * * *     Red-Black Tree Insertion Logic     * * * * * * * * * * */


/* @brief fix_rb_insert  implements Cormen et.al. red black fixup after the insertion of a node.
 *                       Ensures that the rules of a red-black tree are upheld after insertion.
 * @param *current       the current node that has just been added to the red black tree.
 */
void fix_rb_insert(rb_node *current) {
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

/* @brief insert_rb_node  a modified insertion with additional logic to add duplicates if the
 *                        size in bytes of the block is already in the tree.
 * @param *current        we must insert to tree or add to a list as duplicate.
 */
void insert_rb_node(rb_node *current) {
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
}


/* * * * * * * * * *    Red-Black Tree Deletion Helper Functions   * * * * * * * * * * */


/* @brief rb_transplant  replaces node with the appropriate node to start balancing the tree.
 * @param *remove        the node we are removing from the tree.
 * @param *replacement   the node that will fill the remove position. It can be tree.black_nil.
 */
void rb_transplant(const rb_node *remove, rb_node *replacement) {
    if (remove->parent == tree.black_nil) {
        tree.root = replacement;
    } else if (remove == remove->parent->left) {
        remove->parent->left = replacement;
    } else {
        remove->parent->right = replacement;
    }
    replacement->parent = remove->parent;
}


/* * * * * * * * * *      Red-Black Tree Deletion Logic     * * * * * * * * * * */


/* @brief fix_rb_delete  completes repairs on a red black tree to ensure rules hold and balance.
 * @param *extra_black   the current node that was moved into place from the previous delete. It
 *                       holds an extra "black" it must get rid of by either pointing to a red node
 *                       or reaching the root. In either case it is then painted singly black.
 */
void fix_rb_delete(rb_node *extra_black) {
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
rb_node *delete_rb_node(rb_node *remove) {
    rb_color fixup_color_check = get_color(remove->header);

    // We will give the replacement of the replacement an "extra" black color.
    rb_node *extra_black = NULL;
    if (remove->left == tree.black_nil) {
        rb_transplant(remove, (extra_black = remove->right));
    } else if (remove->right == tree.black_nil) {
        rb_transplant(remove, (extra_black = remove->left));
    } else {
        // The node to remove is internal with two children of unkown size subtrees.
        rb_node *right_min = get_min(remove->right);
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
    return remove;
}

/* @brief find_best_fit  a red black tree is well suited to best fit search in O(logN) time. We
 *                       will find the best fitting node possible given the options in our tree.
 * @param key            the size_t number of bytes we are searching for in our tree.
 * @return               the pointer to the rb_node that is the best fit for our need.
 */
rb_node *find_best_fit(size_t key) {
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


/* * * * * * * * * * * *    Minor Heap Methods    * * * * * * * * * */


/* @brief roundup         rounds up size to the nearest multiple of two to be aligned in the heap.
 * @param requested_size  size given to us by the client.
 * @param multiple        the nearest multiple to raise our number to.
 * @return                rounded number.
 */
size_t roundup(const size_t requested_size, size_t multiple) {
    return (requested_size + multiple - 1) & ~(multiple - 1);
}

/* @brief is_block_allocated  determines if a node is allocated or free.
 * @param block_header        the header value of a node passed by value.
 * @return                    true if allocated false if not.
 */
bool is_block_allocated(const header block_header) {
    return block_header & ALLOCATED;
}

/* @brief is_left_space  determines if the left neighbor of a block is free or allocated.
 * @param *node          the node to check.
 * @return               true if left is free false if left is allocated.
 */
bool is_left_space(const rb_node *node) {
    return !(node->header & LEFT_ALLOCATED);
}

/* @brief get_right_neighbor  gets the address of the next rb_node in the heap to the right.
 * @param *current            the rb_node we start at to then jump to the right.
 * @param payload             the size in bytes as a size_t of the current rb_node block.
 * @return                    the rb_node to the right of the current.
 */
rb_node *get_right_neighbor(const rb_node *current, size_t payload) {
    return (rb_node *)((byte *)current + HEADERSIZE + payload);
}

/* @brief *get_left_neighbor  uses the left block size gained from the footer to move to the header.
 * @param *node               the current header at which we reside.
 * @param left_block_size     the space of the left block as reported by its footer.
 * @return                    a header pointer to the header for the block to the left.
 */
rb_node *get_left_neighbor(const rb_node *node) {
    header *left_footer = (header *)((byte *)node - HEADERSIZE);
    return (rb_node *)((byte *)node - (*left_footer & SIZE_MASK) - HEADERSIZE);
}

/* @brief init_header_size  initializes any node as the size and indicating left is allocated. Left
 *                          is allocated because we always coalesce left and right.
 * @param *node             the region of possibly uninitialized heap we must initialize.
 * @param payload           the payload in bytes as a size_t of the current block we initialize
 */
void init_header_size(rb_node *node, size_t payload) {
    node->header = LEFT_ALLOCATED | payload;
}

/* @brief get_client_space  steps into the client space just after the header of a rb_node.
 * @param *node_header      the rb_node we start at before retreiving the client space.
 * @return                  the void* address of the client space they are now free to use.
 */
void *get_client_space(const rb_node *node_header) {
    return (byte *) node_header + HEADERSIZE;
}

/* @brief get_rb_node  steps to the rb_node header from the space the client was using.
 * @param *client_space  the void* the client was using for their type. We step to the left.
 * @return               a pointer to the rb_node of our heap block.
 */
rb_node *get_rb_node(const void *client_space) {
    return (rb_node *)((byte *) client_space - HEADERSIZE);
}

/* @brief init_footer  initializes footer at end of the heap block to matcht the current header.
 * @param *node        the current node with a header field we will use to set the footer.
 * @param payload      the size of the current nodes free memory.
 */
void init_footer(rb_node *node, size_t payload) {
    header *footer = (header *)((byte *)node + payload);
    *footer = node->header;
}

/* * * * * * * * * * * *    Heap Helper Function    * * * * * * * * * */


/* @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
 * @param *to_free        the heap_node to add to the red black tree.
 * @param block_size      the size we use to initialize the node and find the right place in tree.
 */
void init_free_node(rb_node *to_free, size_t block_size) {
    to_free->header = block_size | LEFT_ALLOCATED | RED_PAINT;
    init_footer(to_free, block_size);
    get_right_neighbor(to_free, block_size)->header &= LEFT_FREE;
    insert_rb_node(to_free);
}


/* * * * * * * * * * * *    Core Heap Functions    * * * * * * * * * */


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
    return true;
}


/* @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
 *                      split, it will add the newly freed split block to the free red black tree.
 * @param *free_block   a pointer to the node for a free block in its entirety.
 * @param request       the user request for space.
 * @param block_space   the entire space that we have to work with.
 * @return              a void pointer to generic space that is now ready for the client.
 */
void *split_alloc(rb_node *free_block, size_t request, size_t block_space) {
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


/* * * * * * * * * * *   Debugging Helpers  * * * * * * * * * * * * * */


/* @breif check_init  checks the internal representation of our heap, especially the head and tail
 *                    nodes for any issues that would ruin our algorithms.
 * @return            true if everything is in order otherwise false.
 */
bool check_init() {
    // We also need to make sure the leftmost header always says there is no space to the left.
    if (is_left_space(heap.client_start)) {
        breakpoint();
        return false;
    }
    if ((byte *)heap.client_end - (byte *)heap.client_start + HEAP_NODE_WIDTH != heap.heap_size) {
        breakpoint();
        return false;
    }
    return true;
}

/* @brief get_size_used    loops through all blocks of memory to verify that the sizes
 *                         reported match the global bookeeping in our struct.
 * @param *total_free_mem  the output parameter of the total size used as another check.
 * @return                 true if our tallying is correct and our totals match.
 */
bool is_memory_balanced(size_t *total_free_mem) {
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    rb_node *cur_node = heap.client_start;
    size_t size_used = HEAP_NODE_WIDTH;
    while (cur_node != heap.client_end) {
        size_t block_size_check = get_size(cur_node->header);
        if (block_size_check == 0) {
            // Bad jump check the previous node address compared to this one.
            breakpoint();
            return false;
        }

        // Now tally valid size into total.
        if (is_block_allocated(cur_node->header)) {
            size_used += block_size_check + HEADERSIZE;
        } else {
            *total_free_mem += block_size_check + HEADERSIZE;
        }
        cur_node = get_right_neighbor(cur_node, block_size_check);
    }
    return size_used + *total_free_mem == heap.heap_size;
}

/* @brief get_black_height  gets the black node height of the tree excluding the current node.
 * @param *root             the starting root to search from to find the height.
 * @return                  the black height from the current node as an integer.
 */
int get_black_height(const rb_node *root) {
    if (root == tree.black_nil) {
        return 0;
    }
    if (get_color(root->left->header) == BLACK) {
        return 1 + get_black_height(root->left);
    }
    return get_black_height(root->left);
}

/* @brief get_tree_height  gets the max height in terms of all nodes of the tree.
 * @param *root            the root to start at to measure the height of the tree.
 * @return                 the int of the max height of the tree.
 */
int get_tree_height(const rb_node *root) {
    if (root == tree.black_nil) {
        return 0;
    }
    int left_height = 1 + get_tree_height(root->left);
    int right_height = 1 + get_tree_height(root->right);
    return left_height > right_height ? left_height : right_height;
}

/* @brief is_red_red  determines if a red red violation of a red black tree has occured.
 * @param *root       the current root of the tree to begin at for checking all subtrees.
 */
bool is_red_red(const rb_node *root) {
    if (root == tree.black_nil ||
            (root->right == tree.black_nil && root->left == tree.black_nil)) {
        return false;
    }
    if (get_color(root->header) == RED) {
        if (get_color(root->left->header) == RED
                || get_color(root->right->header) == RED) {
            return true;
        }
    }
    // Check all the subtrees.
    return is_red_red(root->right) || is_red_red(root->left);
}

/* @brief calculate_bheight  determines if every path from a node to the tree.black_nil has the
 *                           same number of black nodes.
 * @param *root              the root of the tree to begin searching.
 * @return                   -1 if the rule was not upheld, the black height if the rule is held.
 */
int calculate_bheight(const rb_node *root) {
    if (root == tree.black_nil) {
        return 0;
    }
    int lf_bheight = calculate_bheight(root->left);
    int rt_bheight = calculate_bheight(root->right);
    int add = get_color(root->header) == BLACK;
    if (lf_bheight == -1 || rt_bheight == -1 || lf_bheight != rt_bheight) {
        return -1;
    } else {
        return lf_bheight + add;
    }
}

/* @brief is_bheight_valid  the wrapper for calculate_bheight that verifies that the black height
 *                          property is upheld.
 * @param *root             the starting node of the red black tree to check.
 */
bool is_bheight_valid(const rb_node *root) {
    return calculate_bheight(root) != -1;
}

/* @brief extract_tree_mem  sums the total memory in the red black tree to see if it matches the
 *                          total memory we got from traversing blocks of the heap.
 * @param *root             the root to start at for the summing recursive search.
 * @return                  the total memory in bytes as a size_t in the red black tree.
 */
size_t extract_tree_mem(const rb_node *root) {
    if (root == tree.black_nil) {
        return 0UL;
    }
    size_t total_mem = extract_tree_mem(root->right) + extract_tree_mem(root->left);
    // We may have repeats so make sure to add the linked list values.
    total_mem += get_size(root->header) + HEADERSIZE;
    return total_mem;
}

/* @brief is_rbtree_mem_valid  a wrapper for tree memory sum function used to check correctness.
 * @param *root                the root node to begin at for the recursive summing search.
 * @return                     true if the totals match false if they do not.
 */
bool is_rbtree_mem_valid(const rb_node *root, size_t total_free_mem) {
    return extract_tree_mem(root) == total_free_mem;
}

/* @brief is_parent_valid  for duplicate node operations it is important to check the parents and
 *                         fields are updated corectly so we can continue using the tree.
 * @param *root            the root to start at for the recursive search.
 */
bool is_parent_valid(const rb_node *root) {
    if (root == tree.black_nil) {
        return true;
    }
    if (root->left != tree.black_nil && root->left->parent != root) {
        return false;
    }
    if (root->right != tree.black_nil && root->right->parent != root) {
        return false;
    }
    return is_parent_valid(root->left) && is_parent_valid(root->right);
}

/* @brief calculate_bheight_V2  verifies that the height of a red-black tree is valid. This is a
 *                              similar function to calculate_bheight but comes from a more
 *                              reliable source, because I saw results that made me doubt V1.
 */
int calculate_bheight_V2(const rb_node *root) {
    if (root == tree.black_nil) {
        return 1;
    }
    int left_height = calculate_bheight_V2(root->left);
    int right_height = calculate_bheight_V2(root->right);
    if (left_height != 0 && right_height != 0 && left_height != right_height) {
        return 0;
    }
    if (left_height != 0 && right_height != 0) {
        return get_color(root->header) == RED ? left_height : left_height + 1;
    } else {
        return 0;
    }
}

/* @brief is_bheight_valid_V2  the wrapper for calculate_bheight_V2 that verifies that the black
 *                             height property is upheld.
 * @param *root                the starting node of the red black tree to check.
 * @return                     true if the paths are valid, false if not.
 */
bool is_bheight_valid_V2(const rb_node *root) {
    return calculate_bheight_V2(tree.root) != 0;
}

/* @brief is_binary_tree  confirms the validity of a binary search tree. Nodes to the left should
 *                        be less than the root and nodes to the right should be greater.
 * @param *root           the root of the tree from which we examine children.
 * @return                true if the tree is valid, false if not.
 */
bool is_binary_tree(const rb_node *root) {
    if (root == tree.black_nil) {
        return true;
    }
    size_t root_value = get_size(root->header);
    if (root->left != tree.black_nil && root_value < get_size(root->left->header)) {
        return false;
    }
    if (root->right != tree.black_nil && root_value > get_size(root->right->header)) {
        return false;
    }
    return is_binary_tree(root->left) && is_binary_tree(root->right);
}


/* * * * * * * * * * *      Debugging       * * * * * * * * * * * * * */


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
    // Does a tree search for all memory match the linear heap search for totals?
    if (!is_rbtree_mem_valid(tree.root, total_free_mem)) {
        breakpoint();
        return false;
    }
    // Two red nodes in a row are invalid for the table.
    if (is_red_red(tree.root)) {
        breakpoint();
        return false;
    }
    // Does every path from a node to the black sentinel contain the same number of black nodes.
    if (!is_bheight_valid(tree.root)) {
        breakpoint();
        return false;
    }
    // Check that the parents and children are updated correctly if duplicates are deleted.
    if (!is_parent_valid(tree.root)) {
        breakpoint();
        return false;
    }
    // This comes from a more official write up on red black trees so I included it.
    if (!is_bheight_valid_V2(tree.root)) {
        breakpoint();
        return false;
    }
    if (!is_binary_tree(tree.root)) {
        breakpoint();
        return false;
    }
    return true;
}


/* * * * * * * * * * *   Printing Helpers   * * * * * * * * * * * * * */


// Text coloring macros (ANSI character escapes)
#define COLOR_BLK "\033[34;1m"
#define COLOR_RED "\033[31;1m"
#define COLOR_CYN "\033[36;1m"
#define COLOR_GRN "\033[32;1m"
#define COLOR_NIL "\033[0m"
#define COLOR_ERR COLOR_RED "Error: " COLOR_NIL
#define PRINTER_INDENT (short)13

typedef enum print_link {
    BRANCH = 0, // ├──
    LEAF = 1    // └──
}print_link;

/* @brief print_node  prints an individual node in its color and status as left or right child.
 * @param *root       the root we will print with the appropriate info.
 */
void print_node(const rb_node *root) {
    size_t block_size = get_size(root->header);
    printf(COLOR_CYN);
    if (root->parent != tree.black_nil) {
        root->parent->left == root ? printf("L:") : printf("R:");
    }
    printf(COLOR_NIL);
    get_color(root->header) == BLACK ? printf(COLOR_BLK) : printf(COLOR_RED);
    printf("%p:", root);
    printf("(%zubytes)", block_size);
    printf(COLOR_NIL);
    // print the black-height
    printf("(bh: %d)", get_black_height(root));
    printf("\n");
}

/* @brief print_inner_tree  recursively prints the contents of a red black tree with color and in
 *                          a style similar to a directory structure to be read from left to right.
 * @param *root             the root node to start at.
 * @param *prefix           the string we print spacing and characters across recursive calls.
 * @param node_type         the node to print can either be a leaf or internal branch.
 */
void print_inner_tree(const rb_node *root, const char *prefix, const print_link node_type) {
    if (root == tree.black_nil) {
        return;
    }
    // Print the root node
    printf("%s", prefix);
    printf("%s", node_type == LEAF ? " └──" : " ├──");
    print_node(root);

    // Print any subtrees
    char *str = NULL;
    int string_length = snprintf(NULL, 0, "%s%s", prefix, node_type == LEAF ? "     " : " │   ");
    if (string_length > 0) {
        str = malloc(string_length + 1);
        snprintf(str, string_length, "%s%s", prefix, node_type == LEAF ? "     " : " │   ");
    }
    if (str != NULL) {
        if (root->right == tree.black_nil) {
            print_inner_tree(root->left, str, LEAF);
        } else if (root->left == tree.black_nil) {
            print_inner_tree(root->right, str, LEAF);
        } else {
            print_inner_tree(root->right, str, BRANCH);
            print_inner_tree(root->left, str, LEAF);
        }
    } else {
        printf(COLOR_ERR "memory exceeded. Cannot display tree." COLOR_NIL);
    }
    free(str);
}

/* @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
 * @param *root          the root node to begin at for printing recursively.
 */
void print_rb_tree(const rb_node *root) {
    if (root == tree.black_nil) {
        return;
    }
    // Print the root node
    printf(" ");
    print_node(root);

    // Print any subtrees
    if (root->right == tree.black_nil) {
        print_inner_tree(root->left, "", LEAF);
    } else if (root->left == tree.black_nil) {
        print_inner_tree(root->right, "", LEAF);
    } else {
        print_inner_tree(root->right, "", BRANCH);
        print_inner_tree(root->left, "", LEAF);
    }
}

/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *node              a valid rb_node to a block of allocated memory.
 */
void print_alloc_block(const rb_node *node) {
    size_t block_size = get_size(node->header);
    // We will see from what direction our header is messed up by printing 16 digits.
    printf(COLOR_GRN "%p: HDR->0x%016zX(%zubytes)\n"
            COLOR_NIL, node, node->header, block_size);
}

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *header           a valid header to a block of allocated memory.
 */
void print_free_block(const rb_node *node) {
    size_t block_size = get_size(node->header);
    header *footer = (header *)((byte *)node + block_size);
    // We should be able to see the header is the same as the footer. However, due to fixup
    // functions, the color may change for nodes and color is irrelevant to footers.
    header to_print = *footer;
    if (get_size(*footer) != get_size(node->header)) {
        to_print = ULLONG_MAX;
    }
    // How far indented the Header field normally is for all blocks.
    short indent_struct_fields = PRINTER_INDENT;
    get_color(node->header) == BLACK ? printf(COLOR_BLK) : printf(COLOR_RED);
    printf("%p: HDR->0x%016zX(%zubytes)\n", node, node->header, block_size);
    printf("%*c", indent_struct_fields, ' ');

    // Printing color logic will help us spot red black violations. Tree printing later helps too.
    if (node->parent) {
        printf(get_color(node->parent->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("PRN->%p\n", node->parent);
    } else {
        printf("PRN->%p\n", NULL);
    }
    printf(COLOR_NIL);
    printf("%*c", indent_struct_fields, ' ');
    if (node->left) {
        printf(get_color(node->left->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("LFT->%p\n", node->left);
    } else {
        printf("LFT->%p\n", NULL);
    }
    printf(COLOR_NIL);
    printf("%*c", indent_struct_fields, ' ');
    if (node->right) {
        printf(get_color(node->right->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("RGT->%p\n", node->right);
    } else {
        printf("RGT->%p\n", NULL);
    }

    /* The next and footer fields may not match the current node's color bit, and that is ok. we
     * will only worry about the next node's color when we delete a duplicate.
     */
    printf(COLOR_NIL);
    printf("%*c", indent_struct_fields, ' ');
    printf("FTR->0x%016zX\n", to_print);
}

/* @brief print_error_block  prints a helpful error message if a block is corrupted.
 * @param *header            a header to a block of memory.
 * @param full_size          the full size of a block of memory, not just the user block size.
 */
void print_error_block(const rb_node *node, size_t block_size) {
    printf("\n%p: HDR->0x%016zX->%zubyts\n",
            node, node->header, block_size);
    printf("Block size is too large and header is corrupted.\n");
}

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 */
void print_bad_jump(const rb_node *current, const rb_node *prev) {
    size_t prev_size = get_size(prev->header);
    size_t cur_size = get_size(current->header);
    printf("A bad jump from the value of a header has occured. Bad distance to next header.\n");
    printf("The previous address: %p:\n", prev);
    printf("\tHeader Hex Value: %016zX:\n", prev->header);
    printf("\tBlock Byte Value: %zubytes:\n", prev_size);
    printf("\nJump by %zubytes...\n", prev_size);
    printf("The current address: %p:\n", current);
    printf("\tHeader Hex Value: 0x%016zX:\n", current->header);
    printf("\tBlock Byte Value: %zubytes:\n", cur_size);
    printf("\nJump by %zubytes...\n", cur_size);
    printf("Current state of the free tree:\n");
    print_rb_tree(tree.root);
}

/* @brief dump_tree  prints just the tree with addresses, colors, black heights, and whether a
 *                   node is a duplicate or not. Duplicates are marked with an asterisk and have
 *                   a node in their next field.
 */
void dump_tree() {
    printf(COLOR_CYN "(+X)" COLOR_NIL);
    printf(" INDICATES DUPLICATE NODES IN THE TREE. THEY HAVE A NEXT NODE.\n");
    print_rb_tree(tree.root);
}


/* * * * * * * * * * * *   Printing Debugger   * * * * * * * * * * */


/* @brief dump_heap  prints out the complete status of the heap, all of its blocks, and the sizes
 *                   the blocks occupy. Printing should be clean with no overlap of unique id's
 *                   between heap blocks or corrupted headers.
 */
void dump_heap() {
    const rb_node *node = heap.client_start;
    printf("Heap client segment starts at address %p, ends %p. %zu total bytes currently used.\n",
            node, heap.client_end, heap.heap_size);
    printf("A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n");
    printf("COLOR KEY: "
            COLOR_BLK "[BLACK NODE] " COLOR_NIL
            COLOR_RED "[RED NODE] " COLOR_NIL
            COLOR_GRN "[ALLOCATED BLOCK]" COLOR_NIL "\n\n");

    printf("%p: START OF HEAP. HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", heap.client_start);
    const rb_node *prev = node;
    while (node != heap.client_end) {
        size_t full_size = get_size(node->header);

        if (full_size == 0) {
            print_bad_jump(node, prev);
            printf("Last known pointer before jump: %p", prev);
            return;
        }
        if ((void *)node > heap.client_end) {
            print_error_block(node, full_size);
            return;
        }
        if (is_block_allocated(node->header)) {
            print_alloc_block(node);
        } else {
            print_free_block(node);
        }
        prev = node;
        node = get_right_neighbor(node, full_size);
    }
    get_color(tree.black_nil->header) == BLACK ? printf(COLOR_BLK) : printf(COLOR_RED);
    printf("%p: BLACK NULL HDR->0x%016zX\n" COLOR_NIL,
            tree.black_nil, tree.black_nil->header);
    printf("%p: FINAL ADDRESS", (byte *)heap.client_end + HEAP_NODE_WIDTH);
    printf("\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n");
    printf("COLOR KEY: "
            COLOR_BLK "[BLACK NODE] " COLOR_NIL
            COLOR_RED "[RED NODE] " COLOR_NIL
            COLOR_GRN "[ALLOCATED BLOCK]" COLOR_NIL "\n\n");

    printf("\nRED BLACK TREE OF FREE NODES AND BLOCK SIZES.\n");
    printf("HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n");
    printf(COLOR_CYN "(+X)" COLOR_NIL);
    printf(" INDICATES DUPLICATE NODES IN THE TREE. THEY HAVE A NEXT NODE.\n");
    print_rb_tree(tree.root);
}

