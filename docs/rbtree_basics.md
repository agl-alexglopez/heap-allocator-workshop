## Background

While there are many excellent explanations of a Red Black tree that I drew upon for this allocator, I can go over the basics as I understand them. For a more detailed, world-class dive into Red-Black Trees, please read *Introduction to Algorithms*, Chapter 13, by Cormen, Leiserson, Rivest, and Stein.

## The Properties

In order to maintain O(lgN) single operations on a binary tree, Red-Black Trees operate under 5 rules.

1. Every node is either red or black.
2. The root is black.
3. Every leaf, `Nil` is black. Instead we will have a black sentinel node wait at the bottom of the tree and any fields in a node that would be `NULL` will point to the sentinel. This is critical to all of these implementations.
4. A red node has two black children.
5. The black height, the number of black nodes on any path from the root to the black sentinel, must be the same for every path.

Here is a basic tree layout that utilizes a `parent` field. As you read through allocator implementations, you will see that the [`rbtree_stack`](/docs/rbtree_stack.md) and [`rbtree_topdown`](/docs/rbtree_topdown.md) implementations are able to eliminate the need for this `parent` field.

![rbtree-basic](/images/rbtree-basic.png)

## Inserting

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

## Deleting

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

If we did not enter case 2 we now must execute Case 4, invariant.

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