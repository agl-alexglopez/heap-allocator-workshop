# Splaytree Stack

## Overview

Splay trees are a fascinating data structure with an amortized O(lgN) runtime. They acheive this with a binary search tree and fixup operations. However, where Red-Black Trees are concerned with perfectly balanced trees, Splay Trees focus more on generally helping all subtrees improve balance as an element is selected for removal. While you can read the intro to Splay Trees in the dedicated writeup, the basics are that we select a node for removal or insertion and fix the tree as we remove or after we insert that element. Fixing the tree means bringing that node to the root and performing a combination of Zig and Zag operations.

Here are the types we can use to acheive our goals for a Splay Tree Heap allocator.

```c
enum tree_link
{
    L = 0,
    R = 1
};

enum list_link
{
    P = 0,
    N = 1
};

struct node
{
    header header;
    struct node *links[2];
    struct duplicate_node *list_start;
};

struct duplicate_node
{
    header header;
    struct duplicate_node *links[2];
    struct node *parent;
};

struct tree_pair
{
    struct node *lesser;
    struct node *greater;
};
```

We keep the common design of many of the tree allocators at this point by using an array to store the nodes. This means that we effectively eliminate left and right cases for a binary search tree and can instead use one code path and an enum to represent left and right. 

We also maintain a doubly linked list of duplicate nodes of the same size. Splay Trees are not designed to work with a heap allocator by default. They do not support duplicates and we have to be very specific in how we apply the best fit rules to a splay tree. Having only one unique node in the tree to worry about makes this much easier.

To see what I mean regarding left and right cases, here is the entire core logic of a splay tree distilled to one code path with no left and right cases needed.

```c
static void splay( struct node *cur, struct path_slice p )
{
    while ( p.len >= 3 && p.nodes[p.len - 2] != free_nodes.nil ) {
        struct node *gparent = p.nodes[p.len - 3];
        struct node *parent = p.nodes[p.len - 2];
        if ( gparent == free_nodes.nil ) {
            // Zig or Zag rotates in the opposite direction of the child relationship.
            rotate( !( parent->links[R] == cur ), parent, ( struct path_slice ){ p.nodes, p.len - 1 } );
            p.nodes[p.len - 2] = cur;
            --p.len;
            continue;
        }
        enum tree_link parent_to_cur_link = cur == parent->links[R];
        enum tree_link gparent_to_parent_link = parent == gparent->links[R];
        // The Zag-Zag and Zig-Zig cases are symmetrical and easily united into a case of a direction and opposite.
        if ( parent_to_cur_link == gparent_to_parent_link ) {
            // The choice of enum is arbitrary here because they are either both L or both R.
            rotate( !parent_to_cur_link, gparent, ( struct path_slice ){ p.nodes, p.len - 2 } );
            p.nodes[p.len - 3] = parent;
            p.nodes[p.len - 2] = cur;
            rotate( !parent_to_cur_link, parent, ( struct path_slice ){ p.nodes, p.len - 2 } );
            p.nodes[p.len - 3] = cur;
            p.len -= 2;
            continue;
        }
        // We unite Zig-Zag and Zag-Zig branches by abstracting links. Here is one of the two symmetric cases.
        // | gparent            gparent                      current       |
        // |      \                   \                     /       \      |
        // |       parent  ->         current    ->   gparent       parent |
        // |      /                          \                             |
        // | current                         parent                        |
        // We want the parent-child link to rotate the same direction as the grandparent-parent link.
        // Then the gparent-rotatedchild link should rotate the same direction as the original parent-child.
        rotate( gparent_to_parent_link, parent, ( struct path_slice ){ p.nodes, p.len - 1 } );
        p.nodes[p.len - 2] = cur;
        rotate( parent_to_cur_link, gparent, ( struct path_slice ){ p.nodes, p.len - 2 } );
        p.nodes[p.len - 3] = cur;
        p.len -= 2;
    }
}
```

