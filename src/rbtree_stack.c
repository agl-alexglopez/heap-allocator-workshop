/* Last Updated: Alex G. Lopez, 2022.07.18
 * Assignment: Bonus, Tree Heap Allocator
 *
 * File: rbtree_stack.c
 * ---------------------
 *  This file contains my implementation of an explicit heap allocator. This allocator uses a tree
 *  implementation to track the free space in the heap, relying on the properties of a red-black
 *  tree to remain balanced. This implementation also uses some interesting strategies to unify
 *  left and right cases for a red black tree and maintains a doubly linked list of duplicate nodes
 *  of the same size if a node in the tree has repeats of its size. We do not use a parent field,
 *  instead opting to form a stack of nodes and pass that stack to the necessary functions.
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
 *  5. I learned about the concept of unifying the left and right cases for red black trees through
 *     this archived article on a stack overflow post. It is a great way to simplify code.
 *          https://web.archive.org/web/20190207151651/http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx
 *
 *
 * The header stays as the first field of the tree_node_t and must remain accessible at all times.
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
 * |    +----------+----------+-----------+----------+------+
 * |    |          |          |           |          |      |
 * |--> |          |          |           |          |      |
 *      |links[L/P]|links[R/N]|*list_start|user space|footer|
 *      |          |          |           |          |      |
 *      |          |          |           |          |      |
 *      +----------+----------+-----------+----------+------+
 *
 *
 * The rest of the tree_node_t remains accessible for the user, even the footer. We only need the
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


/* * * * * * * * * * * * *   Type Declarations   * * * * * * * * * * * * */


#define TWO_NODE_ARRAY (unsigned short)2
typedef size_t header_t;
typedef unsigned char byte_t;

/* Red Black Free Tree:
 *  - Maintain a red black tree of free nodes.
 *  - Root is black
 *  - No red node has a red child
 *  - New insertions are red
 *  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
 *  - Every path from root to free.black_nil, root not included, has same number of black nodes.
 *  - The 3rd LSB of the header_t holds the color: 0 for black, 1 for red.
 *  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status for coalescing.
 *  - Use a next pointer to a doubly linked list of duplicate nodes of the same size.
 */
typedef struct tree_node_t {
    // The header will store block size, allocation status, left neighbor status, and node color.
    header_t header;
    struct tree_node_t *links[TWO_NODE_ARRAY];
    // Use list_start to maintain doubly linked list, using the links[P]-links[N] fields
    struct duplicate_t *list_start;
}tree_node_t;

typedef struct duplicate_t {
    header_t header;
    struct duplicate_t *links[TWO_NODE_ARRAY];
    // We will always store the tree parent in first duplicate node in the list. O(1) coalescing.
    struct tree_node_t *parent;
} duplicate_t;

typedef enum rb_color_t {
    BLACK = 0,
    RED = 1
}rb_color_t;

// Symmetry can be unified to one case because (!L == R) and (!R == L).
typedef enum tree_link_t {
    // (L == LEFT), (R == RIGHT)
    L = 0,
    R = 1
} tree_link_t;

// We use these fields to refer to our doubly linked duplicate nodes.
typedef enum list_link_t {
    // (P == PREVIOUS), (N == NEXT)
    P = 0,
    N = 1
} list_link_t;

typedef enum header_status_t {
    FREE = 0x0UL,
    ALLOCATED = 0x1UL,
    LEFT_ALLOCATED = 0x2UL,
    RED_PAINT = 0x4UL,
    BLK_PAINT = ~0x4UL,
    LEFT_FREE = ~0x2UL
} header_status_t;

static struct free_nodes {
    tree_node_t *tree_root;
    // These two pointers will point to the same address. Used for clarity between tree and list.
    tree_node_t *black_nil;
    duplicate_t *list_tail;
}free_nodes;

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
// Red black trees are always balanced so this should be plenty of height (2^50 nodes)
#define MAX_TREE_HEIGHT (unsigned short)50


/* * * * * * * * * *    Red-Black Tree Helper Functions   * * * * * * * * * * */


/* @brief paint_node  flips the third least significant bit to reflect the color of the node.
 * @param *node       the node we need to paint.
 * @param color       the color the user wants to paint the node.
 */
void paint_node(tree_node_t *node, rb_color_t color) {
    color == RED ? (node->header |= RED_PAINT) : (node->header &= BLK_PAINT);
}

/* @brief extract_color  returns the color of a node from the value of its header.
 * @param header_val     the value of the node in question passed by value.
 * @return               RED or BLACK
 */
