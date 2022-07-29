/* Last Updated: Alex G. Lopez, 2022.07.18
 * Assignment: Bonus, Tree Heap Allocator
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
 *  1. B&O Chapter 9. I used the explicit free list outline from the textbook, specifically
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
 *  3. I took much of the ideas for the pretty printing of the tree and the checks for a valid tree
 *     from Seth Furman's red black tree implementation. Specifically, the tree print structure and
 *     colors came from Furman's implementation. https://github.com/sfurman3/red-black-tree-c
 *
 *  4. I took my function to verify black node paths of a red black tree from kraskevich on
 *     stackoverflow in the following answer:
 *          https://stackoverflow.com/questions/27731072/check-whether-a-tree-satisfies-the-black-height-property-of-red-black-tree
 *
 *
 * The header stays as the first field of the heap_node_t and must remain accessible at all times.
 * The size of the block is a multiple of eight to leave the bottom three bits accessible for info.
 *
 *   v--Most Significnat bit                               v--Least Significnat Bit
 *   0        ...00000000      0             0            0
 *   +--------------------+----------+---------------+----------+
 *   |                    |          |               |          |
 *   |                    |  red     |      left     |allocation|
 *   |            size_t  |  or      |    neighbor   |  status  |----
 *   |            bytes   |  black   |   allocation  |          |   |
 *   |                    |          |     status    |          |   |
 *   +--------------------+----------+---------------+----------+   |
 *  |_____________________________________________________________| |
 *                             |                                    |
 *                        64-bit header                             |
 * |-----------------------------------------------------------------
 * |    +---------+------------+--------------+----------+------+---------+
 * |    |         |            |              |          |      |         |
 * |--> |         |            |              |          |      |         |
 *      | *parent | *links     | *links       | *list    | user |  footer |
 *      |         |[LEFT/PREV] | [RIGHT/NEXT] |  start   | data |         |
 *      |         |            |              |          | ...  |         |
 *      +---------+------------+--------------+----------+------+---------+
 *
 * The rest of the heap_node_t remains accessible for the user, even the footer. We only need the
 * information in the rest of the struct when it is free and either in our tree or doubly linked
 * list.
 */

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./allocator.h"
#include "./debug_break.h"

typedef size_t header_t;
typedef unsigned char byte_t;

/* Red Black Free Tree:
 *  - Maintain a red black tree of free nodes.
 *  - Root is black
 *  - No red node has a red child
 *  - New insertions are red
 *  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
 *  - Every path from root to tree.black_null, root not included, has same number of black nodes.
 *  - The 3rd LSB of the header_t holds the color: 0 for black, 1 for red.
 *  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status for coalescing.
 *  - Use a next pointer to a doubly linked list of duplicate nodes of the same size.
 */
typedef struct heap_node_t {
    // block size, allocation status, left neighbor status, and node color.
    header_t header;
    struct heap_node_t *parent;
    // This will combine with an enum to help unify symmetric cases for insert and delete fixes.
    struct heap_node_t *links[2];
    // Points to a list which we will use PREV and NEXT to manage to distinguish from the tree.
    struct heap_node_t *list_start;
}heap_node_t;

typedef enum node_color_t {
    BLACK = 0,
    RED = 1
}node_color_t;

// We can unify symmetric cases with an enum because (!LEFT == RIGHT) and (!RIGHT == LEFT).
typedef enum direction_t {
    LEFT = 0,
    RIGHT = 1
} direction_t;

// When you see these, know that we are working with a doubly linked list, not a tree.
typedef enum list_t {
    PREV = 0,
    NEXT = 1
} list_t;

typedef enum header_status_t {
    FREE = 0x0UL,
    ALLOCATED = 0x1UL,
    LEFT_ALLOCATED = 0x2UL,
    RED_PAINT = 0x4UL,
    BLK_PAINT = ~0x4UL,
    LEFT_FREE = ~0x2UL
} header_status_t;

static struct tree {
    heap_node_t *root;
    heap_node_t *black_null;
}tree;

// Mainly for debugging and verifying our heap.
static struct heap {
    void *client_start;
    void *client_end;
    size_t heap_size;
}heap;

#define SIZE_MASK ~0x7UL
#define COLOR_MASK 0x4UL
#define HEAP_NODE_WIDTH (unsigned short)40
#define MIN_BLOCK_SIZE (unsigned short)48
#define HEADERSIZE sizeof(size_t)


/* * * * * * * * * *    Red-Black Tree Helper Functions   * * * * * * * * * * */


/* @brief paint_node  flips the third least significant bit to reflect the color of the node.
 * @param *node       the node we need to paint.
 * @param color       the color the user wants to paint the node.
 */
void paint_node(heap_node_t *node, node_color_t color) {
    color == RED ? (node->header |= RED_PAINT) : (node->header &= BLK_PAINT);
}

