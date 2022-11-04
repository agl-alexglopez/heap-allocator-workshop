# Unified Stack

## Navigation

1. Home
   - Documentation **([`README.md`](/README.md))**
2. The CLRS Standard
   - Documentation **([`rbtree_clrs.md`](/docs/rbtree_clrs.md))**
   - Design **([`rbtree_clrs_design.h`](/lib/utility/rbtree_clrs_design.h))**
   - Implementation **([`rbtree_clrs.c`](/lib/rbtree_clrs.c))**
3. Unified Symmetry
   - Documentation **([`rbtree_unified.md`](/docs/rbtree_unified.md))**
   - Design **([`rbtree_unified_design.h`](/lib/utility/rbtree_unified_design.h))**
   - Implementation **([`rbtree_unified.c`](/lib/rbtree_unified.c))**
4. Doubly Linked Duplicates
   - Documentation **([`rbtree_linked.md`](/docs/rbtree_linked.md))**
   - Design **([`rbtree_linked_design.h`](/lib/utility/rbtree_linked_design.h))**
   - Implementation **([`rbtree_linked.c`](/lib/rbtree_linked.c))**
5. Stack Based
   - Documentation **([`rbtree_stack.md`](/docs/rbtree_stack.md))**
   - Design **([`rbtree_stack_design.h`](/lib/utility/rbtree_stack_design.h))**
   - Implementation **([`rbtree_stack.c`](/lib/rbtree_stack.c))**
6. Top-down Fixups
   - Documentation **([`rbtree_topdown.md`](/docs/rbtree_topdown.md))**
   - Design **([`rbtree_topdown_design.h`](/lib/utility/rbtree_topdown_design.h))**
   - Implementation **([`rbtree_topdown.c`](/lib/rbtree_topdown.c))**
7. List Allocators
   - Documentation **([`list_segregated.md`](/docs/list_segregated.md))**
   - Design **([`list_addressorder_design.h`](/lib/utility/list_addressorder_design.h))**
   - Implementation **([`list_addressorder.c`](/lib/list_addressorder.c))**
   - Design **([`list_bestfit_design.h`](/lib/utility/list_bestfit_design.h))**
   - Implementation **([`list_bestfit.c`](/lib/list_bestfit.c))**
   - Design **([`list_segregated_design.h`](/lib/utility/list_segregated_design.h))**
   - Implementation **([`list_segregated.c`](/lib/list_segregated.c))**
8. Runtime Analysis
   - Documentation **([`rbtree_analysis.md`](/docs/rbtree_analysis.md))**
9. The Programs
   - Documentation **([`programs.md`](/docs/programs.md))**

## Overview

If we now bring all of the strengths of the past improvements I have discussed, we can make a more space efficient, fast allocator that solves some of the challenges a heap allocator implementation poses for red black trees. One technique that is common when we want to save space in a red black tree is to eliminate the parent field. Instead, we use an array and treat it as a stack to represent the path down to the nodes we need to work with. This eliminates an eight byte pointer from the struct and in a normal red black tree might look something like this. Please note, I am using my array method for this implementation as well to eliminate symmetric cases.

```c
typedef struct rb_node {
    header header;
    struct rb_node *links[TWO_NODE_ARRAY];
}rb_node;
```

There is one problem with this approach for a heap allocator however. I could not think of a good way to coalesce nodes with the above struct. The problem arises with duplicates. When we insert duplicates into a red black tree, we know that they will always follow the rules of a binary search tree in relation to their parent, but we do not know where rotation may put them over the lifetime of the heap. This is a problem for a coalescing operation that needs to find the exact address of the node it needs.

When we search the tree by block size, as we normally do, there is no garuntee the first node of the correct size we find will be the one we are coalescing. This would result in a non sequential block of memory and corrupt the heap. So, we will simply take from our previous allocator, and store duplicates in a doubly linked list. Then we are free to use the stack, because we know that all nodes in the tree will be unique. We also have immediate access to coalesce any duplicate safely in constant time from its linked list.

We can acheive almost the same speed as the `rbtree_linked` allocator with this more space efficient implementation if we get creative. If we put a duplicate node in a linked list, it will have an extra field of the node going to waste. Why not create another node type, designated specifically for being a duplicate in a list, and have the first node in the linked list always store the parent? This way we acheive our garuntee of O(1) to coalesce any duplicate node. This is tricky to pull off but will produce an arguably superior allocator. The only speed boost we miss out on is when we coalesce a unique node. The `rbtree_linked` allocator can start fixup operations immediately if it coalesces a unique node. The `rbtree_stack` implementation must first do a O(lgN) search of the tree to build the stack. Here is the new tree scheme without a parent field. Now, we have a series of constant time checks we can use to tell where we are in the list and who the parent node is.

![rb-stack-scheme](/images/rb-duplicates-no-parent.png)

Here are the new types we can add for readability to reduce confusion when referring to list and tree.

