# Unified Topdown

## Navigation

|Section|Writeup|Code|
|---           |---    |--- |
|1. Home|**[`README.md`](/README.md)**||
|2. The CLRS Standard|**[`rbtree_clrs.md`](/docs/rbtree_clrs.md)**|**[`rbtree_clrs.c`](/src/rbtree_clrs.c)**|
|3. Unified Symmetry|**[`rbtree_unified.md`](/docs/rbtree_unified.md)**|**[`rbtree_unified.c`](/src/rbtree_unified.c)**|
|4. Doubly Linked Duplicates|**[`rbtree_linked.md`](/docs/rbtree_linked.md)**|**[`rbtree_linked.c`](/src/rbtree_linked.c)**|
|5. Stack Based|**[`rbtree_stack.md`](/docs/rbtree_stack.md)**|**[`rbtree_stack.c`](/src/rbtree_stack.c)**|
|6. Topdown Fixups|**[`rbtree_topdown.md`](/docs/rbtree_topdown.md)**|**[`rbtree_topdown.c`](/src/rbtree_topdown.c)**|
|7. Runtime Analysis|**[`rbtree_analysis.md`](/docs/rbtree_analysis.md)**||

## Overview

Just when I thought that a stack was a space efficient implementation, but sacrificed on some speed, it turns out there is a theoretically optimal solution that mitigates the cost of forgoing a parent field. We can consider repairing a Red Black tree on the way down to a node that we need to insert or remove. This is an amazing idea that I read on Julienne Walker's [**`Eternally Confuzzled`**](https://web.archive.org/web/20141129024312/http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx) website. I could only find the archived website at this point, but I am glad I did. This algorithm solves most of the problems that come with forgoing the parent field in a red black tree. There are, however, significant modifications to Walker's implementation required to make the topdown algorithms work with a heap allocator.

This implementation will use the same array based struct to represent the `LEFT/RIGHT` and `NEXT/PREV`. We will also use a pointer to point to a doubly linked list of duplicate nodes, but more on that later. While in our previous stack based implementation we used an array to remember the path down to a node that we were inserting or deleting, a topdown approach only requires at most four pointers: `ancestor`, `grandparent`, `parent`, and `child`. These represent the greatest lineage that will be in need of repair as we go down the tree fixing rule violations that may arise. So, immediately we save space complexity by reducing our path representation to a constant factor.

The challenge to this approach is that it is not designed to handle duplicates. In fact, Walker explicitly designed the implementation around unique nodes in the tree. Another challenge arises in how Walker handles deletions. Instead of rewiring pointers to replace a node that needs to be transplanted in, Walker simply swaps the data in the key field of the node to be removed with its inorder predecessor. All of these factors present a challenge to this implementation. I need to be able to add and remove duplicates from the tree and I must repair all pointers for any insertion or removal request because users expect my functions to handle exactly the memory that they need or refer to. My heap also is made up of unique addresses that are finite and must be managed in memory exactly.

We can overcome the insertion problem rather easily. When we encounter a duplicate, we simply perform one final fixup but instead of ending the function, we use the doubly linked list we have been employing over the last few iterations of our allocator and add the duplicate to that list. This prevents problems that would otherwise arise with the logic and sequence of fixups on the way down the tree. Here is the new scheme. It is an identical setup to the stack duplicate model from the last allocator.

![rb-topdown-scheme](/images/rb-duplicates-no-parent.png)

Deletion of nodes requiring pointer maintenance and duplicates is by far the most complex aspect of this implementation. Here are all of the pointers that need to accurately represent all paths to their descendants during various and complex rotations.

- The node to be removed.
- The parent of the node to be removed.
- The current node that points to various nodes on the way down.
  - This acts as our "red" node that we push down to the bottom of the tree while we fixup.
- The parent of the current node.
- The grandparent of the current node.

Deleting while keeping track of all of these nodes and how the relationships between them may change was extremely challenging for me. Adding to that complexity, I had to add logic for a best fit search rather than a normal search by key removal. Remember that if we have space in our heap, a user requesting space means we must service the request if possible, finding the best fit that we have. We also must remove a duplicate rather than the node from a tree if we encounter duplicate sizes in the tree. Ultimately, this is all possible, but the code is very complex. I think that Walker is brilliant, and you will see the benefits of these strategies in the analysis section, but the readability of the code suffers greatly in my opinion. I will continue to ponder how to refactor this implementation.

The best part of this implementation is seeing the variety in tree that is produced when compared to other implementations.

![rb-compare](/images/rb-topdown-stack-compare.png)

*Pictured Above: On the left is a topdown insertion and deletion algorithm for a red black tree. Notice how red nodes are pushed to the bottom and there seems to be a higher number of black nodes due to the higher frequency of possible fixup operations. On the right is a bottom up insertion and deletion algorithm that uses a stack to track the path taken to any node in order to fix it later. I will have to take some performance measurements to see if there are quantitative differences between the quality of the tree that is produced with each algorithm.*

### Unified Topdown Analysis

- **`free()`**- Freeing an allocated heap block takes one $\Theta(lgN)$ search to find the place for a new node. During this search we are also performing any possible fixup operations that may arise. In this implementation however, we have a significant best case. If we find a node of the same size that we are inserting, we can insert it into the doubly linked list in $\Theta(1)$ time and we have a worst case of one rotation or recoloring operation before the function completes. If the duplicate does not exist in the tree, we simply place the red node at the bottom of the tree and we are done, having completed all fixup operations necessary.
- **`malloc()`**- Searching the tree for the best fitting block requires one $\Theta(lgN)$ search of the tree. We fix the tree while we go with at most two rotations per node on the way down. We will remove a duplicate node in $\Theta(1)$ or replace the node that needs deletion with the inorder predecessor. The worst case to find the inorder predecessor is $\Theta(lgN)$. However, we do not adjust our search on the way down if we encounter a best fit that needs to be replaced. In other words, there is still one $\Theta(lgN)$ operation on the way down and we can consider the worst case the continued search for the inorder predecessor that may be slightly further down the tree. Again, there are no further fixups required as all necessary operations are complete when the new node is transplanted in.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. If the coalescing operation finds a duplicate node, it will be freed in $\Theta(1)$ time because I have setup the doubly linked list such that we know we are in the list and not the tree. We also have the node in the tree serving as the head and the sentinel black tree node serving as the tail. This makes coalescing a duplicate a constant time operation. This implementation also stores the parent of any duplicate node in the first node of the linked list. Unique nodes are coalesced by removing them from the tree in one $\Theta(lgN)$ operation.
- **`style`**- The ideas of this algorithm are beautiful. The implmentation is not. This code is currently riddled with long functions, an infinite loop, `break` and `continue` statements, and numerous `if`'s for a multitude of scenarios. If you ever have a chance to read Walker's original work, I would recommend it. Walker's implementation is elegant because Walker has constraints on duplicates and can copy data between nodes. Because I have to manage memory addresses directly and reconnect pointers, the cases I consider increase and functions are more dificult to decompose. I will try to discuss this with more experienced developers to see what strategies are possible to simplify the code. The problems this code solves for this particular implmentation are great. I never have more than one $\Theta(lgN)$ operation for any given request to the heap, versus the two $\Theta(lgN)+\Theta(lgN)$ operations that all other implementations up to this point had in the worst case. However, I need to consider how to improve the style and readability of the code.

> **Read the writeup for the next allocator, [`rbtree_clrs.md`](/docs/rbtree_clrs.md)**.