/* @brief extract_color  returns the color of a node from the value of its header.
 * @param header_val     the value of the node in question passed by value.
 * @return               RED or BLACK
 */
node_color_t extract_color(header_t header_val) {
    return (header_val & COLOR_MASK) == RED_PAINT ? RED : BLACK;
}

/* @brief extract_block_size  returns size in bytes as a size_t from the value of node's header.
 * @param header_val          the value of the node in question passed by value.
 * @return                    the size in bytes as a size_t of the node.
 */
size_t extract_block_size(header_t header_val) {
    return SIZE_MASK & header_val;
}

/* @brief tree_minimum  returns the smallest node in a valid binary search tree.
 * @param *root         the root of any valid binary search tree.
 * @return              a pointer to the minimum node in a valid binary search tree.
 */
heap_node_t *tree_minimum(heap_node_t *root) {
    while (root->links[LEFT] != tree.black_null) {
        root = root->links[LEFT];
    }
    return root;
}

/* @brief rotate     a unified version of the traditional left and right rotation functions. The
 *                   rotation is either left or right and opposite is its opposite direction. We
 *                   take the current nodes child, and swap them and their arbitrary subtrees are
 *                   re-linked correctly depending on the direction of the rotation.
 * @param *current   the node around which we will rotate.
 * @param rotation   either left or right. Determines the rotation and its opposite direction.
 */
void rotate(heap_node_t *current, direction_t rotation) {
    direction_t opposite = !rotation;
    heap_node_t *child = current->links[opposite];
    current->links[opposite] = child->links[rotation];
    if (child->links[rotation] != tree.black_null) {
        child->links[rotation]->parent = current;
    }
    child->parent = current->parent;
    heap_node_t *parent = current->parent;
    if (parent == tree.black_null) {
        tree.root = child;
    } else {
        direction_t parent_link = parent->links[RIGHT] == current;
        parent->links[parent_link] = child;
    }
    child->links[rotation] = current;
    current->parent = child;
}


/* * * * * * * * * *    Red-Black Tree Insertion Helper Function   * * * * * * * * * * */


/* @brief add_duplicate  this implementation stores duplicate nodes in a linked list to prevent the
 *                       rotation of duplicates in the tree. This adds the duplicate node to the
 *                       linked list of the node already present.
 * @param *head          the node currently organized in the tree. We will add to its list.
 * @param *to_add        the node to add to the linked list.
 */
void add_duplicate(heap_node_t *head, heap_node_t *to_add) {
    to_add->header = head->header;
    // These fields should not matter but we should initialize them to be safe.
    to_add->parent = NULL;
    to_add->list_start = NULL;
    // Get the first next node in the doubly linked list, invariant and correct its left field.
    head->list_start->links[PREV] = to_add;
    to_add->links[NEXT] = head->list_start;
    to_add->links[PREV] = head;
    head->list_start = to_add;
}


/* * * * * * * * * *     Red-Black Tree Insertion Logic     * * * * * * * * * * */


/* @brief fix_rb_insert  implements a modified Cormen et.al. red black fixup after the insertion of
 *                       a new node. Unifies the symmetric left and right cases with the use of
 *                       an array and an enum direction_t.
 * @param *current       the current node that has just been added to the red black tree.
 */
void fix_rb_insert(heap_node_t *current) {
    while(extract_color(current->parent->header) == RED) {
        heap_node_t *parent = current->parent;
        direction_t symmetric_case = parent->parent->links[RIGHT] == parent;
        direction_t other_direction = !symmetric_case;
        heap_node_t *aunt = parent->parent->links[other_direction];
        if (extract_color(aunt->header) == RED) {
            paint_node(aunt, BLACK);
            paint_node(parent, BLACK);
            paint_node(parent->parent, RED);
            current = parent->parent;
        } else {
            if (current == parent->links[other_direction]) {
                current = current->parent;
                rotate(current, symmetric_case);
            }
            paint_node(current->parent, BLACK);
            paint_node(current->parent->parent, RED);
            rotate(current->parent->parent, other_direction);
        }
    }
    paint_node(tree.root, BLACK);
}

/* @brief insert_rb_node  a modified insertion with additional logic to add duplicates if the
 *                        size in bytes of the block is already in the tree.
 * @param *current        we must insert to tree or add to a list as duplicate.
 */
void insert_rb_node(heap_node_t *current) {
    heap_node_t *seeker = tree.root;
    heap_node_t *parent = tree.black_null;
    size_t current_key = extract_block_size(current->header);
    while (seeker != tree.black_null) {
        parent = seeker;
        size_t seeker_size = extract_block_size(seeker->header);

        // Duplicates with a linked list. No duplicates in tree while staying O(1) coalescing.
        if (current_key == seeker_size) {
            add_duplicate(seeker, current);
            return;
        }
        // You may see this idiom throughout. LEFT(0) if key fits in tree to left, RIGHT(1) if not.
        direction_t search_direction = seeker_size < current_key;
        seeker = seeker->links[search_direction];
    }
    current->parent = parent;
    if (parent == tree.black_null) {
        tree.root = current;
    } else {
        direction_t new_link_direction = extract_block_size(parent->header) < current_key;
        parent->links[new_link_direction] = current;
    }
    current->links[LEFT] = tree.black_null;
    current->links[RIGHT] = tree.black_null;
    // Every node in the tree is a dummy head for a doubly linked list of duplicates.
    current->list_start = tree.black_null;
    paint_node(current, RED);
    fix_rb_insert(current);
}


