# Doubly Linked Duplicates

## Overview

Unified left and right cases makes an implmentation well suited to try out even more creative strategies for using an array and an enum. Recall in both implementations that duplicate nodes are allowed. This makes the allocator fast by giving us immediate access to free any node from the tree and then fix the removal, but what if we could get a best case of constant time removals and cut down on the size of our tree? This might help with costly fixup operations.

One idea I wanted to try was to take what I learned from the explicit allocator that used a doubly linked list, and apply that idea to duplicate nodes in the tree. The left and right fields of a normal binary tree node are just arbitrary names we give to help us organize the relationship between data in our algorithms. We can, at any time, adjust how we interpret the relationship between nodes, thus transforming a tree into a doubly linked list. What if instead of looking at the `links[]` array as a `LEFT`-`RIGHT` array, we switched to viewing it as a `NEXT`-`PREV` array. We can then use the duplicate node in the tree as a dummy head. The black sentinel we have been using at the base of our red-black tree can also become the dummy tail of every doubly linked list for every duplicate. If we also make the parent field of all the nodes in the linked list NULL rather than the black sentinel we can also tell where we are in the list when we check to our left to see if the node to our left is the head node or not (we know the head node will have a valid parent field, either to its parent or the black sentinel). This makes managing the doubly linked list slightly less invariant than a normal doubly linked list with a head and tail, but ensures we maintain both the tree and the list. All of this combines nicely to make fast coalescing possible. Here is the scheme.

![rb-doubly-scheme](/images/rb-doubly-scheme.png)

We can create another enum to help. This will make it clear when we are referring to nodes as connections in a linked list and nodes in a tree.

```c
// When you see these, know that we are working with a doubly linked list, not a tree.
typedef enum list_link {
    // (P == PREVIOUS), (N == NEXT)
    P = 0,
    N = 1
} list_link;
```

Then when we access the indices of our links array, we do so with `P` and `N` rather than `L` and `R`. We now just need to know when we have duplicates at a given node in the tree, and where we can find the start of the doubly linked list. My approach is to use an extra field in my struct to find the first node in the doubly linked list.

```c
struct rb_node
{
    // block size, allocation status, left neighbor status, and node color.
    header header;
    struct rb_node *parent;
    struct rb_node *links[2];
    // Points to a list which we will use P and N to manage to distinguish from
    // the tree.
    struct duplicate_node *list_start;
};

struct duplicate_node
{
    header header;
    struct rb_node *parent;
    struct duplicate_node *links[2];
    struct rb_node *list_start;
};
```

Over the course of an allocator there are many free blocks of the same size that arise and cannot be coalesced because they are not directly next to one another. When coalescing left and right, we may still have a uniquely sized node to delete, resulting in the normal O(lgN) deletion operations. However, if the block that we coalesce is a duplicate, we are garunteed O(1) time to absorb and free the node from the doubly linked list. Here is one of the core functions for coalescing that shows just how easy it is with the help of our enum to switch between referring to nodes as nodes in a tree and links in a list.

```c
static struct rb_node *free_coalesced_node( void *to_coalesce )
{
    struct rb_node *tree_node = to_coalesce;
    // Quick return if we just have a standard deletion.
    if ( tree_node->list_start == free_nodes.list_tail ) {
        return delete_rb_node( tree_node );
    }

    struct duplicate_node *list_node = to_coalesce;
    // to_coalesce is the head of a doubly linked list. Remove and make a new head.
    if ( tree_node->parent ) {
        remove_head( tree_node );

        // to_coalesce is next after the head and needs special attention due to list_start field.
    } else if ( list_node->links[P]->list_start == to_coalesce ) {
        tree_node = (struct rb_node *)list_node->links[P];
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
```

Here is the same picture from the earlier tree with these new linked nodes in the tree. We are able to save some meaningful bulk to the tree. Whereas before we could not even see half of the tree, now our whole tree prints nicely and is more shallow.

![rb-tree-shallow](/images/rb-tree-shallow.png)

This implementation comes out slightly faster than a normal red black tree. I only observed a 1% drop in overall utilization. However, I am sure that under the right use cases we would begin to suffer the cost of adding another field to the struct. This increases the internal fragmentation of the heap by creating hefty nodes with many fields. Small requests will chew through more memory than they need. However, I hypothesize that this approach would scale well when the number of free nodes grows significantly, especially if there are many duplicates. We get an absolute worst case operation of O(lgN) for any operation on the heap, but the garunteed best case for coalescing a duplicate becomes O(1). We also do not have to rebalance, recolor, or rotate the tree for calls to malloc if the best fit search happens to yeild a duplicate. I will have to put together a more thorough testing framework later to see if this implementation yeilds any benefits. Overall, this was an interesting exercise in thinking about the difference in how we interpret the connections between nodes.

## Unified Doubly Analysis

- **`free()`**- Freeing an takes one O(lgN) search to find the place for a new node. In this implementation however, we have a significant best case. If we find a node of the same size that we are inserting, we can insert it into the doubly linked list in O(1) time and we do not need to rotate or recolor the tree. If the node does not exist in the tree, we have the normal O(lgN) fixup operations.
- **`malloc()`**- Searching the tree for the best fitting block requires one O(lgN) search of the tree. Again, however, we can eliminate fixups if the best fit node is a duplicate. Then we have O(1) operations to take the duplicate from a doubly linked list.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. If the coalescing operation finds a duplicate node, it will be freed in O(1) time because I have setup the doubly linked list such that we know we are in the list and not the tree. We also have the node in the tree serving as the head and the sentinel black tree node serving as the tail. This makes coalescing a duplicate a constant time operation no matter where it is in the list. If we coalesce a unique node we have our normal O(lgN) fixup operations.
- **`style`**- This is one of the more readable and well decomposed implementation of all the allocators. The extra complexity of duplicates is handled through simple helper functions and the generalization of symmetric cases to directions and their opposites makes the code quite readable.


