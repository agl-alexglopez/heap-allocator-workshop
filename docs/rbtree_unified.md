# Unified Allocator

|Section|Writeup|Code|
|---           |---    |--- |
|1. Home|**[`README.md`](/README.md)**||
|2. The CLRS Standard|**[`rbtree_clrs.md`](/docs/rbtree_clrs.md)**|**[`rbtree_clrs.c`](/src/rbtree_clrs.c)**|
|3. Doubly Linked Duplicates|**[`rbtree_linked.md`](/docs/rbtree_linked.md)**|**[`rbtree_linked.c`](/src/rbtree_linked.c)**|
|4. Stack Based|**[`rbtree_stack.md`](/docs/rbtree_stack.md)**|**[`rbtree_stack.c`](/src/rbtree_stack.c)**|
|5. Topdown Fixups|**[`rbtree_topdown.md`](/docs/rbtree_topdown.md)**|**[`rbtree_topdown.c`](/src/rbtree_topdown.c)**|
|6. Runtime Analysis|**[`rbtree_analysis.md`](/docs/rbtree_analysis.md)**||

## Overview

The downside to the CLRS implementation of a Red Black Tree is that there are many cases to consider, and for the major fixup operations after inserting and deleting, we have to consider two symmetric cases. It would be much more readable code if we could unite these cases and functions into the minimum possible lines of code. A great way to do this is with an array and an enum!

The traditional red-black node in the context of a heap allocator could look something like this.

```c
typedef struct heap_node_t {
    // The header will store block size, allocation status, left neighbor status, and node color.
    header_t header;
    struct heap_node_t *parent;
    struct heap_node_t *left;
    struct heap_node_t *right;
}heap_node_t;
```

If we use an array for the left and right fields in the struct and pair that with smart use of an enum, we can do some clever tricks to unify left and right cases to simple logic points in the code. Here is the new setup.

```c
typedef struct heap_node_t {
    // The header will store block size, allocation status, left neighbor status, and node color.
    header_t header;
    struct heap_node_t *parent;
    struct heap_node_t *links[2];
}heap_node_t;

// NOT(!) operator will flip this enum to the opposite field. !LEFT == RIGHT and !RIGHT == LEFT;
typedef enum direction_t {
    // These represent indices in the lr_array array of a heap_node_t node in the tree.
    LEFT = 0,
    RIGHT = 1
} direction_t;
```

With this new approach, your traditional search function in a binary tree can look slightly more elegant, in my opinion.

```c
/* @brief find_node_size  performs a standard binary search for a node based on the value of key.
 * @param key             the size_t representing the number of bytes a node must have in the tree.
 * @return                a pointer to the node with the desired size or NULL if size is not found.
 */
heap_node_t *find_node_size(size_t key) {
    heap_node_t *current = tree_root;

    while(current != black_sentinel) {
        size_t cur_size = extract_block_size(current->header);
        if (key == cur_size) {
            return current;
        }
        // Idiom for choosing direction. LEFT(0), RIGHT(1);
        direction_t search_direction = cur_size < key;
        current = current->links[search_direction];
    }
    return NULL;
}
```

This might not look like much but the saving grow substantially with more complex functions. Here is the CLRS left rotation function. We also have to write the right rotation function as a perfect mirror for left and right.

```c
/* @brief left_rotate  complete a left rotation to help repair a red-black tree. Assumes current is
 *                     not the black_sentinel and that the right child is not black sentinel.
 * @param *current     current will move down the tree, it's right child will move up to replace.
 * @warning            this function assumes current and current->right are not black_sentinel.
 */
void left_rotate(heap_node_t *current) {
    heap_node_t *right_child = current->right;
    current->right = right_child->left;
    if (right_child->left != black_sentinel) {
        right_child->left->parent = current;
    }
    right_child->parent = current->parent;
    // Take care of the root edgecase and find where the parent is in relation to current.
    if (current->parent == black_sentinel) {
        tree_root = right_child;
    } else if (current == current->parent->left) {
        current->parent->left = right_child;
    } else {
        current->parent->right = right_child;
    }
    right_child->left = current;
    current->parent = right_child;
}
```

Now, use some clever naming and the `direction_t` type and you can unify both rotation functions into one.

```c
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
    if (child->links[rotation] != black_sentinel) {
        child->links[rotation]->parent = current;
    }
    child->parent = current->parent;
    heap_node_t *parent = current->parent;
    if (parent == black_sentinel) {
        tree_root = child;
    } else {
        // Another idiom for direction. Think of how we can use True/False.
        direction_t parent_link = parent->links[RIGHT] == current;
        parent->links[parent_link] = child;
    }
    child->links[rotation] = current;
    current->parent = child;
}
```