/* * * * * * * * * *    Red-Black Tree Deletion Helper Functions   * * * * * * * * * * */


/* @brief rb_transplant  replaces node with the appropriate node to start balancing the tree.
 * @param *to_remove     the node we are removing from the tree.
 * @param *replacement   the node that will fill the to_remove position. It can be tree.black_null.
 */
void rb_transplant(heap_node_t *to_remove, heap_node_t *replacement) {
    heap_node_t *parent = to_remove->parent;
    if (parent == tree.black_null) {
        tree.root = replacement;
    } else {
        direction_t direction = parent->links[RIGHT] == to_remove;
        parent->links[direction] = replacement;
    }
    replacement->parent = parent;
}

/* @brief delete_duplicate  will remove a duplicate node from the tree when the request is coming
 *                          from a call from malloc. Address of duplicate does not matter so we
 *                          remove the first node from the linked list.
 * @param *head             We know this node has a next node and it must be removed for malloc.
 */
heap_node_t *delete_duplicate(heap_node_t *head) {
    heap_node_t *next_node = head->list_start;
    next_node->links[NEXT]->links[PREV] = head;
    head->list_start = next_node->links[NEXT];
    return next_node;
}


/* * * * * * * * * *      Red-Black Tree Deletion Logic     * * * * * * * * * * */


/* @brief fix_rb_delete  completes a unified Cormen et.al. fixup function. Uses a direction enum
 *                       and an array to help unify code paths based on direction and opposites.
 * @param *current       the current node that was moved into place from the previous delete. It
 *                       may have broken rules of the tree or thrown off balance.
 */
void fix_rb_delete(heap_node_t *current) {
    while (current != tree.root && extract_color(current->header) == BLACK) {
        // Rotations in this function are very specific so we will forgoe making a parent variable.
        direction_t symmetric_case = current->parent->links[RIGHT] == current;
        direction_t other_direction = !symmetric_case;
        heap_node_t *sibling = current->parent->links[other_direction];
        if (extract_color(sibling->header) == RED) {
            paint_node(sibling, BLACK);
            paint_node(current->parent, RED);
            rotate(current->parent, symmetric_case);
            sibling = current->parent->links[other_direction];
        }
        if (extract_color(sibling->links[LEFT]->header) == BLACK
                && extract_color(sibling->links[RIGHT]->header) == BLACK) {
            paint_node(sibling, RED);
            current = current->parent;
        } else {
            if (extract_color(sibling->links[other_direction]->header) == BLACK) {
                paint_node(sibling->links[symmetric_case], BLACK);
                paint_node(sibling, RED);
                rotate(sibling, other_direction);
                sibling = current->parent->links[other_direction];
            }
            paint_node(sibling, extract_color(current->parent->header));
            paint_node(current->parent, BLACK);
            paint_node(sibling->links[other_direction], BLACK);
            rotate(current->parent, symmetric_case);
            current = tree.root;
        }
    }
    paint_node(current, BLACK);
}

/* @brief delete_rb_node  performs the necessary steps to have a functional, balanced tree after
 *                        deletion of any node in the tree.
 * @param *to_remove      the node to remove from the tree from a call to malloc or coalesce.
 */
heap_node_t *delete_rb_node(heap_node_t *to_remove) {
    heap_node_t *current = to_remove;
    heap_node_t *fixup_starting_node = tree.black_null;
    node_color_t original_color = extract_color(to_remove->header);

    if (to_remove->links[LEFT] == tree.black_null) {
        fixup_starting_node = to_remove->links[RIGHT];
        rb_transplant(to_remove, fixup_starting_node);
    } else if (to_remove->links[RIGHT] == tree.black_null) {
        fixup_starting_node = to_remove->links[LEFT];
        rb_transplant(to_remove, fixup_starting_node);
    } else {
        current = tree_minimum(to_remove->links[RIGHT]);
        original_color = extract_color(current->header);
        fixup_starting_node = current->links[RIGHT];
        if (current != to_remove->links[RIGHT]) {
            rb_transplant(current, fixup_starting_node);
            current->links[RIGHT] = to_remove->links[RIGHT];
            current->links[RIGHT]->parent = current;
        } else {
            fixup_starting_node->parent = current;
        }
        rb_transplant(to_remove, current);
        current->links[LEFT] = to_remove->links[LEFT];
        current->links[LEFT]->parent = current;
        paint_node(current, extract_color(to_remove->header));
    }
    if (original_color == BLACK) {
        fix_rb_delete(fixup_starting_node);
    }
    return to_remove;
}