```c
typedef struct rb_node {
    header header;
    struct rb_node *links[TWO_NODE_ARRAY];
    // Use list_start to maintain doubly linked list, using the links[P]-links[N] fields
    struct duplicate_node *list_start;
}rb_node;

typedef struct duplicate_node {
    header header;
    struct duplicate_node *links[TWO_NODE_ARRAY];
    // We will always store the tree parent in first duplicate node in the list. O(1) coalescing.
    struct rb_node *parent;
} duplicate_node;

typedef enum rb_color {
    BLACK = 0,
    RED = 1
}rb_color;

typedef enum tree_link {
    // (L == LEFT), (R == RIGHT)
    L = 0,
    R = 1
} tree_link;

typedef enum list_link {
    // (P == PREVIOUS), (N == NEXT)
    P = 0,
    N = 1
} list_link;

static struct free_nodes {
    rb_node *tree_root;
    // These two pointers will point to the same address. Used for clarity between tree and list.
    rb_node *black_nil;
    duplicate_node *list_tail;
}free_nodes;
```

Here is a function that utilizes all of these types to help make these ideas a reality. This function relies on the time saving measures we put in place by tracking the parent so carefully in the first duplicate node. Notice how we transition explicitly between referring to our list and our tree. It is possible to save some complexity by not making a new type or worrying about casting and still acheive the same results. However, for the sake of clarity and other developers reading the code, I think it is important to use these types to make it as clear as possible what is happening.

This function executes whenever we begin our coalescing operation on `free()` and `realloc()`. You can observe that when we encounter a duplicate our logic is slightly more complex than a simple doubly linked list with a head and tail, but we do not exceed O(1) in our time complexity of freeing a duplicate from the list. This also allows us to avoid fixups when coalescing duplicates, as discussed earlier. If you want to see how we handle the more complex case of coalescing the tree node that serves as the head of our linked list of duplicates, please see the code.

```c
/* @brief free_coalesced_node  a specialized version of node freeing when we find a neighbor we
 *                             need to free from the tree before absorbing into our coalescing. If
 *                             this node is a duplicate we can splice it from a linked list.
 * @param *to_coalesce         the address for a node we must treat as a list or tree node.
 * @return                     the node we have now correctly freed given all cases to find it.
 */
void *free_coalesced_node(void *to_coalesce) {
    rb_node *tree_node = to_coalesce;
    // Go find and fix the node the normal way if it is unique.
    if (tree_node->list_start == free_nodes.list_tail) {
       return find_best_fit(get_size(tree_node->header));
    }
    duplicate_node *list_node = to_coalesce;
    rb_node *lft_tree_node = tree_node->links[L];

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
```

This was the most challenging of all the allocators for me to implement. It requires a strong handle on a spacial understanding of the changes that occur in a tree during rotations and transplants. We have to manage the array ourselves, accurately updating the nodes in the stack if they move or rotate during fixup operations and keep a good track of our index. It was challenging for me to apply the same structure of CLRS over a stack implmentation. I might recommend looking over the [`libval`](https://adtinfo.org/libavl.html/Red_002dBlack-Trees.html) implementation of a red black tree with a stack if you were to do this from scratch. It is a different approach and I struggled to read the code due to the naming convenients and lack of comments. However, I am sure it would be a good starting point to implement this technique. So, I stuck with the past implementations I had done and reworked them for this technique.

## Unified Stack Analysis

- **`free()`**- Freeing an allocated heap block takes one O(lgN) search to find the place for a new node. In this implementation however, we have a significant best case. If we find a node of the same size that we are inserting, we can insert it into the doubly linked list in O(1) time and we do not need to rotate or recolor the tree. If the node does not exist in the tree, we have the normal O(lgN) fixup operations.
- **`malloc()`**- Searching the tree for the best fitting block requires one O(lgN) search of the tree. Again, however, we can eliminate fixups if the best fit node is a duplicate. Then we have O(1) operations to take the duplicate from a doubly linked list.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. If the coalescing operation finds a duplicate node, it will be freed in O(1) time because I have setup the doubly linked list such that we know we are in the list and not the tree. We also have a node that perpetually serves as the tail to the list, its the same heap address we use for the null in the tree. This makes coalescing a duplicate a constant time operation. Because we always store the parent of a duplicate node in the first node of the linked list we can coalesce a duplicate in constant time. Great!
- **`style`**- This was a challenging implementation to complete, but it is able to loosely follow the structure of all other implementations up to this point. The greatest concern for this allocator is the readability of the stack management. To improve the style of this code it would be important to come up with a consistent method for rearranging and managing the stack during fixup operations. There are specific adjustments to the path in the stack that are required for rotations and deciding where and when to adjust the nodes in the array is an important style question. Also, tracking the parent node in duplicate nodes was very difficult. The effort is aided by the fact that we use the `black_nil` aka `list_tail` to help with edgcases, but there are still details to consider. However, the benefit is extreme speed and space optimizations.

> **Read the writeup for the next allocator, [`rbtree_topdown.md`](/docs/rbtree_topdown.md)**.

