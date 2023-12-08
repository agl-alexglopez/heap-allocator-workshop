# Unified Allocator

## Overview

The downside to the CLRS implementation of a Red Black Tree is that there are many cases to consider, and for the major fixup operations after inserting and deleting, we have to consider two symmetric cases. It would be much more readable code if we could unite these cases and functions into the minimum possible lines of code. A great way to do this is with an array and an enum!

The traditional red-black node in the context of a heap allocator could look something like this.

```c
typedef struct rb_node {
    header header;
    struct rb_node *parent;
    struct rb_node *left;
    struct rb_node *right;
}rb_node;
```

If we use an array for the left and right fields in the struct and pair that with smart use of an enum, we can do some clever tricks to unify left and right cases to simple logic points in the code. Here is the new setup.

```c
struct rb_node
{
    // The header will store block size, allocation status, left neighbor status, and node color.
    header header;
    struct rb_node *parent;
    struct rb_node *links[2];
    // A footer goes at end of unused blocks. Need at least 8 bytes of user space to fit footer.
};

// NOT(!) operator will flip this enum to the opposite field. !L == R and !R == L;
enum tree_link
{
    // (L == LEFT), (R == RIGHT)
    L = 0,
    R = 1
};
```

With this new approach, your traditional search function in a binary tree can look slightly more elegant, in my opinion.

```c
rb_node *find_node_size(size_t key) {
    rb_node *current = tree_root;
    while(current != black_sentinel) {
        size_t cur_size = get_size(current->header);
        if (key == cur_size) {
            return current;
        }
        // Idiom for choosing direction. LEFT(0), RIGHT(1);
        current = current->links[cur_size < key];
    }
    return NULL;
}
```

This might not look like much but the saving grow substantially with more complex functions. Here is the CLRS left rotation function. We also have to write the right rotation function as a perfect mirror for left and right.

```c
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
```

Now, use some clever naming and the `tree_link` type and you can unify both rotation functions into one.

```c
void rotate(rb_node *current, tree_link rotation) {
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
```

How much more clear and readable this is may be a matter up for debate, but we can save a significant number of lines in later functions. Here is an impressive, but still digestible, amount of savings to skim over. The `fix_rb_insert` function is quite readable when unified. For even more savings, go look at the `fix_rb_delete` function in the implementations. That one is a little too long to post here.

Here is the CLRS version.

```c
void fix_rb_insert(rb_node *current) {
    while(extract_color(current->parent->header) == RED) {
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
```

Here is the unified version.

```c
void fix_rb_insert(rb_node *current) {
    while(extract_color(current->parent->header) == RED) {
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
```

This implementation seems to speed up the allocator slightly on my desktop running wsl2 Ubuntu and slow down slightly on my laptop running PopOS!. However, the core logic does not change and we have to do the same number of steps in each case. We save a few meaningful operations in some places, but the benefit is primarily in readability and reducing the places for a bug to hide in key functions by about half.

## Unified Analysis

- **`free()`**- Freeing an allocated block, thus inserting it into a red black tree, takes one O(lgN) search to find the place for a new node. The insertion fixup algorithm calls for a worst case of O(lgN) fixup operations as we work back up the tree, giving us O(lgN) + O(lgN), or just O(lgN) in big-O time complexity.
- **`malloc()`**- Searching the tree for the best fitting block requires one O(lgN) search of the tree. Deleting from a red black tree takes at most three rotations, but O(lgN) color adjustments on the way back up the tree in the worst case.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. When they do this, if they find a block they will be able to immediately free it from its position in the tree and start the fixup operations. This is because we have the memory address of the block in our heap, and thus our tree. So, we use the parent field to start fixups immediately, costing O(lgN) time.
- **`style`**- The style of this implementation is short, readable, and simple. The symmetric cases being unified helps make the key functions shorter to read. Thinking about the clearest way to name the `tree_link` and `links[]` pointer array operations for every function is key to helping others understand this more generalized code.

