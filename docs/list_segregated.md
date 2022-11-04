# List Allocators

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


What follows is a brief explanation of allocators I implemented in this repository other than those based on a Red-Black tree. I needed other implementations to serve as a baseline, point of comparison, or competition for the Red-Black tree allocators. I will go over the high level designs of each allocator with some visualizations to help explain the data structures. For the exact algorithms, please see the source code.

## A Simple List

This project began after I completed a Stanford Computer Science assignment that used a doubly linked list to maintain the free nodes in a heap allocator. While it sounds simple, a first encounter with a heap allocator can be quite intimidating. However, with the right helper functions and design choices, I was able to make it happen. Here is how a doubly linked list can be used to manage an allocator.

### Address or Size

We begin our allocator with one large pool of memory we have available to the user. If they request memory from us, we will begin the process of organizing our heap in terms of which sections are allocated and which are free. One way to do this is with a simple list.

We can choose to organize this list by address in memory or by size, with tradeoffs for each approach. Both are attempts to maximize utilization and decrease fragmentation.

![list-bestfit](/images/list-bestfit.png)

*Pictured Above: A simplified illustration of a doubly linked list of free nodes.*

Here are the key details from the above illustration.

- We can use a head and tail node to simplify instruction counts, safety, and ease of use for the implementer.
- We organize by size with a header that contains the size and and whether the block is free or allocated. In this image all the pictured nodes are free.
- We have a certain amount of bytes available for the user and then a footer at the bottom of our block. This will help with coalescing, which we won't discuss in detail here.

Now that you have the idea of the list in the abstract, here is how it actually appears in memory in a heap allocator.

![list-bestfit-real](/images/list-bestfit-real.png)

*Pictured Above: A more chaotic illustration of how the list actually works in memory. The doubly linked list is drawn to the right and the actual nodes they represent are illustrated by the thin white line.*

As an exercise, think about how this same configuration of nodes would look if they were organized by address in memory rather than by size.

## A Smart List

While the previous doubly linked list will work in many cases and is simple to implement, we can do better. With a strategy called Segregated Fits, we can massively decrease our runtime speed while improving utilization in many cases.

When we begin managing our heap, we can decide to make an array of doubly linked lists. There are many ways to organize this array, but one approach is to organize it by increasing size. We can store nodes that fit in a size range for a particular list.

For smaller sizes we can choose to maintain a list of one size. However, once we reach a size of 8 bytes, we can start doubling our sizes that serve as the minimum for each list. While I won't go over the details here, this doubling in base 2 allows us to make some interesting optimizations you can check out in the code.

Here is the high level scheme of this list of lists.

![list-segregated](/images/list-segregated.png)

*Pictured Above: An abstract layout of our lookup table. Notice that the nodes in the actual lists are not sorted. We rely on our loose sorting across all lists and add nodes to the front of each list to speed things up.*

However, this list is somewhat long. There are 20 slots and they start at one byte. This is not practical in the scheme of a heap allocator because our minimum block size we can accommodate or store starts at 32 bytes. So let's take a look at how this scheme actually looks in a heap allocator with properly sized segregated lists.

![list-segregated-real](/images/list-segregated-real.png)

*Pictured Above: The segregated lists that store various block sizes in our heap. Notice that the first four lists increment by 8 because that is the alignment we hold all blocks to for performance.*

Read the runtime analysis section for a detailed breakdown of how this more clever take on a doubly linked list stacks up against a Red-Black tree based allocator.

## Performance Analysis

These allocators serve as a point of comparison in my [Runtime Analysis](/docs/rbtree_analysis.md) section of the repository. For the Segregated Fits allocator there is much more room for optimizations and strategies to make the best fits possible. However, these strategies are all dependent on the intended utilization. For the purpose of this repository, the above strategy serves well as decent competition for the Red-Black tree allocators.
