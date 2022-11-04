# Red Black Tree Allocators

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

## Summary

I worked through five separate implementations of a Red-Black Tree heap allocator with different design choices, sacrifices, and optimizations. Here is a summary of each and a link if you wish to jump to any one section.

- **[`CLRS`](/docs/rbtree_clrs.md)**- An implementation of a Red Black Tree heap allocator by the book, *Introduction to Algorithms: Fourth Edition* by Cormen, Leiserson, Rivest, and Stein, to be specific. This allocator follows one of the leading implementations of a red black tree through the pseudocode outlined in chapter 13 of the textbook. With some slight adjustments as appropriate, we have a solid heap allocator with code that is straightforward to understand. The only annoyance is that we must write code for the left and right case of any fixup operation for the tree. This means the functions are lengthy and hard to decompose.
- **[`Unified`](/docs/rbtree_unified.md)**- The next implementation follows the structure of the first except that we cut out the need for symmetric cases, instead taking a more general approach. Instead of thinking about left and right, consider a generic direction and its opposing direction. With some modifications to the fields of our node and some custom types, we can complete an allocator that cuts the lines of code for the symmetric operations of a red black tree in half. This makes the code significantly shorter and with careful naming, arguably more readable. This convention of eliminating left and right cases continues in all subsequent implementations.
- **[`Doubly-Linked Duplicates`](/docs/rbtree_linked.md)**- This implementation adds an additional field to all nodes to track duplicate nodes. Red Black trees are capable of handling duplicates, but there are many duplicates over the course of a heap's lifetime. We can consider pruning our tree and making the tree as small as possible while maintaining as many constant time operations as possible for the duplicates. This implementation is all about speed, but sacrifices on efficient space usage.
- **[`Unified Stack`](/docs/rbtree_stack.md)**- Many common Red Black tree implementations use a parent field to make fixing the tree from the bottom up easier. This makes any operation on the tree straightforward with edge cases that are easy to account for with the right moves. However, if we want to eliminate the parent field we must track the nodes on the way down somehow. Recursion is not the best option for a heap allocator, so we can opt for an array that will act as a more efficient stack. When we abandon the parent field, there are many unique challenges that we must solve in the context of a heap allocator. This was a difficult implementation.
- **[`Unified Topdown`](/docs/rbtree_topdown.md)**- All other allocators in this repository use a bottom up approach to fix a Red Black Tree. This means that we venture down the tree to insert or find our to node, and then fix the tree as much as necessary on the way back up. Without a parent field, any operation requires O(lgN) + O(lgN) operations to go down and back up the tree. We can eliminate one of these O(lgN) operations if we fix the tree on the way down. With guidance from Julienne Walker's **[`Eternally Confuzzled`](https://web.archive.org/web/20141129024312/http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx)** article on top-down operations on a Red Black Tree, we can get the best of all worlds. We are space efficient and fast.
- **[`List Allocators`](/docs/list_segregated.md)**- If you would like to see the more basic allocators that inspired this project see the list based allocators. These also serve as a great point of comparison against the Red-Black tree allocators in the runtime analysis section of the repository.
- **[`Runtime Analysis`](/docs/rbtree_analysis.md)**- See how the allocators stack up against one another in terms of performance. Explore the data of both artificial and real world tests to see if the different optimizations and design choices of these allocators create any costs or advantages.
- **[`The Programs`](/docs/programs.md)**- Once you understand how the heap allocators operate, you can interact with them through three programs in this repository: `test_harness.c`, `time_harness.c`, and `print_peaks.c`. They will all give you information about how a heap allocator handles a script in terms of efficiency, speed, and algorithmic complexity. You can also explore how I parsed real world `ltrace` output or generated artificial scripts with the python code in this repository.

![rb-tree-collage](/images/rb-tree-collage.png)

*Pictured Above: A sampling of the programs, tools, and analysis I used to complete this project. Explore the repository to see how I implemented the allocators, how I measured their performance, and how I visualized or interacted with them through the programs in this repository.*

## Background

While there are many excellent explanations of a Red Black tree that I drew upon for this allocator, I can go over the basics as I understand them. For a more detailed, world-class dive into Red-Black Trees, please read *Introduction to Algorithms*, Chapter 13, by Cormen, Leiserson, Rivest, and Stein.

### The Properties

In order to maintain O(lgN) single operations on a binary tree, Red-Black Trees operate under 5 rules.

1. Every node is either red or black.
2. The root is black.
3. Every leaf, `Nil` is black. Instead we will have a black sentinel node wait at the bottom of the tree and any fields in a node that would be `NULL` will point to the sentinel. This is critical to all of these implementations.
4. A red node has two black children.
5. The black height, the number of black nodes on any path from the root to the black sentinel, must be the same for every path.

Here is a basic tree layout that utilizes a `parent` field. As you read through allocator implementations, you will see that the [`rbtree_stack`](/docs/rbtree_stack.md) and [`rbtree_topdown`](/docs/rbtree_topdown.md) implementations are able to eliminate the need for this `parent` field.

![rbtree-basic](/images/rbtree-basic.png)

### Inserting

We always insert new nodes into the tree as red nodes. This is because we will have the least impact on property 4 and 5 with this approach. If we insert a node and its parent is black, we do not need to fix the tree. If we insert a node and the parent is red, we will launch into our fixup operations.

When we insert a node and have a double red we are most concerned with two cases.

1. If we have a black aunt node, remember the black sentinel can be an aunt, we rotate.
2. If we have a red aunt node, we color flip.

Here is an example of a basic color flip we need to perform after inserting the node with the value `105`.

![red-aunt](/images/red-aunt.png)