/* @brief find_best_fit  a red black tree is well suited to best fit search in O(logN) time. We
 *                       will find the best fitting node possible given the options in our tree.
 * @param key            the size_t number of bytes we are searching for in our tree.
 * @return               the pointer to the heap_node_t that is the best fit for our need.
 */
heap_node_t *find_best_fit(size_t key) {
    heap_node_t *seeker = tree.root;
    // We will use this sentinel to start our competition while we search for best fit.
    size_t best_fit_size = ULLONG_MAX;
    heap_node_t *to_remove = seeker;
    while (seeker != tree.black_null) {
        size_t seeker_size = extract_block_size(seeker->header);

        if (key == seeker_size) {
            to_remove = seeker;
            break;
        }
        direction_t search_direction = seeker_size < key;
        /* The key is less than the current found size but let's remember this size on the way down
         * as a candidate for the best fit. The closest fit will have won when we reach the bottom.
         */
        if (search_direction == LEFT && seeker_size < best_fit_size) {
            to_remove = seeker;
            best_fit_size = seeker_size;
        }
        seeker = seeker->links[search_direction];
    }

    if (to_remove->list_start != tree.black_null) {
        // We will keep to_remove in the tree and just get the first node in doubly linked list.
        return delete_duplicate(to_remove);
    }
    return delete_rb_node(to_remove);
}

/* @brief free_coalesced_node  a specialized version of node freeing when we find a neighbor we
 *                             need to free from the tree before absorbing into our coalescing. If
 *                             this node is a duplicate we can splice it from a linked list.
 * @param *to_coalesce         the node we now must find by address in the tree.
 * @return                     the node we have now correctly freed given all cases to find it.
 */
heap_node_t *free_coalesced_node(heap_node_t *to_coalesce) {
    // Quick return if we just have a standard deletion.
    if (to_coalesce->list_start == tree.black_null) {
       return delete_rb_node(to_coalesce);
    }
    header_t size_and_bits = to_coalesce->header;
    heap_node_t *tree_parent = to_coalesce->parent;
    heap_node_t *tree_right = to_coalesce->links[RIGHT];
    heap_node_t *tree_left = to_coalesce->links[LEFT];
    // to_coalesce is the head of a doubly linked list. Remove and make a new head.
    if (to_coalesce->parent) {
        heap_node_t *new_head = to_coalesce->list_start;
        new_head->header = size_and_bits;
        // Make sure we set up new start of list correctly for linked list.
        new_head->list_start = new_head->links[NEXT];

        // Now transition to thinking about this new_head as a node in a tree, not a list.
        new_head->links[LEFT] = tree_left;
        new_head->links[RIGHT] = tree_right;
        tree_right->parent = new_head;
        tree_left->parent = new_head;
        new_head->parent = tree_parent;
        if (tree_parent == tree.black_null) {
            tree.root = new_head;
        } else {
            direction_t parent_link = tree_parent->links[RIGHT] == to_coalesce;
            tree_parent->links[parent_link] = new_head;
        }
    // to_coalesce is next after the head and needs special attention due to list_start field.
    } else if (to_coalesce->links[PREV]->list_start == to_coalesce){
        to_coalesce->links[PREV]->list_start = to_coalesce->links[NEXT];
        to_coalesce->links[NEXT]->links[PREV] = to_coalesce->links[PREV];
    // Finally the simple invariant case of the node being in middle or end of list.
    } else {
        to_coalesce->links[PREV]->links[NEXT] = to_coalesce->links[NEXT];
        to_coalesce->links[NEXT]->links[PREV] = to_coalesce->links[PREV];
    }
    return to_coalesce;
}


/* * * * * * * * * * * *    Minor Heap Methods    * * * * * * * * * */


/* @brief roundup         rounds up size to the nearest multiple of two to be aligned in the heap.
 * @param requested_size  size given to us by the client.
 * @param multiple        the nearest multiple to raise our number to.
 * @return                rounded number.
 */
size_t roundup(size_t requested_size, size_t multiple) {
    return (requested_size + multiple - 1) & ~(multiple - 1);
}

/* @brief is_block_allocated  determines if a node is allocated or free.
 * @param header              the header value of a node passed by value.
 * @return                    true if allocated false if not.
 */
bool is_block_allocated(header_t header) {
    return header & ALLOCATED;
}

/* @brief is_left_space  determines if the left neighbor of a block is free or allocated.
 * @param *node          the node to check.
 * @return               true if left is free false if left is allocated.
 */
bool is_left_space(heap_node_t *node) {
    return !(node->header & LEFT_ALLOCATED);
}