rb_color_t extract_color(header_t header_val) {
    return (header_val & COLOR_MASK) == RED_PAINT;
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
tree_node_t *tree_minimum(tree_node_t *root, tree_node_t *path[], int *path_len) {
    for (; root->links[L] != free_nodes.black_nil; root = root->links[L]) {
        path[(*path_len)++] = root;
    }
    path[(*path_len)++] = root;
    return root;
}

/* @brief rotate    a unified version of the traditional left and right rotation functions. The
 *                  rotation is either left or right and opposite is its opposite direction. We
 *                  take the current nodes child, and swap them and their arbitrary subtrees are
 *                  re-linked correctly depending on the direction of the rotation.
 * @param *current  the node around which we will rotate.
 * @param rotation  either left or right. Determines the rotation and its opposite direction.
 * @warning         this function modifies the stack.
 */
void rotate(tree_link_t rotation, tree_node_t *current, tree_node_t *path[], int path_len) {
    tree_node_t *child = current->links[!rotation];
    current->links[!rotation] = child->links[rotation];

    if (child->links[rotation] != free_nodes.black_nil) {
        child->links[rotation]->list_start->parent = current;
    }

    tree_node_t *parent = path[path_len - 2];
    if (child != free_nodes.black_nil) {
        child->list_start->parent = parent;
    }

    if (parent == free_nodes.black_nil) {
        free_nodes.tree_root = child;
    } else {
        parent->links[parent->links[R] == current] = child;
    }
    child->links[rotation] = current;
    current->list_start->parent = child;
    // Make sure to adjust lineage due to rotation for the path.
    path[path_len - 1] = child;
    path[path_len] = current;
}


/* * * * * * * * * *    Red-Black Tree Insertion Helper Function   * * * * * * * * * * */


/* @brief add_duplicate  this implementation stores duplicate nodes in a linked list to prevent the
 *                       rotation of duplicates in the tree. This adds the duplicate node to the
 *                       linked list of the node already present.
 * @param *head          the node currently organized in the tree. We will add to its list.
 * @param *to_add        the node to add to the linked list.
 */
void add_duplicate(tree_node_t *head, duplicate_t *add, tree_node_t *parent) {
    add->header = head->header;
    // The first node in the list can store the tree nodes parent for faster coalescing later.
    if (head->list_start == free_nodes.list_tail) {
        add->parent = parent;
    } else {
        add->parent = head->list_start->parent;
        head->list_start->parent = NULL;
    }
    // Get the first next node in the doubly linked list, invariant and correct its left field.
    head->list_start->links[P] = add;
    add->links[N] = head->list_start;
    head->list_start = add;
    add->links[P] = (duplicate_t*)head;
}


/* * * * * * * * * *     Red-Black Tree Insertion Logic     * * * * * * * * * * */


/* @brief fix_rb_insert  implements a modified Cormen et.al. red black fixup after the insertion of
 *                       a new node. Unifies the symmetric left and right cases with the use of
 *                       an array and an enum tree_link_t.
 * @param *path[]        the path representing the route down to the node we have inserted.
 * @param path_len       the length of the path.
 */
void fix_rb_insert(tree_node_t *path[], int path_len) {
    // We always place the black_nil at 0th index in the stack as root's parent so this is safe.
    while(extract_color(path[path_len - 2]->header) == RED) {
        tree_node_t *current = path[path_len - 1];
        tree_node_t *parent = path[path_len - 2];
        tree_node_t *grandparent = path[path_len - 3];

        // We can store the case we need to complete and its opposite rather than repeat code.
        tree_link_t symmetric_case = grandparent->links[R] == parent;
        tree_node_t *aunt = grandparent->links[!symmetric_case];
        if (extract_color(aunt->header) == RED) {
            paint_node(aunt, BLACK);
            paint_node(parent, BLACK);
            paint_node(grandparent, RED);
            current = grandparent;
            path_len -= 2;
        } else {
            if (current == parent->links[!symmetric_case]) {
                current = parent;
                tree_node_t *other_child = current->links[!symmetric_case];
                rotate(symmetric_case, current, path, path_len - 1);
                parent = other_child;
            }
            paint_node(parent, BLACK);
            paint_node(grandparent, RED);
            rotate(!symmetric_case, grandparent, path, path_len - 2);
            path_len--;
        }
    }
    paint_node(free_nodes.tree_root, BLACK);
}

/* @brief insert_rb_node  a modified insertion with additional logic to add duplicates if the
 *                        size in bytes of the block is already in the tree.
 * @param *current        we must insert to tree or add to a list as duplicate.
 */
void insert_rb_node(tree_node_t *current) {
    size_t current_key = extract_block_size(current->header);

    tree_node_t *path[MAX_TREE_HEIGHT];
    // This simplifies our insertion fixup logic later. See loop in the fixup function.
    path[0] = free_nodes.black_nil;
    int path_len = 1;
    tree_node_t *seeker = free_nodes.tree_root;
    size_t parent_size = 0;
    while (seeker != free_nodes.black_nil) {
        path[path_len++] = seeker;

        parent_size = extract_block_size(seeker->header);
        // Ability to add duplicates to linked list means no fixups necessary if duplicate.
        if (current_key == parent_size) {
            add_duplicate(seeker, (duplicate_t *)current, path[path_len - 2]);
            return;
        }
        // You may see this idiom throughout. L(0) if key fits in tree to left, R(1) if not.
        seeker = seeker->links[parent_size < current_key];
    }
    tree_node_t *parent = path[path_len - 1];
    if (parent == free_nodes.black_nil) {
        free_nodes.tree_root = current;
    } else {
        parent->links[parent_size < current_key] = current;
    }
    current->links[L] = free_nodes.black_nil;
    current->links[R] = free_nodes.black_nil;
    // Store the doubly linked duplicates in list. list_tail aka black_nil is the dummy tail.
    current->list_start = free_nodes.list_tail;
    paint_node(current, RED);
    path[path_len++] = current;
    fix_rb_insert(path, path_len);
}


/* * * * * * * * * *    Red-Black Tree Deletion Helper Functions   * * * * * * * * * * */


/* @brief rb_transplant  replaces node with the appropriate node to start balancing the tree.
 * @param *replacement   the node that will fill the deleted position. It can be black_nil.
 * @warning              this function modifies the stack.
 */
void rb_transplant(tree_node_t *replacement, tree_node_t *path[], int path_len) {
    tree_node_t *parent = path[path_len - 2];
    tree_node_t *remove = path[path_len - 1];
    if (parent == free_nodes.black_nil) {
        free_nodes.tree_root = replacement;
    } else {
        parent->links[ parent->links[R] == remove ] = replacement;
    }
    if (replacement != free_nodes.black_nil) {
        replacement->list_start->parent = parent;
    }
    // Delete the removed node from the path to give back correct path to fixup function.
    path[path_len - 1] = replacement;
}

/* @brief delete_duplicate  will remove a duplicate node from the tree when the request is coming
 *                          from a call from malloc. Address of duplicate does not matter so we
 *                          remove the first node from the linked list.
 * @param *head             We know this node has a next node and it must be removed for malloc.
 */
tree_node_t *delete_duplicate(tree_node_t *head) {
    duplicate_t *next_node = head->list_start;
    /* Take care of the possible node to the right in the doubly linked list first. This could be
     * another node or it could be free_nodes.list_tail, it does not matter either way.
     */
    next_node->links[N]->parent = next_node->parent;
    next_node->links[N]->links[P] = (duplicate_t *)head;
    head->list_start = next_node->links[N];
    return (tree_node_t *)next_node;
}


/* * * * * * * * * *      Red-Black Tree Deletion Logic     * * * * * * * * * * */


/* @brief fix_rb_delete  completes a unified Cormen et.al. fixup function. Uses a direction enum
 *                       and an array to help unify code paths based on direction and opposites.
 * @param *extra_black   the current node that was moved into place from the previous delete. It
 *                       may have broken rules of the tree or thrown off balance.
 * @param *path[]        the path representing the route down to the node we have inserted.
 * @param path_len       the length of the path.
 */
void fix_rb_delete(tree_node_t *extra_black, tree_node_t *path[], int path_len) {
    // The extra_black is "doubly black" if we enter the loop, requiring repairs.
    while (extra_black != free_nodes.tree_root && extract_color(extra_black->header) == BLACK) {
        tree_node_t *parent = path[path_len - 2];

        // We can cover left and right cases in one with simple directional link and its opposite.
        tree_link_t symmetric_case = parent->links[R] == extra_black;

        tree_node_t *sibling = parent->links[!symmetric_case];
        if (extract_color(sibling->header) == RED) {
            paint_node(sibling, BLACK);
            paint_node(parent, RED);
            rotate(symmetric_case, parent, path, path_len - 1);
            // Rotating a parent the same direction as the extra_black, moves it down the path.
            path[path_len++] = extra_black;
            sibling = parent->links[!symmetric_case];
        }
        if (extract_color(sibling->links[L]->header) == BLACK
                && extract_color(sibling->links[R]->header) == BLACK) {
            paint_node(sibling, RED);
            extra_black = path[path_len - 2];
            path_len--;
        } else {
            if (extract_color(sibling->links[!symmetric_case]->header) == BLACK) {
                paint_node(sibling->links[symmetric_case], BLACK);
                paint_node(sibling, RED);
                rotate(!symmetric_case, sibling, path, path_len);
                sibling = parent->links[!symmetric_case];
            }
            paint_node(sibling, extract_color(parent->header));
            paint_node(parent, BLACK);
            paint_node(sibling->links[!symmetric_case], BLACK);
            rotate(symmetric_case, parent, path, path_len - 1);
            extra_black = free_nodes.tree_root;
        }
    }
    // The extra_black has reached a red node, making it "red-and-black", or the root. Paint BLACK.
    paint_node(extra_black, BLACK);
}

/* @brief delete_rb_node  performs the necessary steps to have a functional, balanced tree after
 *                        deletion of any node in the free.
 * @param *remove         the node to remove from the tree from a call to malloc or coalesce.
 * @param *path[]         the path representing the route down to the node we have inserted.
 * @param path_len        the length of the path.
 */
tree_node_t *delete_rb_node(tree_node_t *remove, tree_node_t *path[], int path_len) {
    rb_color_t fixup_color_check = extract_color(remove->header);

    tree_node_t *parent = path[path_len - 2];
    tree_node_t *extra_black = NULL;
    if (remove->links[L] == free_nodes.black_nil || remove->links[R] == free_nodes.black_nil) {
        tree_link_t nil_link = remove->links[L] != free_nodes.black_nil;
        extra_black = remove->links[!nil_link];
        rb_transplant(extra_black, path, path_len);
    } else {
        int len_removed_node = path_len;
        // Warning, path_len may have changed.
        tree_node_t *right_min = tree_minimum(remove->links[R], path, &path_len);
        fixup_color_check = extract_color(right_min->header);

        extra_black = right_min->links[R];
        if (right_min != remove->links[R]) {
            rb_transplant(extra_black, path, path_len);
            right_min->links[R] = remove->links[R];
            right_min->links[R]->list_start->parent = right_min;
        } else {
            path[path_len - 1] = extra_black;
        }
        rb_transplant(right_min, path, len_removed_node);
        right_min->links[L] = remove->links[L];
        right_min->links[L]->list_start->parent = right_min;
        right_min->list_start->parent = parent;
        paint_node(right_min, extract_color(remove->header));
    }
    // Nodes can only be red or black, so we need to get rid of "extra" black by fixing tree.
    if (fixup_color_check == BLACK) {
        fix_rb_delete(extra_black, path, path_len);
    }
    return remove;
}

/* @brief find_best_fit  a red black tree is well suited to best fit search in O(logN) time. We
 *                       will find the best fitting node possible given the options in our tree.
 * @param key            the size_t number of bytes we are searching for in our tree.
 * @return               the pointer to the tree_node_t that is the best fit for our need.
 */
tree_node_t *find_best_fit(size_t key) {
    tree_node_t *path[MAX_TREE_HEIGHT];
    path[0] = free_nodes.black_nil;
    int path_len = 1;
    int len_to_best_fit = 1;

    tree_node_t *seeker = free_nodes.tree_root;
    // We will use this sentinel to start our competition while we search for best fit.
    size_t best_fit_size = ULLONG_MAX;
    tree_node_t *remove = seeker;
    while (seeker != free_nodes.black_nil) {
        size_t seeker_size = extract_block_size(seeker->header);
        path[path_len++] = seeker;
        if (key == seeker_size) {
            remove = seeker;
            len_to_best_fit = path_len;
            break;
        }
        tree_link_t search_direction = seeker_size < key;
        /* The key is less than the current found size but let's remember this size on the way down
         * as a candidate for the best fit. The closest fit will have won when we reach the bottom.
         */
        if (search_direction == L && seeker_size < best_fit_size) {
            remove = seeker;
            best_fit_size = seeker_size;
            len_to_best_fit = path_len;
        }
        seeker = seeker->links[search_direction];
    }

    if (remove->list_start != free_nodes.list_tail) {
        // We will keep remove in the tree and just get the first node in doubly linked list.
        return delete_duplicate(remove);
    }
    return delete_rb_node(remove, path, len_to_best_fit);
}

/* @brief remove_head  frees the head of a doubly linked list of duplicates which is a node in a
 *                     red black tree. The next duplicate must become the new tree node and the
 *                     parent and children must be adjusted to track this new node.
 * @param head         the node in the tree that must now be coalesced.
 * @param lft_child    if the left child has a duplicate, it tracks the new tree node as parent.
 * @param rgt_child    if the right child has a duplicate, it tracks the new tree node as parent.
 */
void remove_head(tree_node_t *head, tree_node_t *lft_child, tree_node_t *rgt_child) {
    // Store the parent in an otherwise unused field for a major O(1) coalescing speed boost.
    tree_node_t *tree_parent = head->list_start->parent;
    head->list_start->header = head->header;
    head->list_start->links[N]->parent = head->list_start->parent;

    tree_node_t *new_tree_node = (tree_node_t *)head->list_start;
    new_tree_node->list_start = head->list_start->links[N];
    new_tree_node->links[L] = lft_child;
    new_tree_node->links[R] = rgt_child;

    // We often write to fields of the black_nil, invariant. DO NOT use the nodes it stores!
    if (lft_child != free_nodes.black_nil) {
        lft_child->list_start->parent = new_tree_node;
    }
    if (rgt_child != free_nodes.black_nil) {
        rgt_child->list_start->parent = new_tree_node;
    }
    if (tree_parent == free_nodes.black_nil) {
        free_nodes.tree_root = new_tree_node;
    } else {
        tree_parent->links[tree_parent->links[R] == head] = new_tree_node;
    }
}

/* @brief free_coalesced_node  a specialized version of node freeing when we find a neighbor we
 *                             need to free from the tree before absorbing into our coalescing. If
 *                             this node is a duplicate we can splice it from a linked list.
 * @param *to_coalesce         the address for a node we must treat as a list or tree node.
 * @return                     the node we have now correctly freed given all cases to find it.
 */
void *free_coalesced_node(void *to_coalesce) {
    tree_node_t *tree_node = to_coalesce;
    // Go find and fix the node the normal way if it is unique.
    if (tree_node->list_start == free_nodes.list_tail) {
       return find_best_fit(extract_block_size(tree_node->header));
    }
    duplicate_t *list_node = to_coalesce;
    tree_node_t *lft_tree_node = tree_node->links[L];

    // Coalescing the first node in linked list. Dummy head, aka lft_tree_node, is to the left.
    if (lft_tree_node != free_nodes.black_nil && lft_tree_node->list_start == to_coalesce) {
        list_node->links[N]->parent = list_node->parent;
        lft_tree_node->list_start = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

    // All nodes besides the tree node head and the first duplicate node have parent set to NULL.
    }else if (NULL == list_node->parent) {
        list_node->links[P]->links[N] = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

    // Coalesce head of a doubly linked list in the tree. Remove and make a new head.
    } else {
        remove_head(tree_node, lft_tree_node, tree_node->links[R]);
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
bool is_left_space(const tree_node_t *node) {
    return !(node->header & LEFT_ALLOCATED);
}

/* @brief get_right_neighbor  gets the address of the next tree_node_t in the heap to the right.
 * @param *current            the tree_node_t we start at to then jump to the right.
 * @param payload             the size in bytes as a size_t of the current tree_node_t block.
 * @return                    the tree_node_t to the right of the current.
 */
tree_node_t *get_right_neighbor(const tree_node_t *current, size_t payload) {
    return (tree_node_t *)((byte_t *)current + HEADERSIZE + payload);
}

/* @brief *get_left_neighbor  uses the left block size gained from the footer to move to the header.
 * @param *node               the current header at which we reside.
 * @param left_block_size     the space of the left block as reported by its footer.
 * @return                    a header_t pointer to the header for the block to the left.
 */
tree_node_t *get_left_neighbor(const tree_node_t *node) {
    header_t *left_footer = (header_t *)((byte_t *)node - HEADERSIZE);
    return (tree_node_t *)((byte_t *)node - (*left_footer & SIZE_MASK) - HEADERSIZE);
}

/* @brief init_header_size  initializes any node as the size and indicating left is allocated. Left
 *                          is allocated because we always coalesce left and right.
 * @param *node             the region of possibly uninitialized heap we must initialize.
 * @param payload           the payload in bytes as a size_t of the current block we initialize
 */
void init_header_size(tree_node_t *node, size_t payload) {
    node->header = LEFT_ALLOCATED | payload;
}

/* @brief get_client_space  steps into the client space just after the header of a tree_node_t.
 * @param *node_header      the tree_node_t we start at before retreiving the client space.
 * @return                  the void* address of the client space they are now free to use.
 */
void *get_client_space(const tree_node_t *node_header) {
    return (byte_t *) node_header + HEADERSIZE;
}

/* @brief get_heap_node  steps to the tree_node_t header from the space the client was using.
 * @param *client_space  the void* the client was using for their type. We step to the left.
 * @return               a pointer to the tree_node_t of our heap block.
 */
tree_node_t *get_heap_node(const void *client_space) {
    return (tree_node_t *)((byte_t *) client_space - HEADERSIZE);
}

/* @brief init_footer  initializes footer at end of the heap block to matcht the current header.
 * @param *node        the current node with a header field we will use to set the footer.
 */
void init_footer(const tree_node_t *node, size_t payload) {
    header_t *footer = (header_t *)((byte_t *)node + payload);
    *footer = node->header;
}


/* * * * * * * * * * * *    Heap Helper Function    * * * * * * * * * */


/* @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
 * @param *to_free        the heap_node to add to the red black tree.
 * @param block_size      the size we use to initialize the node and find the right place in tree.
 */
void init_free_node(tree_node_t *to_free, size_t block_size) {
    to_free->header = LEFT_ALLOCATED | block_size;
    to_free->header |= RED_PAINT;
    to_free->list_start = free_nodes.list_tail;
    // Block sizes don't include header size so this is safe addition.
    header_t *footer = (header_t *)((byte_t *)to_free + block_size);
    *footer = to_free->header;
    tree_node_t *neighbor = (tree_node_t *)((byte_t *) footer + HEADERSIZE);
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

    // This helps us clarify if we are referring to tree or duplicates in a list. Use same address.
    free_nodes.list_tail = heap.client_end;
    free_nodes.black_nil = heap.client_end;
    free_nodes.black_nil->header = 1UL;
    paint_node(free_nodes.black_nil, BLACK);

    free_nodes.tree_root = heap.client_start;
    init_header_size(free_nodes.tree_root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE);
    paint_node(free_nodes.tree_root, BLACK);
    init_footer(free_nodes.tree_root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE);
    free_nodes.tree_root->links[L] = free_nodes.black_nil;
    free_nodes.tree_root->links[R] = free_nodes.black_nil;
    free_nodes.tree_root->list_start = free_nodes.list_tail;
    return true;
}

/* @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
 *                      split, it will add the newly freed split block to the free red black tree.
 * @param *free_block   a pointer to the node for a free block in its entirety.
 * @param request       the user request for space.
 * @param block_space   the entire space that we have to work with.
 * @return              a void pointer to generic space that is now ready for the client.
 */
void *split_alloc(tree_node_t *free_block, size_t request, size_t block_space) {
    tree_node_t *neighbor = NULL;
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
        tree_node_t *found_node = find_best_fit(client_request);
        return split_alloc(found_node, client_request, extract_block_size(found_node->header));
    }
    return NULL;
}

/* @brief coalesce        attempts to coalesce left and right if the left and right tree_node_t
 *                        are free. Runs the search to free the specific free node in O(logN) + d
 *                        where d is the number of duplicate nodes of the same size.
 * @param *leftmost_node  the current node that will move left if left is free to coalesce.
 * @return                the leftmost node from attempts to coalesce left and right. The leftmost
 *                        node is initialized to reflect the correct size for the space it now has.
 * @warning               this function does not overwrite the data that may be in the middle if we
 *                        expand left and write. The user may wish to move elsewhere if reallocing.
 */
tree_node_t *coalesce(tree_node_t *leftmost_node) {
    // What if your left or right free node to coalesce is a repeat?
    // It may not be the first node in the repeat list.
    size_t coalesced_space = extract_block_size(leftmost_node->header);
    tree_node_t *rightmost_node = get_right_neighbor(leftmost_node, coalesced_space);
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
    tree_node_t *old_node = get_heap_node(old_ptr);
    size_t old_size = extract_block_size(old_node->header);

    tree_node_t *leftmost_node = coalesce(old_node);
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
        tree_node_t *to_insert = get_heap_node(ptr);
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
    tree_node_t *cur_node = heap.client_start;
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
int get_black_height(const tree_node_t *root) {
    if (root == free_nodes.black_nil) {
        return 0;
    }
    if (extract_color(root->links[L]->header) == BLACK) {
        return 1 + get_black_height(root->links[L]);
    }
    return get_black_height(root->links[L]);
}

/* @brief get_tree_height  gets the max height in terms of all nodes of the tree.
 * @param *root            the root to start at to measure the height of the tree.
 * @return                 the int of the max height of the tree.
 */
int get_tree_height(const tree_node_t *root) {
    if (root == free_nodes.black_nil) {
        return 0;
    }
    int left_height = 1 + get_tree_height(root->links[L]);
    int right_height = 1 + get_tree_height(root->links[R]);
    return left_height > right_height ? left_height : right_height;
}

/* @brief is_red_red  determines if a red red violation of a red black tree has occured.
 * @param *root       the current root of the tree to begin at for checking all subtrees.
 */
bool is_red_red(const tree_node_t *root) {
    if (root == free_nodes.black_nil ||
            (root->links[R] == free_nodes.black_nil
             && root->links[L] == free_nodes.black_nil)) {
        return false;
    }
    if (extract_color(root->header) == RED) {
        if (extract_color(root->links[L]->header) == RED
                || extract_color(root->links[R]->header) == RED) {
            return true;
        }
    }
    // Check all the subtrees.
    return is_red_red(root->links[R]) || is_red_red(root->links[L]);
}

/* @brief calculate_bheight  determines if every path from a node to the free.black_nil has the
 *                           same number of black nodes.
 * @param *root              the root of the tree to begin searching.
 * @return                   -1 if the rule was not upheld, the black height if the rule is held.
 */
int calculate_bheight(const tree_node_t *root) {
    if (root == free_nodes.black_nil) {
        return 0;
    }
    int lf_bheight = calculate_bheight(root->links[L]);
    int rt_bheight = calculate_bheight(root->links[R]);
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
bool is_bheight_valid(const tree_node_t *root) {
    return calculate_bheight(root) != -1;
}

/* @brief extract_tree_mem  sums the total memory in the red black tree to see if it matches the
 *                          total memory we got from traversing blocks of the heap.
 * @param *root             the root to start at for the summing recursive search.
 * @return                  the total memory in bytes as a size_t in the red black tree.
 */
size_t extract_tree_mem(const tree_node_t *root) {
    if (root == free_nodes.black_nil) {
        return 0UL;
    }
    size_t total_mem = extract_tree_mem(root->links[R])
                       + extract_tree_mem(root->links[L]);
    // We may have repeats so make sure to add the linked list values.
    size_t node_size = extract_block_size(root->header) + HEADERSIZE;
    total_mem += node_size;
    duplicate_t *tally_list = root->list_start;
    if (tally_list != free_nodes.list_tail) {
        // We have now entered a doubly linked list that uses left(prev) and right(next).
        while (tally_list != free_nodes.list_tail) {
            total_mem += node_size;
            tally_list = tally_list->links[N];
        }
    }
    return total_mem;
}

/* @brief is_rbtree_mem_valid  a wrapper for tree memory sum function used to check correctness.
 * @param *root                the root node to begin at for the recursive summing search.
 * @return                     true if the totals match false if they do not.
 */
bool is_rbtree_mem_valid(const tree_node_t *root, size_t total_free_mem) {
    return extract_tree_mem(root) == total_free_mem;
}

/* @brief calculate_bheight_V2  verifies that the height of a red-black tree is valid. This is a
 *                              similar function to calculate_bheight but comes from a more
 *                              reliable source, because I saw results that made me doubt V1.
 */
int calculate_bheight_V2(const tree_node_t *root) {
    if (root == free_nodes.black_nil) {
        return 1;
    }
    tree_node_t *left = root->links[L];
    tree_node_t *right = root->links[R];
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
 * @return                     true if the paths are valid, false if not.
 */
bool is_bheight_valid_V2(const tree_node_t *root) {
    return calculate_bheight_V2(free_nodes.tree_root) != 0;
}

/* @brief is_binary_tree  confirms the validity of a binary search tree. Nodes to the left should
 *                        be less than the root and nodes to the right should be greater.
 * @param *root           the root of the tree from which we examine children.
 * @return                true if the tree is valid, false if not.
 */
bool is_binary_tree(const tree_node_t *root) {
    if (root == free_nodes.black_nil) {
        return true;
    }
    size_t root_value = extract_block_size(root->header);
    if (root->links[L] != free_nodes.black_nil
            && root_value < extract_block_size(root->links[L]->header)) {
        return false;
    }
    if (root->links[R] != free_nodes.black_nil
            && root_value > extract_block_size(root->links[R]->header)) {
        return false;
    }
    return is_binary_tree(root->links[L]) && is_binary_tree(root->links[R]);
}

/* @brief is_duplicate_storing_parent  confirms that if a duplicate node is present it accurately
 *                                     stores the parent so that all duplicate nodes are coalesced
 *                                     in O(1) time, garunteed. Parent may change during rotations
 *                                     so good tracking is critical.
 * @param *root                        the current node under consideration.
 * @param *parent                      the parent of the current node.
 * @return                             true if we store the parent in first duplicate list node.
 */
bool is_duplicate_storing_parent(const tree_node_t *root, const tree_node_t *parent) {
    if (root == free_nodes.black_nil) {
        return true;
    }
    if (root->list_start != free_nodes.list_tail && root->list_start->parent != parent) {
        breakpoint();
        return false;
    }
    return is_duplicate_storing_parent(root->links[L], root)
             && is_duplicate_storing_parent(root->links[R], root);
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
    if (!is_rbtree_mem_valid(free_nodes.tree_root, total_free_mem)) {
        breakpoint();
        return false;
    }
    // Two red nodes in a row are invalid for the table.
    if (is_red_red(free_nodes.tree_root)) {
        breakpoint();
        return false;
    }
    // Does every path from a node to the black sentinel contain the same number of black nodes.
    if (!is_bheight_valid(free_nodes.tree_root)) {
        breakpoint();
        return false;
    }

    // This comes from a more official write up on red black trees so I included it.
    if (!is_bheight_valid_V2(free_nodes.tree_root)) {
        breakpoint();
        return false;
    }
    if (!is_binary_tree(free_nodes.tree_root)) {
        breakpoint();
        return false;
    }
    if (!is_duplicate_storing_parent(free_nodes.tree_root, free_nodes.black_nil)) {
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
void print_node(const tree_node_t *root) {
    size_t block_size = extract_block_size(root->header);
    extract_color(root->header) == BLACK ? printf(COLOR_BLK) : printf(COLOR_RED);
    printf("%p:", root);
    printf("(%zubytes)", block_size);
    printf(COLOR_NIL);
    // print the black-height
    printf("(bh: %d)", get_black_height(root));
    printf(COLOR_CYN);
    // If a node is a duplicate, we will give it a special mark among nodes.
    if (root->list_start != free_nodes.list_tail) {
        int duplicates = 1;
        duplicate_t *duplicate = root->list_start;
        for (;(duplicate = duplicate->links[N]) != free_nodes.list_tail; duplicates++) {
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
void print_inner_tree(const tree_node_t *root, const char *prefix,
                      const print_node_t node_type, const tree_link_t dir) {
    if (root == free_nodes.black_nil) {
        return;
    }
    // Print the root node
    printf("%s", prefix);
    printf("%s", node_type == LEAF ? " └──" : " ├──");
    printf(COLOR_CYN);
    dir == L ? printf("L:") : printf("R:");
    printf(COLOR_NIL);
    print_node(root);

    // Print any subtrees
    char *str = NULL;
    int string_length = snprintf(NULL, 0, "%s%s", prefix, node_type == LEAF ? "     " : " │   ");
    if (string_length > 0) {
        str = malloc(string_length + 1);
        snprintf(str, string_length, "%s%s", prefix, node_type == LEAF ? "     " : " │   ");
    }
    if (str != NULL) {
        if (root->links[R] == free_nodes.black_nil) {
            print_inner_tree(root->links[L], str, LEAF, L);
        } else if (root->links[L] == free_nodes.black_nil) {
            print_inner_tree(root->links[R], str, LEAF, R);
        } else {
            print_inner_tree(root->links[R], str, BRANCH, R);
            print_inner_tree(root->links[L], str, LEAF, L);
        }
    } else {
        printf(COLOR_ERR "memory exceeded. Cannot display tree." COLOR_NIL);
    }
    free(str);
}

/* @brief print_rb_tree  prints the contents of an entire rb tree in a directory tree style.
 * @param *root          the root node to begin at for printing recursively.
 */
void print_rb_tree(const tree_node_t *root) {
    if (root == free_nodes.black_nil) {
        return;
    }
    // Print the root node
    printf(" ");
    print_node(root);

    // Print any subtrees
    if (root->links[R] == free_nodes.black_nil) {
        print_inner_tree(root->links[L], "", LEAF, L);
    } else if (root->links[L] == free_nodes.black_nil) {
        print_inner_tree(root->links[R], "", LEAF, R);
    } else {
        print_inner_tree(root->links[R], "", BRANCH, R);
        print_inner_tree(root->links[L], "", LEAF, L);
    }
}

/* @brief print_alloc_block  prints the contents of an allocated block of memory.
 * @param *node              a valid tree_node_t to a block of allocated memory.
 */
void print_alloc_block(const tree_node_t *node) {
    size_t block_size = extract_block_size(node->header);
    // We will see from what direction our header is messed up by printing 16 digits.
    printf(COLOR_GRN "%p: HDR->0x%016zX(%zubytes)\n"
            COLOR_NIL, node, node->header, block_size);
}

/* @brief print_free_block  prints the contents of a free block of heap memory.
 * @param *header           a valid header to a block of allocated memory.
 */
void print_free_block(const tree_node_t *node) {
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
    // Printing color logic will help us spot red black violations. Tree printing later helps too.
    printf("%*c", indent_struct_fields, ' ');
    if (node->links[L]) {
        printf(extract_color(node->links[L]->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("LFT->%p\n", node->links[L]);
    } else {
        printf("LFT->%p\n", NULL);
    }
    printf(COLOR_NIL);
    printf("%*c", indent_struct_fields, ' ');
    if (node->links[R]) {
        printf(extract_color(node->links[R]->header) == BLACK ? COLOR_BLK : COLOR_RED);
        printf("RGT->%p\n", node->links[R]);
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
void print_error_block(tree_node_t *node, size_t block_size) {
    printf("\n%p: HDR->0x%016zX->%zubyts\n",
            node, node->header, block_size);
    printf("Block size is too large and header is corrupted.\n");
}

/* @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
 *                        notice where we went wrong and what the addresses were.
 * @param *current        the current node that is likely garbage values that don't make sense.
 * @param *prev           the previous node that we jumped from.
 */
void print_bad_jump(const tree_node_t *current, const tree_node_t *prev) {
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
    print_rb_tree(free_nodes.tree_root);
}

/* @brief dump_tree  prints just the tree with addresses, colors, black heights, and whether a
 *                   node is a duplicate or not. Duplicates are marked with an asterisk and have
 *                   a node in their next field.
 */
void dump_tree() {
    printf(COLOR_CYN "(+X)" COLOR_NIL);
    printf(" INDICATES DUPLICATE NODES IN THE TREE. THEY HAVE A N NODE.\n");
    print_rb_tree(free_nodes.tree_root);
}


/* * * * * * * * * * * *   Printing Debugger   * * * * * * * * * * */


/* @brief dump_heap  prints out the complete status of the heap, all of its blocks, and the sizes
 *                   the blocks occupy. Printing should be clean with no overlap of unique id's
 *                   between heap blocks or corrupted headers.
 */
void dump_heap() {
    tree_node_t *node = heap.client_start;
    printf("Heap client segment starts at address %p, ends %p. %zu total bytes currently used.\n",
            node, heap.client_end, heap.heap_size);
    printf("A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n");
    printf("COLOR KEY: "
            COLOR_BLK "[BLACK NODE] " COLOR_NIL
            COLOR_RED "[RED NODE] " COLOR_NIL
            COLOR_GRN "[ALLOCATED BLOCK]" COLOR_NIL "\n\n");

    printf("%p: START OF HEAP. HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", heap.client_start);
    tree_node_t *prev = node;
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
    extract_color(free_nodes.black_nil->header) == BLACK ? printf(COLOR_BLK) : printf(COLOR_RED);
    printf("%p: BLACK NULL HDR->0x%016zX\n" COLOR_NIL,
            free_nodes.black_nil, free_nodes.black_nil->header);
    printf("%p: FINAL ADDRESS", (byte_t *)heap.client_end + HEAP_NODE_WIDTH);
    printf("\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n");
    printf("COLOR KEY: "
            COLOR_BLK "[BLACK NODE] " COLOR_NIL
            COLOR_RED "[RED NODE] " COLOR_NIL
            COLOR_GRN "[ALLOCATED BLOCK]" COLOR_NIL "\n\n");

    printf("\nRED BLACK TREE OF FREE NODES AND BLOCK SIZES.\n");
    printf("HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n");
    printf(COLOR_CYN "(+X)" COLOR_NIL);
    printf(" INDICATES DUPLICATE NODES IN THE TREE. THEY HAVE A N NODE.\n");
    print_rb_tree(free_nodes.tree_root);
}

