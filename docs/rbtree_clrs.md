# CLRS Allocator

## Navigation

1. Home
   - Documentation **([`README.md`](/README.md))**
2. The CLRS Standard
   - Documentation **([`rbtree_clrs.md`](/docs/rbtree_clrs.md))**
   - Implementation **([`rbtree_clrs.c`](/src/rbtree_clrs.c))**
3. Unified Symmetry
   - Documentation **([`rbtree_unified.md`](/docs/rbtree_unified.md))**
   - Implementation **([`rbtree_unified.c`](/src/rbtree_unified.c))**
4. Doubly Linked Duplicates
   - Documentation **([`rbtree_linked.md`](/docs/rbtree_linked.md))**
   - Implementation **([`rbtree_linked.c`](/src/rbtree_linked.c))**
5. Stack Based
   - Documentation **([`rbtree_stack.md`](/docs/rbtree_stack.md))**
   - Implementation **([`rbtree_stack.c`](/src/rbtree_stack.c))**
6. Topdown Fixups
   - Documentation **([`rbtree_topdown.md`](/docs/rbtree_topdown.md))**
   - Implementation **([`rbtree_topdown.c`](/src/rbtree_topdown.c))**
7. Runtime Analysis
   - Documentation **([`rbtree_analysis.md`](/docs/rbtree_analysis.md))**
8. The Programs
   - Documentation **([`programs.md`](/docs/programs.md))**

## Overview

I modeled the code in my first allocator directly from the pseudocode in the textbook and it was a challenging endeavor. Making sure that all the code across every symmetric case is correct, well named, and specific to the needs of a heap allocator was a bug prone journey. However, I now have a faithful reproduction of the textbook algorithm where applicable. I added logic to make deleting from the tree a best fit approach. For a call to `malloc` we are able to service the request in O(lgN) time while providing the best possible fitting node and splitting off any extra space and returning it to the tree if it is far too big.


![explicit-tree](/images/explicit-tree.png)

*Pictured Above: The final form of my printing debugger function. I learned how to incorporate colors into output in C in order to track red nodes, black nodes, and allocated blocks. Leaving more helpful information for free blocks is also important.*

It also turns out that red-black trees are perfectly capable of handling duplicates while maintaining balance and red black rules. This is convenient because we do not need to handle the case of repeats, and if we use a parent field, we maintain constant time coalescing and worst case O(lgN) fixups after freeing the coalesced node from the tree. The downside is that many allocator usages have repeating blocks and trees can become quite large. However, thankfully, the balance of a red black tree mitigates this by making the tree extremely wide. here is an example. Here is just a small glimpse of a pattern of requests. See how many repeats you can spot. Read on to find out some ways to cut down not only on lines of code but also on the size of a tree with some interesting strategies.

![rb-tree-print](/images/rb-tree-wide.png)

*Pictured Above: A more detailed printing debugging function for the red black tree proves to be a huge help for spotting where the problem nodes are and what is going wrong with the insertions, deletions, rotations, and color flips.*

## CLRS Analysis

- **`free()`**- Freeing an allocated block, thus inserting it into a red black tree, takes one O(lgN) search to find the place for a new node. The insertion fixup algorithm calls for a worst case of O(lgN) fixup operations as we work back up the tree, giving us O(lgN) + O(lgN), or just O(lgN) in big-O time complexity.
- **`malloc()`**- Searching the tree for the best fitting block requires one O(lgN) search of the tree. Deleting from a red black tree takes at most three rotations, but O(lgN) color adjustments on the way back up the tree in the worst case.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. When they do this, if they find a block they will be able to immediately free it from its position in the tree and start the fixup operations. This is because we have the memory address of the block in our heap, and thus our tree. So, we use the parent field to start fixups immediately, costing O(lgN) time.
- **`style`**- The style of this code is direct and understandable. It is clear what is happening in any given code block due to the use of the left and right cases. However, it can become overwhelming to try to trace through the code for a series of tree operations due to the length of the functions and the cost of the symmetry.

> **See the next allocator writeup, [`rbtree_unified.md`](/docs/rbtree_unified.md)**.