/* @brief get_right_neighbor  gets the address of the next heap_node_t in the heap to the right.
 * @param *current            the heap_node_t we start at to then jump to the right.
 * @param payload             the size in bytes as a size_t of the current heap_node_t block.
 * @return                    the heap_node_t to the right of the current.
 */
heap_node_t *get_right_neighbor(heap_node_t *current, size_t payload) {
    return (heap_node_t *)((byte_t *)current + HEADERSIZE + payload);
}

/* @brief *get_left_neighbor  uses the left block size gained from the footer to move to the header.
 * @param *node               the current header at which we reside.
 * @param left_block_size     the space of the left block as reported by its footer.
 * @return                    a header_t pointer to the header for the block to the left.
 */
heap_node_t *get_left_neighbor(heap_node_t *node) {
    header_t *left_footer = (header_t *)((byte_t *)node - HEADERSIZE);
    return (heap_node_t *)((byte_t *)node - (*left_footer & SIZE_MASK) - HEADERSIZE);
}

/* @brief init_header_size  initializes any node as the size and indicating left is allocated. Left
 *                          is allocated because we always coalesce left and right.
 * @param *node             the region of possibly uninitialized heap we must initialize.
 * @param payload           the payload in bytes as a size_t of the current block we initialize
 */
void init_header_size(heap_node_t *node, size_t payload) {
    node->header = LEFT_ALLOCATED | payload;
}

/* @brief get_client_space  steps into the client space just after the header of a heap_node_t.
 * @param *node_header      the heap_node_t we start at before retreiving the client space.
 * @return                  the void* address of the client space they are now free to use.
 */
void *get_client_space(heap_node_t *node_header) {
    return (byte_t *) node_header + HEADERSIZE;
}

/* @brief get_heap_node  steps to the heap_node_t header from the space the client was using.
 * @param *client_space  the void* the client was using for their type. We step to the left.
 * @return               a pointer to the heap_node_t of our heap block.
 */
heap_node_t *get_heap_node(void *client_space) {
    return (heap_node_t *)((byte_t *) client_space - HEADERSIZE);
}

/* @brief init_footer  initializes footer at end of the heap block to matcht the current header.
 * @param *node        the current node with a header field we will use to set the footer.
 */
void init_footer(heap_node_t *node, size_t payload) {
    header_t *footer = (header_t *)((byte_t *)node + payload);
    *footer = node->header;
}


/* * * * * * * * * * * *    Heap Helper Function    * * * * * * * * * */


/* @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
 * @param *to_free        the heap_node to add to the red black tree.
 * @param block_size      the size we use to initialize the node and find the right place in tree.
 */
void init_free_node(heap_node_t *to_free, size_t block_size) {
    to_free->header = LEFT_ALLOCATED | block_size;
    to_free->header |= RED_PAINT;
    to_free->list_start = tree.black_null;
    // Block sizes don't include header size so this is safe addition.
    header_t *footer = (header_t *)((byte_t *)to_free + block_size);
    *footer = to_free->header;
    heap_node_t *neighbor = (heap_node_t *)((byte_t *) footer + HEADERSIZE);
    neighbor->header &= LEFT_FREE;
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
    heap.client_end = (byte_t *)heap.client_start + heap.heap_size - HEAP_NODE_WIDTH;
    // Set up the dummy base of the tree to which all leaves will point.
    tree.black_null = heap.client_end;
    tree.black_null->header = 1UL;
    tree.black_null->parent = NULL;
    tree.black_null->links[LEFT] = NULL;
    tree.black_null->links[RIGHT] = NULL;
    tree.black_null->list_start = NULL;
    paint_node(tree.black_null, BLACK);
    // Set up the root of the tree (top) that starts as our largest free block.
    tree.root = heap.client_start;
    init_header_size(tree.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE);
    paint_node(tree.root, BLACK);
    init_footer(tree.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE);
    tree.root->parent = tree.black_null;
    tree.root->links[LEFT] = tree.black_null;
    tree.root->links[RIGHT] = tree.black_null;
    tree.root->list_start = tree.black_null;
    return true;
}

/* @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
 *                      split, it will add the newly freed split block to the free red black tree.
 * @param *free_block   a pointer to the node for a free block in its entirety.
 * @param request       the user request for space.
 * @param block_space   the entire space that we have to work with.
 * @return              a void pointer to generic space that is now ready for the client.
 */
void *split_alloc(heap_node_t *free_block, size_t request, size_t block_space) {
    heap_node_t *neighbor = NULL;
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
        heap_node_t *found_node = find_best_fit(client_request);
        return split_alloc(found_node, client_request, extract_block_size(found_node->header));
    }
    return NULL;
}