There are a few things to note. The splay operation is simple compared to the operations and code I have discussed for Red-Black trees. I thought that the lines of codes savings we got with the Red-Black trees was impressive when we united left and right cases, but those savings had to be applied across removal, insertion, and fixup functions. Here, everything that makes a splay tree effective is pretty much directly in front of us. While the `split` and `join` operations are common to completing an operation on a Splay Tree those are only a few lines each. It is remarkable that such a simple concept in terms of implementation complexity can yeild such strong runtime results. In practice the Splay Tree allocators perform just as well as the Red Black Tree allocators, even outperforming most of them in some workloads.

It is also fun to compare the trees produced by the Splay Tree allocator when compared to the Red Black Tree allocators. For example here is a comparison across an identical workload.

![splay-rb-compare](/images/splay-rb-compare.png)

On the left you see the peak free nodes our splay tree had to manage for a given script. On the right, the Red Black Tree. It is interesting to see how tidy the Red-Black Tree looks in comparison and yet the Splay Tree maintains its speed. Because the Splay Tree prioritizes moving frequently accessed nodes to the root, there may be cases where the tree does not look so balanced down certain branches. It is the amortized runtime we care about so this acceptable.

A final point is to notice that in a Red Black Tree it is the color and logic of the nodes we focus on where the nodes actually carry meaning. In a Splay Tree we do not have any extra logic in the code for coloring the edges, but it can help to conceptually understand a Splay Tree through a heavy/light decomposition when thinking about why the tree is fast. Consider blue edges as a link to a subtree rooted at X where the size of that subtree is less than or equal to one halft the size of the tree rooted at the parent. Then, red edges are those where the number of nodes rooted at X is greater than one half the nodes rooted at the parent. 

The goal of a splay tree is then to take advantage of *good* edges that drop half the weight of the tree, weight being the number of nodes rooted at X. Blue edges are obviously advantageous so if we have a mathematical bound on the cost of those edges, a splay tree then **amortizes** the cost of the red edges, leaving a solid O(lgN) runtime. This is similar to how I think about `vector<T>` in C++. We have amortized constant insertion time even though pesky O(N) operations do occur. However, this is where my knowledge on the *why* behind splay trees ends. I could not tell you the full mathematical proof for why the runtime is O(lgN). I still need time with the papers and data structure to fully understand this and explain it in my own words. For more information on the heavy light decomposition of Splay Trees that I used and much more fascinating information, please see any [CS166 Lecture material from Stanford University](chrome-extension://efaidnbmnnnibpcajpcglclefindmkaj/https://web.stanford.edu/class/cs166/lectures/13/Small13.pdf). Links may break so just google "Splay Trees Stanford" for some great slides!


## Splaytree Stack Analysis

- **`free()`**- Freeing an allocated heap block takes one O(lgN) search to find the place for a new node and an O(lgN) splay operation. In this implementation I choose to fix the tree for any accessed node. Perhaps this indicates a usage trend we can take advantage of and it is always good to try to heal the tree with splay operations. I can assess if the runtime is better if I insert duplicates into the doubly linked list with or without a splay. Now, I splay every time.
- **`malloc()`**- Searching the tree for the best fitting block requires one O(lgN) search of the tree and one O(lgN) splay operation.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. If the coalescing operation finds a duplicate node, it will be freed in O(1) time because I have setup the doubly linked list such that we know we are in the list and not the tree. We also have a node that perpetually serves as the tail to the list, its the same heap address we use for the null in the tree. This makes coalescing a duplicate a constant time operation. Because we always store the parent of a duplicate node in the first node of the linked list we can coalesce a duplicate in constant time. Great! If we are coalescing a unique node we need to find it then splay it to the root 2O(lgN) aka O(lgN).
- **`style`**- This was a fun implementation. It is surprising how few lines of code are required for a fast data structure when compared to a Red Black tree. This implementation benefits greatly from the uniting of left and right cases with an enum into one code path. The combinations of Zig and Zag operations can be confusing and is an opportunity to make mistakes across symmetric cases. Eliminating the room for error and abstracting the cases to a generalized link rather than a set left and right direction is helpful.