How much more clear and readable this is may be a matter up for debate, but we can save a significant number of lines in later functions. Here is an impressive, but still digestible, amount of savings to skim over. The `fix_rb_insert` function is quite readable when unified. For even more savings, go look at the `fix_rb_delete` function in the implementations. That one is a little too long to post here.

Here is the CLRS version.

```c
/* @brief fix_rb_insert  implements Cormen et.al. red black fixup after the insertion of a node.
 *                       Ensures that the rules of a red-black tree are upheld after insertion.
 * @param *current       the current node that has just been added to the red black tree.
 */
void fix_rb_insert(heap_node_t *current) {
    while(extract_color(current->parent->header) == RED) {
        if (current->parent == current->parent->parent->left) {
            heap_node_t *uncle = current->parent->parent->right;
            if (extract_color(uncle->header) == RED) {
                paint_node(current->parent, BLACK);
                paint_node(uncle, BLACK);
                paint_node(current->parent->parent, RED);
                current = current->parent->parent;
            } else {
                if (current == current->parent->right) {
                    current = current->parent;
                    left_rotate(current);
                }
                paint_node(current->parent, BLACK);
                paint_node(current->parent->parent, RED);
                right_rotate(current->parent->parent);
            }
        } else {
            heap_node_t *uncle = current->parent->parent->left;
            if (extract_color(uncle->header) == RED) {
                paint_node(current->parent, BLACK);
                paint_node(uncle, BLACK);
                paint_node(current->parent->parent, RED);
                current = current->parent->parent;
            } else {
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
    paint_node(tree_root, BLACK);
}
```

Here is the unified version.

```c
/* @brief fix_rb_insert  implements a modified Cormen et.al. red black fixup after the insertion of
 *                       a new node. Unifies the symmetric left and right cases with the use of
 *                       an array and an enum direction_t.
 * @param *current       the current node that has just been added to the red black tree.
 */
void fix_rb_insert(heap_node_t *current) {
    while(extract_color(current->parent->header) == RED) {
        heap_node_t *parent = current->parent;
        direction_t grandparent_link = parent->parent->links[RIGHT] == parent;
        direction_t opposite_link = !grandparent_link;
        heap_node_t *aunt = parent->parent->links[opposite_link];
        if (extract_color(aunt->header) == RED) {
            paint_node(aunt, BLACK);
            paint_node(parent, BLACK);
            paint_node(parent->parent, RED);
            current = parent->parent;
        } else {
            if (current == parent->links[opposite_link]) {
                current = current->parent;
                rotate(current, grandparent_link);
            }
            paint_node(current->parent, BLACK);
            paint_node(current->parent->parent, RED);
            rotate(current->parent->parent, opposite_link);
        }
    }
    paint_node(tree_root, BLACK);
}
```

This implementation seems to speed up the allocator slightly on my desktop running wsl2 Ubuntu and slow down slightly on my laptop running PopOS!. However, the core logic does not change and we have to do the same number of steps in each case. We save a few meaningful operations in some places, but the benefit is primarily in readability and reducing the places for a bug to hide in key functions by about half.

## Unified Analysis

- **`free()`**- Freeing an allocated block, thus inserting it into a red black tree, takes one $\Theta(lgN)$ search to find the place for a new node. The insertion fixup algorithm calls for a worst case of $O(lgN)$ fixup operations as we work back up the tree, giving us $\Theta(lgN) + \Theta(lgN)$, or just $\Theta(lgN)$ in big-O time complexity.
- **`malloc()`**- Searching the tree for the best fitting block requires one $\Theta(lgN)$ search of the tree. Deleting from a red black tree takes at most three rotations, but $\Theta(lgN)$ color adjustments on the way back up the tree in the worst case.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. When they do this, if they find a block they will be able to immediately free it from its position in the tree and start the fixup operations. This is because we have the memory address of the block in our heap, and thus our tree. So, we use the parent field to start fixups immediately, costing $\Theta(lgN)$ time.
- **`style`**- The style of this implementation is short, readable, and simple. The symmetric cases being unified helps make the key functions shorter to read. Thinking about the clearest way to name the `direction_t` and `links[]` pointer array operations for every function is key to helping others understand this more generalized code.

> **Read the writeup for the next allocator, [`rbtree_linked`](/docs/rbtree_linked.md)**.