/* @brief coalesce        attempts to coalesce left and right if the left and right heap_node_t
 *                        are free. Runs the search to free the specific free node in O(logN) + d
 *                        where d is the number of duplicate nodes of the same size.
 * @param *leftmost_node  the current node that will move left if left is free to coalesce.
 * @return                the leftmost node from attempts to coalesce left and right. The leftmost
 *                        node is initialized to reflect the correct size for the space it now has.
 * @warning               this function does not overwrite the data that may be in the middle if we
 *                        expand left and write. The user may wish to move elsewhere if reallocing.
 */
heap_node_t *coalesce(heap_node_t *leftmost_node) {
    // What if your left or right free node to coalesce is a repeat?
    // It may not be the first node in the repeat list.
    size_t coalesced_space = extract_block_size(leftmost_node->header);
    heap_node_t *rightmost_node = get_right_neighbor(leftmost_node, coalesced_space);
    if (!is_block_allocated(rightmost_node->header)) {
        coalesced_space += extract_block_size(rightmost_node->header) + HEADERSIZE;
        rightmost_node = free_coalesced_node(rightmost_node);
    }
    if (leftmost_node != heap.client_start && is_left_space(leftmost_node)) {
        leftmost_node = get_left_neighbor(leftmost_node);
        coalesced_space += extract_block_size(leftmost_node->header) + HEADERSIZE;
        leftmost_node = free_coalesced_node(leftmost_node);
    }
    // We do not initialize a footer here because we don't want to overwrite user data.
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
    heap_node_t *old_node = get_heap_node(old_ptr);
    size_t old_size = extract_block_size(old_node->header);

    heap_node_t *leftmost_node = coalesce(old_node);
    size_t coalesced_space = extract_block_size(leftmost_node->header);
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
        heap_node_t *to_insert = get_heap_node(ptr);
        to_insert = coalesce(to_insert);
        init_free_node(to_insert, extract_block_size(to_insert->header));
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
    if ((byte_t *)heap.client_end
            - (byte_t *)heap.client_start + HEAP_NODE_WIDTH != heap.heap_size) {
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
    heap_node_t *cur_node = heap.client_start;
    size_t size_used = HEAP_NODE_WIDTH;
    while (cur_node != heap.client_end) {
        size_t block_size_check = extract_block_size(cur_node->header);
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
int get_black_height(heap_node_t *root) {
    if (root == tree.black_null) {
        return 0;
    }
    if (extract_color(root->links[LEFT]->header) == BLACK) {
        return 1 + get_black_height(root->links[LEFT]);
    }
    return get_black_height(root->links[LEFT]);
}

/* @brief get_tree_height  gets the max height in terms of all nodes of the tree.
 * @param *root            the root to start at to measure the height of the tree.
 * @return                 the int of the max height of the tree.
 */
int get_tree_height(heap_node_t *root) {
    if (root == tree.black_null) {
        return 0;
    }
    int left_height = 1 + get_tree_height(root->links[LEFT]);
    int right_height = 1 + get_tree_height(root->links[RIGHT]);
    return left_height > right_height ? left_height : right_height;
}

/* @brief is_red_red  determines if a red red violation of a red black tree has occured.
 * @param *root       the current root of the tree to begin at for checking all subtrees.
 */
bool is_red_red(heap_node_t *root) {
    if (root == tree.black_null ||
            (root->links[RIGHT] == tree.black_null
             && root->links[LEFT] == tree.black_null)) {
        return false;
    }
    if (extract_color(root->header) == RED) {
        if (extract_color(root->links[LEFT]->header) == RED
                || extract_color(root->links[RIGHT]->header) == RED) {
            return true;
        }
    }
    // Check all the subtrees.
    return is_red_red(root->links[RIGHT]) || is_red_red(root->links[LEFT]);
}

/* @brief calculate_bheight  determines if every path from a node to the tree.black_null has the
 *                           same number of black nodes.
 * @param *root              the root of the tree to begin searching.
 * @return                   -1 if the rule was not upheld, the black height if the rule is held.
 */
int calculate_bheight(heap_node_t *root) {
    if (root == tree.black_null) {
        return 0;
    }
    int lf_bheight = calculate_bheight(root->links[LEFT]);
    int rt_bheight = calculate_bheight(root->links[RIGHT]);
    int add = extract_color(root->header) == BLACK ? 1 : 0;
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
bool is_bheight_valid(heap_node_t *root) {
    return calculate_bheight(root) != -1;
}

/* @brief extract_tree_mem  sums the total memory in the red black tree to see if it matches the
 *                          total memory we got from traversing blocks of the heap.
 * @param *root             the root to start at for the summing recursive search.
 * @return                  the total memory in bytes as a size_t in the red black tree.
 */
size_t extract_tree_mem(heap_node_t *root) {
    if (root == tree.black_null) {
        return 0UL;
    }
    size_t total_mem = extract_tree_mem(root->links[RIGHT])
                       + extract_tree_mem(root->links[LEFT]);
    // We may have repeats so make sure to add the linked list values.
    size_t node_size = extract_block_size(root->header) + HEADERSIZE;
    total_mem += node_size;
    if ((root = root->list_start) != tree.black_null) {
        // We have now entered a doubly linked list that uses left(prev) and right(next).
        while (root != tree.black_null) {
            total_mem += node_size;
            root = root->links[NEXT];
        }
    }
    return total_mem;
}

/* @brief is_rbtree_mem_valid  a wrapper for tree memory sum function used to check correctness.
 * @param *root                the root node to begin at for the recursive summing search.
 * @return                     true if the totals match false if they do not.
 */
bool is_rbtree_mem_valid(heap_node_t *root, size_t total_free_mem) {
    return extract_tree_mem(root) == total_free_mem;
}

/* @brief is_parent_valid  for duplicate node operations it is important to check the parents and
 *                         fields are updated corectly so we can continue using the tree.
 * @param *root            the root to start at for the recursive search.
 */
bool is_parent_valid(heap_node_t *root) {
    if (root == tree.black_null) {
        return true;
    }
    if (root->links[LEFT] != tree.black_null && root->links[LEFT]->parent != root) {
        return false;
    }
    if (root->links[RIGHT] != tree.black_null && root->links[RIGHT]->parent != root) {
        return false;
    }
    return is_parent_valid(root->links[LEFT]) && is_parent_valid(root->links[RIGHT]);
}

/* @brief calculate_bheight_V2  verifies that the height of a red-black tree is valid. This is a
 *                              similar function to calculate_bheight but comes from a more
 *                              reliable source, because I saw results that made me doubt V1.
 */
int calculate_bheight_V2(heap_node_t *root) {
    if (root == tree.black_null) {
        return 1;
    }
    heap_node_t *left = root->links[LEFT];
    heap_node_t *right = root->links[RIGHT];
    int left_height = calculate_bheight_V2(left);
    int right_height = calculate_bheight_V2(right);
    if (left_height != 0 && right_height != 0 && left_height != right_height) {
        return 0;
    }
    if (left_height != 0 && right_height != 0) {
        return extract_color(root->header) == RED ? left_height : left_height + 1;
    } else {
        return 0;
    }
}

/* @brief is_bheight_valid_V2  the wrapper for calculate_bheight_V2 that verifies that the black
 *                             height property is upheld.
 * @param *root                the starting node of the red black tree to check.
 */
bool is_bheight_valid_V2(heap_node_t *root) {
    return calculate_bheight_V2(tree.root) != 0;
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
    // This comes from a more official write up on red black trees so I included it.
    if (!is_bheight_valid_V2(tree.root)) {
        breakpoint();
        return false;
    }
    // Check that the parents and children are updated correctly if duplicates are deleted.
    if (!is_parent_valid(tree.root)) {
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

typedef enum print_node_t {
    BRANCH = 0, // ├──
    LEAF = 1    // └──
}print_node_t;

/* @brief print_node  prints an individual node in its color and status as left or right child.
 * @param *root       the root we will print with the appropriate info.
 */
void print_node(heap_node_t *root) {
    size_t block_size = extract_block_size(root->header);
    printf(COLOR_CYN);
    if (root->parent != tree.black_null) {
        root->parent->links[LEFT] == root ? printf("L:") : printf("R:");
    }
    printf(COLOR_NIL);
    extract_color(root->header) == BLACK ? printf(COLOR_BLK) : printf(COLOR_RED);
    printf("%p:", root);
    printf("(%zubytes)", block_size);
    printf(COLOR_NIL);
    // print the black-height
    printf("(bh: %d)", get_black_height(root));
    printf(COLOR_CYN);
    // If a node is a duplicate, we will give it a special mark among nodes.
    if (root->list_start != tree.black_null) {
        int duplicates = 1;
        heap_node_t *duplicate = root->list_start;
        for (;(duplicate = duplicate->links[NEXT]) != tree.black_null; duplicates++) {
        }
        printf("(+%d)", duplicates);
    }
    printf(COLOR_NIL);
    printf("\n");
}

/* @brief print_inner_tree  recursively prints the contents of a red black tree with color and in
 *                          a style similar to a directory structure to be read from left to right.
 * @param *root             the root node to start at.
 * @param *prefix           the string we print spacing and characters across recursive calls.
 * @param node_type         the node to print can either be a leaf or internal branch.
 */
void print_inner_tree(heap_node_t *root, char *prefix, print_node_t node_type) {
    if (root == tree.black_null) {
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
        if (root->links[RIGHT] == tree.black_null) {
            print_inner_tree(root->links[LEFT], str, LEAF);
        } else if (root->links[LEFT] == tree.black_null) {
            print_inner_tree(root->links[RIGHT], str, LEAF);
        } else {
            print_inner_tree(root->links[RIGHT], str, BRANCH);
            print_inner_tree(root->links[LEFT], str, LEAF);
        }
    } else {
        printf(COLOR_ERR "memory exceeded. Cannot display tree." COLOR_NIL);
    }
    free(str);
}

/* @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
 * @param *root          the root node to begin at for printing recursively.
 */
void print_rb_tree(heap_node_t *root) {
    if (root == tree.black_null) {
        return;
    }
    // Print the root node
    printf(" ");
    print_node(root);

    // Print any subtrees
    if (root->links[RIGHT] == tree.black_null) {
        print_inner_tree(root->links[LEFT], "", LEAF);
    } else if (root->links[LEFT] == tree.black_null) {
        print_inner_tree(root->links[RIGHT], "", LEAF);
    } else {
        print_inner_tree(root->links[RIGHT], "", BRANCH);
        print_inner_tree(root->links[LEFT], "", LEAF);
    }
}

/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *node              a valid heap_node_t to a block of allocated memory.
 */
void print_alloc_block(heap_node_t *node) {
    size_t block_size = extract_block_size(node->header);
    // We will see from what direction our header is messed up by printing 16 digits.
    printf(COLOR_GRN "%p: HDR->0x%016zX(%zubytes)\n"
            COLOR_NIL, node, node->header, block_size);
}

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *header           a valid header to a block of allocated memory.
 */
void print_free_block(heap_node_t *node) {
    size_t block_size = extract_block_size(node->header);
    header_t *footer = (header_t *)((byte_t *)node + block_size);
    // We should be able to see the header is the same as the footer. However, due to fixup
    // functions, the color may change for nodes and color is irrelevant to footers.
    header_t to_print = *footer;
    if (extract_block_size(*footer) != extract_block_size(node->header)) {
        to_print = ULLONG_MAX;
    }
    // How far indented the Header field normally is for all blocks.
    short indent_struct_fields = PRINTER_INDENT;
    extract_color(node->header) == BLACK ? printf(COLOR_BLK) : printf(COLOR_RED);
    printf("%p: HDR->0x%016zX(%zubytes)\n", node, node->header, block_size);
    printf("%*c", indent_struct_fields, ' ');

    // Printing color logic will help us spot red black violations. Tree printing later helps too.
    if (node->parent) {
        printf(extract_color(node->parent->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("PRN->%p\n", node->parent);
    } else {
        printf("PRN->%p\n", NULL);
    }
    printf(COLOR_NIL);
    printf("%*c", indent_struct_fields, ' ');
    if (node->links[LEFT]) {
        printf(extract_color(node->links[LEFT]->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("LFT->%p\n", node->links[LEFT]);
    } else {
        printf("LFT->%p\n", NULL);
    }
    printf(COLOR_NIL);
    printf("%*c", indent_struct_fields, ' ');
    if (node->links[RIGHT]) {
        printf(extract_color(node->links[RIGHT]->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("RGT->%p\n", node->links[RIGHT]);
    } else {
        printf("RGT->%p\n", NULL);
    }

    /* The next and footer fields may not match the current node's color bit, and that is ok. we
     * will only worry about the next node's color when we delete a duplicate.
     */
    printf(COLOR_NIL);
    printf("%*c", indent_struct_fields, ' ');
    printf("LST->%p\n", node->list_start ? node->list_start : NULL);
    printf("%*c", indent_struct_fields, ' ');
    printf("FTR->0x%016zX\n", to_print);
}

/* @brief print_error_block  prints a helpful error message if a block is corrupted.
 * @param *header            a header to a block of memory.
 * @param full_size          the full size of a block of memory, not just the user block size.
 */
void print_error_block(heap_node_t *node, size_t block_size) {
    printf("\n%p: HDR->0x%016zX->%zubyts\n",
            node, node->header, block_size);
    printf("Block size is too large and header is corrupted.\n");
}

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 */
void print_bad_jump(heap_node_t *current, heap_node_t *prev) {
    size_t prev_size = extract_block_size(prev->header);
    size_t cur_size = extract_block_size(current->header);
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
    heap_node_t *node = heap.client_start;
    printf("Heap client segment starts at address %p, ends %p. %zu total bytes currently used.\n",
            node, heap.client_end, heap.heap_size);
    printf("A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n");
    printf("COLOR KEY: "
            COLOR_BLK "[BLACK NODE] " COLOR_NIL
            COLOR_RED "[RED NODE] " COLOR_NIL
            COLOR_GRN "[ALLOCATED BLOCK]" COLOR_NIL "\n\n");

    printf("%p: START OF HEAP. HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", heap.client_start);
    heap_node_t *prev = node;
    while (node != heap.client_end) {
        size_t full_size = extract_block_size(node->header);

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
    extract_color(tree.black_null->header) == BLACK ? printf(COLOR_BLK) : printf(COLOR_RED);
    printf("%p: BLACK NULL HDR->0x%016zX\n" COLOR_NIL,
            tree.black_null, tree.black_null->header);
    printf("%p: FINAL ADDRESS", (byte_t *)heap.client_end + HEAP_NODE_WIDTH);
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