*Pictured Above: A color flip to repair insertion into a Red-Black tree. Note that the root is temporarily colored red in this fixup, but we always recolor the root black as an invariant to satisfy property 2.*

Finally here is a more complex example to show what we do if we encounter a red aunt and a black aunt. I found it more helpful to consider how dramatically a larger tree can change when these cases occur, then just looking at the smallest subcases. This illustration exercises all code paths of the insert fixup operations. The tree in phases 3 and 4 is still part of the same case that occurs when we encounter a black aunt. It is possible for a black aunt to force two rotations in order to correct the tree.

The `*` marks the `current` node under consideration. We define the `parent` and `grandparent` in relation to the `current` node marked with the `*` as we move up the tree.

![insert-cases](/images/insert-cases.png)

*Pictured Above: Encountering a red aunt, moving up the tree, then encountering a black aunt. Notice how the entire tree rebalances as we eventually redefine our root.*

### Deleting

If you are familiar with the delete operation for normal Binary Trees, the operation is similar for Red Black Trees. The initial three cases, in broad terms, for deleting a node are demonstrated below. There are a few more subtleties that can pop up in the bottom case, but you can explore those in the code. Note, the top two cases are occuring somewhere down an arbitrary tree, illustrated by the squiggly line, while the third case shows a complete tree. The blue x indicates the node being deleted, the `*` represents the replacement node, and the second black circle represents the node that we give an *extra black* that it must get rid of as we fix the tree.

![rb-delete-cases](/images/rb-delete-cases.png)

*Pictured Above: Three cases for deleting from a Red Black Tree. The top two cases occur somewhere in an arbitrary tree while the bottom case illustrates a complete tree.*

All three of these cases will require us to at least enter the fixup helper function after we delete the node, because the node we are deleting is black, which could affect the black height of the tree. However, do you see a simple fix for the first two cases that would maintain the black height of the tree and not violate property 4 (property 4 states that a red node must have two black children)? Think about how you can use the *extra black* we have given to the replacement node. It is the third case at the bottom that is most interesting to us because it presents some interesting challenges.

In the third case, when we need to go find an inorder successor for the node being deleted, we give the *extra black* to the right child of the replacement node. The black sentinel is a valid node in a Red Black Tree, so it is acceptable to give it the *extra black*. There are then four cases to consider when fixing up a tree. We only enter a loop to fix the tree if the current node with the *extra black* is black. In the case we are discussing now, the fix is relatively simple and we will go over the other cases shortly.

The `*` marks the `current` node under consideration. We define the `parent` and in relation to the `current` node marked with the `*` as we move up the tree.

![rb-delete-case-2](/images/rb-delete-case-2.png)

*Pictured Above: Case 2 for a Red Black Tree delete. While this illustration has a node with two children that are `Null`, in reality, we just point both fields to the black sentinel that we have waiting at the bottom of the tree.*

Now that you have seen how one of the delete fixup cases works on a real tree, we will step back to illustrate how all cases work overall. I am using the same demonstration method that is implemented in CLRS. We will operate somewhere down an arbitrary tree and introduce some new abstractions.

- `A`, `B`, `C`, `D`, `E`, and `F` represent arbitrary subtrees further down the tree.
- Brown nodes represent nodes that can be either red or black. This means we either do not care what the color is, or we use the color it has without needing to know the color beforehand.
- `*` represents the current node under consideration. Parent and sibling are defined in relation to this node.
- The second black circle represents the *extra black* given to a node under consideration.

Here is Case 1. Case 1 falls through meaning we will go on to check two scenarios afterwards.

- Execute Case 2
- `OR...`
- Check Case 3 and fall through to execute case 4

![rb-delete-case-1](/images/rb-delete-case-1.png)

We will now determine if it necessary to enter Case 2. This should look familiar to the case I demonstrated with the real tree deletion earlier.

![rb-delete-case-2-abstract](/images/rb-delete-case-2-abstract.png)

If we did not enter Case 2, we will determine if we should enter Case 3.

![rb-delete-case-3](/images/rb-delete-case-3.png)

Whether we entered Case 3 or not, we now must execute Case 4, invariant.

![rb-delete-case-4](/images/rb-delete-case-4.png)

## Trees in a Heap

The above overview of Red-Black trees is agnostic to specific applications. Because this is a repository focused on heap allocators, specifically, it is helpful to see how the data structure is applied to the heap.

![rbtree-real](/images/rbtree-real.png)

*Pictured Above: A illustration of a red black tree in the context of a heap allocator.*

Here are the key details of the above image.

- The white lines represent the parent and child links. There are two lines because every child also tracks its parent. However, explore this repository for implementations that forgo this parent field.
- The black sentinel node is made visible here for clarity. You can actually see its location in memory and how it serves as the child of all leaves and the parent of the root.
- The purple lines help you see where in memory the nodes in the tree are located, for clarity.

That completes our overview of Red Black Trees. There are many interesting optimizations both in terms of lines of code and speed of the tree I decided to pursue. There are also interesting challenges to solve for a Red Black Tree in the context of a heap allocator. Please explore the implementation write ups and code in this repository for more.

## Next Steps

This allocator is contained within a *segment* that helps create artificial boundaries for the heap with addresses that will remain the same upon successive runs. This makes testing and verifying correctness easier, especially when we run the **[`test_harness.c`](/src/test_harness.c)** program and run the debugging functions. To expand this project further, I could explore implementing this as if it were a heap allocator library, providing the full suite of heap operations. This would require more thought towards memory safety and verification of input from the user, but could be an interesting exercise in bringing these ideas into a "real" context. It would then be easier to compare this custom allocator to standard allocator offerings.

**([`Navigation`](#navigation))**

