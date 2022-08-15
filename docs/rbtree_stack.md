# Unified Stack

## Navigation

|Section|Writeup|Code|
|---           |---    |--- |
|1. Home|**[`README.md`](/README.md)**||
|2. The CLRS Standard|**[`rbtree_clrs.md`](/docs/rbtree_clrs.md)**|**[`rbtree_clrs.c`](/src/rbtree_clrs.c)**|
|3. Unified Symmetry|**[`rbtree_unified.md`](/docs/rbtree_unified.md)**|**[`rbtree_unified.c`](/src/rbtree_unified.c)**|
|4. Doubly Linked Duplicates|**[`rbtree_linked.md`](/docs/rbtree_linked.md)**|**[`rbtree_linked.c`](/src/rbtree_linked.c)**|
|5. Topdown Fixups|**[`rbtree_topdown.md`](/docs/rbtree_topdown.md)**|**[`rbtree_topdown.c`](/src/rbtree_topdown.c)**|
|6. Runtime Analysis|**[`rbtree_analysis.md`](/src/rbtree_analysis.md)**||

## Overview

> **Read my code here ([`rbtree_stack.c`](/src/rbtree_stack.c)).**

If we now bring all of the strengths of the past improvements I have discussed, we can make a more space efficient, fast allocator that solves some of the challenges a heap allocator implementation poses for red black trees. One technique that is common when we want to save space in a red black tree is to eliminate the parent field. Instead, we use an array and treat it as a stack to represent the path down to the nodes we need to work with. This eliminates an eight byte pointer from the struct and in a normal red black tree might look something like this. Please note, I am using my array method for this implementation as well to eliminate symmetric cases.

```c
typedef struct heap_node_t {
    header_t header;
    struct heap_node_t *links[2];
}heap_node_t;
```

There is one problem with this approach for a heap allocator however. I could not think of a good way to coalesce nodes with the above struct. The problem arises with duplicates. When we insert duplicates into a red black tree, we know that they will always follow the rules of a binary search tree in relation to their parent, but we do not know where rotation may put them over the lifetime of the heap. This is a problem for a coalescing operation that needs to find the exact address of the node it needs.

When we search the tree by block size, as we normally do, there is no garuntee the first node of the correct size we find will be the one we are coalescing. This would result in a non sequential block of memory and corrupt the heap. So, we will simply take from our previous allocator, and store duplicates in a doubly linked list. Then we are free to use the stack, because we know that all nodes in the tree will be unique. We also have immediate access to coalesce any duplicate safely in constant time from its linked list. Here is the node. Here is the new tree scheme without a parent field. Now, we have a series of constant time checks we can use to tell where we are in the list and who the parent node is.

![rb-stack-scheme](/images/rb-duplicates-no-parent.png)

```c
typedef struct tree_node_t {
    header_t header;
    struct tree_node_t *links[TWO_NODE_ARRAY];
    struct duplicate_t *list_start;
}tree_node_t;

typedef struct duplicate_t {
    header_t header;
    struct duplicate_t *links[TWO_NODE_ARRAY];
    // We will always store the tree parent in first duplicate node in the list. O(1) coalescing.
    struct tree_node_t *parent;
} duplicate_t;

typedef enum tree_link_t {
    LEFT = 0,
    RIGHT = 1
} tree_link_t;

// We use these fields to refer to our doubly linked duplicate nodes.
typedef enum list_link_t {
    PREV = 0,
    NEXT = 1
} list_link_t;
```

We can acheive the same speed as the `rbtree_linked` allocator with this more space efficient implementation if we get creative. If we put a duplicate node in a linked list, it will have an extra field of the node going to waste. Why not create another node type, designated specifically for being a duplicate in a list, and have the first node in the linked list always store the parent? This way we acheive our garuntee of $\Theta(1)$ to coalesce any duplicate node. This is tricky to pull of but will produce a superior allocator in every way.

This was the most challenging of all the allocators for me to implement. It requires a strong handle on a spacial understanding of the changes that occur in a tree during rotations and transplants. We have to manage the array ourselves, accurately updating the nodes in the stack if they move or rotate during fixup operations and keep a good track of our index. It was challenging for me to apply the same structure of CLRS over a stack implmentation. I might recommend looking over the [`libval`](https://adtinfo.org/libavl.html/Red_002dBlack-Trees.html) implementation of a red black tree with a stack if you were to do this from scratch. It is a different approach and I struggled to read the code due to the naming convenients and lack of comments. However, I am sure it would be a good starting point to implement this technique. So, I stuck with the past implementations I had done and reworked them for this technique.

## Unified Stack Analysis

- **`free()`**- Freeing an allocated heap block takes one $\Theta(lgN)$ search to find the place for a new node. In this implementation however, we have a significant best case. If we find a node of the same size that we are inserting, we can insert it into the doubly linked list in $\Theta(1)$ time and we do not need to rotate or recolor the tree. If the node does not exist in the tree, we have the normal $\Theta(lgN)$ fixup operations.
- **`malloc()`**- Searching the tree for the best fitting block requires one $\Theta(lgN)$ search of the tree. Again, however, we can eliminate fixups if the best fit node is a duplicate. Then we have $\Theta(1)$ operations to take the duplicate from a doubly linked list.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. If the coalescing operation finds a duplicate node, it will be freed in $\Theta(1)$ time because I have setup the doubly linked list such that we know we are in the list and not the tree. We also have a node that perpetually serves as the tail to the list, its the same node we use for the null in the tree. This makes coalescing a duplicate a constant time operation. Because we always store the parent of a duplicate node in the first node of the linked list we can coalesce a duplicate in constant time. Great!
- **`style`**- This was a challenging implementation to complete, but it is able to loosely follow the structure of all other implementations up to this point. The greatest concern for this allocator is the readability of the stack management. To improve the style of this code it would be important to come up with a consistent method for rearranging and managing the stack during fixup operations. There are specific adjustments to the path in the stack that are required for rotations and deciding where and when to adjust the nodes in the array is an important style question. Also, tracking the parent node in duplicate nodes was very difficult. The effort is aided by the fact that we use the `black_nil` aka `list_tail` to help with edgcases, but there are still details to consider. However, the benefit is extreme speed and space optimizations.

> **Read the writeup for the next allocator, [`rbtree_topdown.md`](/docs/rbtree_topdown.md)**.

