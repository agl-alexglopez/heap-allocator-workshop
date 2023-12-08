# Splaytree Topdown

## Overview

As discussed in the Splay Tree Stack implementation, Splay Trees are great! They are fast and require only a reasonable amount of code to implement. However, with any tree data structure research and teaching often push to eliminate traversals. Topdown Splay Trees address traversals in a fascinating way. They seek to fix the tree on the way down with two helper trees. In broad terms, the way I think about topdown fixups is that we are pushing nodes out of our way and hanging them up in the left and right helper trees as we descend in search for our desired node. The ideas for this algorithm are quite impressive, and I used Carnegie Mellon University Lecture Material and Daniel Sleator's implementation as a starting point. However, I quickly found there were some tradeoffs for a topdown Splay Tree in the context of a heap allocator.

First, we know that Splay Trees are not designed for duplicates. At this point, we know from our other allocators that this problem can be solved with the help of the doubly linked list attached to the nodes in the tree. A duplicate will simply be tacked on to the node in the tree and thus we can use the tree as a set, like normal. However, another problem is that a heap must service a request from `malloc` if it has the capacity to do so. This means that a traditional binary search can get us in trouble. For example consider this tree.

![splaytree-topdown-problem](/images/splaytree-topdown-problem.png)

Our key is 325, so when we search for that entry we won't find it. We will end up at 200, which is too small to accomodate our request. Not only that, but because we are performing topdown fixups we will have completely altered the structure of the tree after this failed search is complete. We could wait to do any fixups until we have found the right fit, but then we are just duplicating bottom up fixups and we would need to bring back our stack!

Instead, my solution was to remember the best fit on the way down. In this case, the best fit would be 350. However, due to topdown rotations, that node is off in the right helper tree until we make 200 the root of the tree and join everything back together. So the full altered algorithm is as follows.

1. Search for the desired size, remembering the best fit on the way down.
2. If the best fit is our final node in the search we will finish our removal process and we are done.
3. If the best fit is elsewhere finish making the current node the new root and then do another topdown splay search for the fit we know will work. This is two traversals total.

While I am not happy with the extra traversal in the worst case, consider that it is no worse than the bottom up version. Also, perhaps moving a close fit to the root will benefit us in the future if similar usage patterns continue. Also, when we know a node is in our tree, such as during coalescing, a single topdown pass will find it, remove it, and fix the tree. So we do benefit from the topdown algorithm at times. In fact, the standard splay search is quite short which amazes me that such simple code can yeild such a fast data structure. Here is splay.

```c
static struct node *splay( struct node *root, size_t key )
{
    // Pointers in an array and we can use the symmetric enum and flip it to choose the Left or Right subtree.
    // Another benefit of our nil node: use it as our helper tree because we don't need its Left Right fields.
    free_nodes.nil->links[L] = free_nodes.nil;
    free_nodes.nil->links[R] = free_nodes.nil;
    struct node *left_right_subtrees[2] = { free_nodes.nil, free_nodes.nil };
    struct node *finger = NULL;
    for ( ;; ) {
        size_t root_size = get_size( root->header );
        enum tree_link link_to_descend = root_size < key;
        if ( key == root_size || root->links[link_to_descend] == free_nodes.nil ) {
            break;
        }
        size_t child_size = get_size( root->links[link_to_descend]->header );
        enum tree_link link_to_descend_from_child = child_size < key;
        if ( key != child_size && link_to_descend == link_to_descend_from_child ) {
            finger = root->links[link_to_descend];
            give_parent_subtree( root, link_to_descend, finger->links[!link_to_descend] );
            give_parent_subtree( finger, !link_to_descend, root );
            root = finger;
            if ( root->links[link_to_descend] == free_nodes.nil ) {
                break;
            }
        }
        give_parent_subtree( left_right_subtrees[!link_to_descend], link_to_descend, root );
        left_right_subtrees[!link_to_descend] = root;
        root = root->links[link_to_descend];
    }
    give_parent_subtree( left_right_subtrees[L], R, root->links[L] );
    give_parent_subtree( left_right_subtrees[R], L, root->links[R] );
    give_parent_subtree( root, L, free_nodes.nil->links[R] );
    give_parent_subtree( root, R, free_nodes.nil->links[L] );
    return root;
}
```

See the code for more! Finally, we can compare the trees that are produced from bottom up and topdwon splays.

![splay-compare](/images/splay-compare.png)

On the left is bottom up and on the right is topdown at the peak free nodes size for a script. They are very similar! But not identical.

## Splaytree Topdown Analysis

- **`free()`**- Freeing an allocated heap block takes one O(lgN) search to place a new node in the tree. This is a benefit that we see when compared to the Splay Tree Stack version where it took a search and a fixup. In this implementation I choose to fix the tree for any accessed node. Perhaps this indicates a usage trend we can take advantage of and it is always good to try to heal the tree with splay operations. I can assess if the runtime is better if I insert duplicates into the doubly linked list with or without a splay. Now, I splay every time.
- **`malloc()`**- Searching the tree for the best fitting block requires at most two O(lgN) splay operations to fix the tree. In the best case we only have one search and we find a node sufficiently large to accomodate our key. If we end up at a node that is too small we will finish fixing that search and then launch into a second search for the best fit that we remembered on the way down.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. If the coalescing operation finds a duplicate node, it will be freed in O(1) time because I have setup the doubly linked list such that we know we are in the list and not the tree. We also have a node that perpetually serves as the tail to the list, its the same heap address we use for the null in the tree. This makes coalescing a duplicate a constant time operation. Because we always store the parent of a duplicate node in the first node of the linked list we can coalesce a duplicate in constant time. Great! If we are coalescing a unique node we need to find it then splay it to the root. However, the topdown implementation helps us again. We know that this node is in the tree so the single topdown O(lgN) operation will fix the tree on the way down and find it. Great!
- **`style`**- This was a challenging implementation. Making a Topdown Splay tree work for a best fit allocator had some challenges and corner cases that took me a while to discover. My biggest dissapointment was having to write two almost identical splay operations, one for best fit and one for exact fit. However, once everything is working, it is an interesting implementation to read that is quite short.

