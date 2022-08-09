# Red Black Tree Allocators

I spent much of the summer of 2022 exploring the different ways that I could implement a Red-Black Tree heap allocator. I worked through five seperate implementations of a heap allocator with different design choices, sacrifices, and optimizations. Here is a summary of each and a link if you wish to jump to any one section.

- **[`CLRS`](#clrs-allocator)**- An implementation of a Red Black Tree heap allocator by the book, *Introduction to Algorithms: Fourth Edition* by Cormen, Leiserson, Rivest, and Stein, to be specific. This allocator follows one of the leading implementations of a red black tree through the pseudocode outlined in chapter 13 of the textbook. With some slight adjustments as appropriate, we have a solid heap allocator with code that is straightforward to understand. The only annoyance is that we must write code for the left and right case of any fixup operation for the tree. This means the functions are lengthy and hard to decompose.
- **[`Unified`](#unified-allocator)**- The next implmentation follows the structure of the first except that we cut out the need for symmetric cases, instead taking a more general approach. Instead of thinking about left and right, consider a generic direction and its opposing direction. With some modifications to the fields of our node and some custom types, we can complete an allocator that cuts the lines of code for the symmetric operations of a red black tree in half. This makes the code significantly shorter and with careful naming, arguably more readable. This convention of eliminating left and right cases continues in all subsequent implmentations.
- **[`Doubly-Linked Duplicates`](#doubly-linked-duplicates)**- This implementation adds an additional field to all nodes to track duplicate nodes. Red Black trees are capable of handling duplicates, but there are many duplicates over the course of a heap's lifetime. We can consider pruning our tree and making the tree as small as possible while maintaining as many constant time operations as possible for the duplicates. This implementation is all about speed, but sacrifices on efficient space usage.
- **[`Unified Stack`](#unified-stack)**- Many common Red Black tree implementations use a parent field to make fixing the tree from the bottom up easier. This makes any operation on the tree straightforward with edgecases that are easy to account for with the right moves. However, if we want to eliminate the parent field we must track the nodes on the way down somehow. Recursion is not the best option for a heap allocator, so we can opt for an array that will act as a more efficient stack. When we abondon the parent field, there are many unique challenges that we must solve in the context of a heap allocator. This was a difficult implementation.
- **[`Unified Topdown`](#unified-topdown)**- All other allocators in this repository use a bottom up approach to fix a Red Black Tree. This means that we venture down the tree to insert or find our to node, and then fix the tree as much as necessary on the way back up. Without a parent field, any operation requires $\Theta(lgN) + \Theta(lgN)$ operations to go down and back up the tree. We can eliminate one of these $\Theta(lgN)$ operations if we fix the tree on the way down. With guidance from Julienne Walker's **[`Eternally Confuzzled`](https://web.archive.org/web/20141129024312/http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx)** article on topdown operations on a Red Black Tree, we can get the best of all worlds. We are space efficient and fast.

![rb-tree-collage](/images/rb-tree-collage.png)

*Pictured Above: From left to right: the CLRS allocator, the unified allocator, the unified doubly linked list allocator, the unified stack allocator, and finally the unified topdown allocator. To learn more about the different implementations of a Red-Black Tree heap allocator and how the implementations evolved and improved over five iterations read on.*

## CLRS Allocator

> **Read my code here ([`rbtree_clrs.c`](/src/rbtree_clrs.c)).**

I modeled the code in my first allocator directly from the pseudocode in the textbook and it was a challenging endeavor. Making sure that all the code across every symmetric case is correct, well named, and specific to the needs of a heap allocator was a bug prone journey. However, I now have a faithful reproduction of the textbook algorithm where applicable. I added logic to make deleting from the tree a best fit approach. For a call to `malloc` we are able to service the request in $\Theta(lgN)$ time while providing the best possible fitting node and splitting off any extra space and returning it to the tree if it is far too big.


![explicit-tree](/images/explicit-tree.png)

*Pictured Above: The final form of my printing debugger function. I learned how to incorporate colors into output in C in order to track red nodes, black nodes, and allocated blocks. Leaving more helpful information for free blocks is also important.*

It also turns out that red-black trees are perfectly capable of handling duplicates while maintaining balance and red black rules. This is convenient because we do not need to handle the case of repeats, and if we use a parent field, we maintain constant time coalescing and worst case $\Theta(lgN)$ fixups after freeing the coalesced node from the tree. The downside is that many allocator usages have repeating blocks and trees can become quite large. However, thankfully, the balance of a red black tree mitigates this by making the tree extremely wide. here is an example. Here is just a small glimpse of a pattern of requests. See how many repeats you can spot. Read on to find out some ways to cut down not only on lines of code but also on the size of a tree with some interesting strategies.

![rb-tree-print](/images/rb-tree-wide.png)

*Pictured Above: A more detailed printing debugging function for the red black tree proves to be a huge help for spotting where the problem nodes are and what is going wrong with the insertions, deletions, rotations, and color flips.*

### CLRS Analysis

- **`free()`**- Freeing an allocated block, thus inserting it into a red black tree, takes one $\Theta(lgN)$ search to find the place for a new node. The insertion fixup algorithm calls for a worst case of $\Theta(lgN)$ fixup operations as we work back up the tree, giving us $\Theta(lgN) + \Theta(lgN)$, or just $\Theta(lgN)$ in big-O time complexity.
- **`malloc()`**- Searching the tree for the best fitting block requires one $\Theta(lgN)$ search of the tree. Deleting from a red black tree takes at most three rotations, but $\Theta(lgN)$ color adjustments on the way back up the tree in the worst case.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. When they do this, if they find a block they will be able to immediately free it from its position in the tree and start the fixup operations. This is because we have the memory address of the block in our heap, and thus our tree. So, we use the parent field to start fixups immediately, costing $\Theta(lgN)$ time.
- **`style`**- The style of this code is direct and understandable. It is clear what is happening in any given code block due to the use of the left and right cases. However, it can become overwhelming to try to trace through the code for a series of tree operations due to the length of the functions and the cost of the symmetry.

## Unified Allocator

> **Read my code here ([`rbtree_unified.c`](/src/rbtree_unified.c)).**

The downside to the CLRS implementation of a Red Black Tree is that there are many cases to consider, and for the major fixup operations after inserting and deleting, we have to consider two symmetric cases. It would be much more readable code if we could unite these cases and functions into the minimum possible lines of code. A great way to do this is with an array and an enum!

The traditional red-black node in the context of a heap allocator could look something like this.

```c
typedef struct heap_node_t {
    // The header will store block size, allocation status, left neighbor status, and node color.
    header_t header;
    struct heap_node_t *parent;
    struct heap_node_t *left;
    struct heap_node_t *right;
}heap_node_t;
```

If we use an array for the left and right fields in the struct and pair that with smart use of an enum, we can do some clever tricks to unify left and right cases to simple logic points in the code. Here is the new setup.

```c
typedef struct heap_node_t {
    // The header will store block size, allocation status, left neighbor status, and node color.
    header_t header;
    struct heap_node_t *parent;
    struct heap_node_t *links[2];
}heap_node_t;

// NOT(!) operator will flip this enum to the opposite field. !LEFT == RIGHT and !RIGHT == LEFT;
typedef enum direction_t {
    // These represent indices in the lr_array array of a heap_node_t node in the tree.
    LEFT = 0,
    RIGHT = 1
} direction_t;
```

With this new approach, your traditional search function in a binary tree can look slightly more elegant, in my opinion.

```c
/* @brief find_node_size  performs a standard binary search for a node based on the value of key.
 * @param key             the size_t representing the number of bytes a node must have in the tree.
 * @return                a pointer to the node with the desired size or NULL if size is not found.
 */
heap_node_t *find_node_size(size_t key) {
    heap_node_t *current = tree_root;

    while(current != black_sentinel) {
        size_t cur_size = extract_block_size(current->header);
        if (key == cur_size) {
            return current;
        }
        // Idiom for choosing direction. LEFT(0), RIGHT(1);
        direction_t search_direction = cur_size < key;
        current = current->links[search_direction];
    }
    return NULL;
}
```

This might not look like much but the saving grow substantially with more complex functions. Here is the CLRS left rotation function. We also have to write the right rotation function as a perfect mirror for left and right.

```c
/* @brief left_rotate  complete a left rotation to help repair a red-black tree. Assumes current is
 *                     not the black_sentinel and that the right child is not black sentinel.
 * @param *current     current will move down the tree, it's right child will move up to replace.
 * @warning            this function assumes current and current->right are not black_sentinel.
 */
void left_rotate(heap_node_t *current) {
    heap_node_t *right_child = current->right;
    current->right = right_child->left;
    if (right_child->left != black_sentinel) {
        right_child->left->parent = current;
    }
    right_child->parent = current->parent;
    // Take care of the root edgecase and find where the parent is in relation to current.
    if (current->parent == black_sentinel) {
        tree_root = right_child;
    } else if (current == current->parent->left) {
        current->parent->left = right_child;
    } else {
        current->parent->right = right_child;
    }
    right_child->left = current;
    current->parent = right_child;
}
```

Now, use some clever naming and the `direction_t` type and you can unify both rotation functions into one.

```c
/* @brief rotate     a unified version of the traditional left and right rotation functions. The
 *                   rotation is either left or right and opposite is its opposite direction. We
 *                   take the current nodes child, and swap them and their arbitrary subtrees are
 *                   re-linked correctly depending on the direction of the rotation.
 * @param *current   the node around which we will rotate.
 * @param rotation   either left or right. Determines the rotation and its opposite direction.
 */
void rotate(heap_node_t *current, direction_t rotation) {
    direction_t opposite = !rotation;
    heap_node_t *child = current->links[opposite];
    current->links[opposite] = child->links[rotation];
    if (child->links[rotation] != black_sentinel) {
        child->links[rotation]->parent = current;
    }
    child->parent = current->parent;
    heap_node_t *parent = current->parent;
    if (parent == black_sentinel) {
        tree_root = child;
    } else {
        // Another idiom for direction. Think of how we can use True/False.
        direction_t parent_link = parent->links[RIGHT] == current;
        parent->links[parent_link] = child;
    }
    child->links[rotation] = current;
    current->parent = child;
}
```

How much more clear and readable this is may be a matter up for debate, but we can save a significant number of lines in later functions. Here is an impressive, but still digestible, amount of savings to skim over. The `fix_rb_insert` function is quite readable when unified. For even more savings, go look at the `fix_rb_delete` function in the implementations. That one is a little too long to post here.

Here is the CLRS version.

```c
/* @brief fix_rb_insert  implements Cormen et.al. red black fixup after the insertion of a node.
 *                       Ensures that the rules of a red-black tree are upheld after insertion.
 * @param *current       the current node that has just been added to the red black tree.
 */
void fix_rb_insert(heap_node_t *current) {
    while(extract_color(current->parent->header) == RED) {
        if (current->parent == current->parent->parent->left) {
            heap_node_t *uncle = current->parent->parent->right;
            if (extract_color(uncle->header) == RED) {
                paint_node(current->parent, BLACK);
                paint_node(uncle, BLACK);
                paint_node(current->parent->parent, RED);
                current = current->parent->parent;
            } else {
                if (current == current->parent->right) {
                    current = current->parent;
                    left_rotate(current);
                }
                paint_node(current->parent, BLACK);
                paint_node(current->parent->parent, RED);
                right_rotate(current->parent->parent);
            }
        } else {
            heap_node_t *uncle = current->parent->parent->left;
            if (extract_color(uncle->header) == RED) {
                paint_node(current->parent, BLACK);
                paint_node(uncle, BLACK);
                paint_node(current->parent->parent, RED);
                current = current->parent->parent;
            } else {
                if (current == current->parent->left) {
                    current = current->parent;
                    right_rotate(current);
                }
                paint_node(current->parent, BLACK);
                paint_node(current->parent->parent, RED);
                left_rotate(current->parent->parent);
            }
        }
    }
    paint_node(tree_root, BLACK);
}
```

Here is the unified version.

```c
/* @brief fix_rb_insert  implements a modified Cormen et.al. red black fixup after the insertion of
 *                       a new node. Unifies the symmetric left and right cases with the use of
 *                       an array and an enum direction_t.
 * @param *current       the current node that has just been added to the red black tree.
 */
void fix_rb_insert(heap_node_t *current) {
    while(extract_color(current->parent->header) == RED) {
        heap_node_t *parent = current->parent;
        direction_t grandparent_link = parent->parent->links[RIGHT] == parent;
        direction_t opposite_link = !grandparent_link;
        heap_node_t *aunt = parent->parent->links[opposite_link];
        if (extract_color(aunt->header) == RED) {
            paint_node(aunt, BLACK);
            paint_node(parent, BLACK);
            paint_node(parent->parent, RED);
            current = parent->parent;
        } else {
            if (current == parent->links[opposite_link]) {
                current = current->parent;
                rotate(current, grandparent_link);
            }
            paint_node(current->parent, BLACK);
            paint_node(current->parent->parent, RED);
            rotate(current->parent->parent, opposite_link);
        }
    }
    paint_node(tree_root, BLACK);
}
```

This implementation seems to speed up the allocator slightly on my desktop running wsl2 Ubuntu and slow down slightly on my laptop running PopOS!. However, the core logic does not change and we have to do the same number of steps in each case. We save a few meaningful operations in some places, but the benefit is primarily in readability and reducing the places for a bug to hide in key functions by about half.

### Unified Analysis

- **`free()`**- Freeing an allocated block, thus inserting it into a red black tree, takes one $\Theta(lgN)$ search to find the place for a new node. The insertion fixup algorithm calls for a worst case of $O(lgN)$ fixup operations as we work back up the tree, giving us $\Theta(lgN) + \Theta(lgN)$, or just $\Theta(lgN)$ in big-O time complexity.
- **`malloc()`**- Searching the tree for the best fitting block requires one $\Theta(lgN)$ search of the tree. Deleting from a red black tree takes at most three rotations, but $\Theta(lgN)$ color adjustments on the way back up the tree in the worst case.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. When they do this, if they find a block they will be able to immediately free it from its position in the tree and start the fixup operations. This is because we have the memory address of the block in our heap, and thus our tree. So, we use the parent field to start fixups immediately, costing $\Theta(lgN)$ time.
- **`style`**- The style of this implementation is short, readable, and simple. The symmetric cases being unified helps make the key functions shorter to read. Thinking about the clearest way to name the `direction_t` and `links[]` pointer array operations for every function is key to helping others understand this more generalized code.

## Doubly Linked Duplicates

> **Read my code here ([`rbtree_linked.c`](/src/rbtree_linked.c)).**

Unified left and right cases makes an implmentation well suited to try out even more creative strategies for using an array and an enum. Recall in both implementations that duplicate nodes are allowed. This makes the allocator fast by giving us immediate access to free any node from the tree and then fix the removal, but what if we could get a best case of constant time removals and cut down on the size of our tree? This might help with costly fixup operations.

One idea I wanted to try was to take what I learned from the explicit allocator that used a doubly linked list, and apply that idea to duplicate nodes in the tree. The left and right fields of a normal binary tree node are just arbitrary names we give to help us organize the relationship between data in our algorithms. We can, at any time, adjust how we interpret the relationship between nodes, thus transforming a tree into a doubly linked list. What if instead of looking at the `links[]` array as a `LEFT`-`RIGHT` array, we switched to viewing it as a `NEXT`-`PREV` array. We can then use the duplicate node in the tree as a dummy head. The black sentinel we have been using at the base of our red-black tree can also become the dummy tail of every doubly linked list for every duplicate. If we also make the parent field of all the nodes in the linked list NULL rather than the black sentinel we can also tell where we are in the list when we check to our left to see if the node to our left is the head node or not (we know the head node will have a valid parent field, either to its parent or the black sentinel). This makes managing the doubly linked list slightly less invariant than a normal doubly linked list with a head and tail, but ensures we maintain both the tree and the list. All of this combines nicely to make fast coalescing possible. Here is the scheme.

![rb-doubly-scheme](/images/rb-doubly-scheme.png)

We can create another enum to help. This will make it clear when we are referring to nodes as connections in a linked list and nodes in a tree.

```c
// These represent indices in the links array of a heap_node_t node in a doubly linked list.
typedef enum list_t {
    PREV = 0,
    NEXT = 1
} list_t;
```

Then when we access the indices of our links array, we do so with PREV and NEXT rather than LEFT and RIGHT. We now just need to know when we have duplicates at a given node in the tree, and where we can find the start of the doubly linked list. My approach is to use an extra field in my struct to find the first node in the doubly linked list.

```c
typedef struct heap_node_t {
    header_t header;
    struct heap_node_t *parent;
    struct heap_node_t *links[2];
    // Use list_start to maintain doubly linked list, using the links[PREV]-links[NEXT] fields
    struct heap_node_t *list_start;
}heap_node_t;
```

Over the course of an allocator there are many free blocks of the same size that arise and cannot be coalesced because they are not directly next to one another. When coalescing left and right, we may still have a uniquely sized node to delete, resulting in the normal $\Theta(lgN)$ deletion operations. However, if the block that we coalesce is a duplicate, we are garunteed $\Theta(1)$ time to absorb and free the node from the doubly linked list. Here is one of the core functions for coalescing that shows just how easy it is with the help of our enum to switch between referring to nodes as nodes in a tree and links in a list.

```c
/* @brief free_coalesced_node  a specialized version of node freeing when we find a neighbor we
 *                             need to free from the tree before absorbing into our coalescing. If
 *                             this node is a duplicate we can splice it from a linked list.
 * @param *to_coalesce         the node we now must find by address in the tree.
 * @return                     the node we have now correctly freed given all cases to find it.
 */
heap_node_t *free_coalesced_node(heap_node_t *to_coalesce) {
    // Quick return if we just have a standard deletion.
    if (to_coalesce->list_start == black_sentinel) {
       return delete_rb_node(to_coalesce);
    }
    // to_coalesce is the head of a doubly linked list. Remove and make a new head.
    if (to_coalesce->parent) {
        header_t size_and_bits = to_coalesce->header;
        heap_node_t *tree_parent = to_coalesce->parent;
        heap_node_t *tree_right = to_coalesce->links[RIGHT];
        heap_node_t *tree_left = to_coalesce->links[LEFT];
        heap_node_t *new_head = to_coalesce->list_start;
        new_head->header = size_and_bits;
        // Make sure we set up new start of list correctly for linked list.
        new_head->list_start = new_head->links[NEXT];

        // Now transition to thinking about this new_head as a node in a tree, not a list.
        new_head->links[LEFT] = tree_left;
        new_head->links[RIGHT] = tree_right;
        tree_right->parent = new_head;
        tree_left->parent = new_head;
        new_head->parent = tree_parent;
        if (tree_parent == black_sentinel) {
            tree_root = new_head;
        } else {
            direction_t parent_link = tree_parent->links[RIGHT] == to_coalesce;
            tree_parent->links[parent_link] = new_head;
        }
    // to_coalesce is next after the head and needs special attention due to list_start field.
    } else if (to_coalesce->links[PREV]->list_start == to_coalesce){
        to_coalesce->links[PREV]->list_start = to_coalesce->links[NEXT];
        to_coalesce->links[NEXT]->links[PREV] = to_coalesce->links[PREV];
    // Finally the simple invariant case of the node being in middle or end of list.
    } else {
        to_coalesce->links[PREV]->links[NEXT] = to_coalesce->links[NEXT];
        to_coalesce->links[NEXT]->links[PREV] = to_coalesce->links[PREV];
    }
    return to_coalesce;
}
```

Here is the same picture from the earlier tree with these new linked nodes in the tree. We are able to save some meaningful bulk to the tree. Whereas before we could not even see half of the tree, now our whole tree prints nicely and is more shallow.

![rb-tree-shallow](/images/rb-tree-shallow.png)

This implementation comes out slightly faster than a normal red black tree. I only observed a 1% drop in overall utilization. However, I am sure that under the right use cases we would begin to suffer the cost of adding another field to the struct. This increases the internal fragmentation of the heap by creating hefty nodes with many fields. Small requests will chew through more memory than they need. However, I hypothesize that this approach would scale well when the number of free nodes grows significantly, especially if there are many duplicates. We get an absolute worst case operation of $\Theta(lgN)$ for any operation on the heap, but the garunteed best case for coalescing a duplicate becomes $\Theta(1)$. We also do not have to rebalance, recolor, or rotate the tree for calls to malloc if the best fit search happens to yeild a duplicate. I will have to put together a more thorough testing framework later to see if this implementation yeilds any benefits. Overall, this was an interesting exercise in thinking about the difference in how we interpret the connections between nodes.

### Unified Doubly Analysis

- **`free()`**- Freeing an takes one $\Theta(lgN)$ search to find the place for a new node. In this implementation however, we have a significant best case. If we find a node of the same size that we are inserting, we can insert it into the doubly linked list in $\Theta(1)$ time and we do not need to rotate or recolor the tree. If the node does not exist in the tree, we have the normal $\Theta(lgN)$ fixup operations.
- **`malloc()`**- Searching the tree for the best fitting block requires one $\Theta(lgN)$ search of the tree. Again, however, we can eliminate fixups if the best fit node is a duplicate. Then we have $\Theta(1)$ operations to take the duplicate from a doubly linked list.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. If the coalescing operation finds a duplicate node, it will be freed in $\Theta(1)$ time because I have setup the doubly linked list such that we know we are in the list and not the tree. We also have the node in the tree serving as the head and the sentinel black tree node serving as the tail. This makes coalescing a duplicate a constant time operation no matter where it is in the list. If we coalesce a unique node we have our normal $\Theta(lgN)$ fixup operations.
- **`style`**- This is one of the more readable and well decomposed implementation of all the allocators. The extra complexity of duplicates is handled through simple helper functions and the generalization of symmetric cases to directions and their opposites makes the code quite readable.

## Unified Stack

> **Read my code here ([`rbtree_stack.c`](/src/rbtree_stack.c)).**

If we now bring all of the strengths of the past improvements I have discussed, we can make a more space efficient, fast allocator that solves some of the challenges a heap allocator implementation poses for red black trees. One technique that is common when we want to save space in a red black tree is to eliminate the parent field. Instead, we use an array and treat it as a stack to represent the path down to the nodes we need to work with. This eliminates an eight byte pointer from the struct and in a normal red black tree might look something like this. Please note, I am using my array method for this implementation as well to eliminate symmetric cases.

```c
typedef struct heap_node_t {
    header_t header;
    struct heap_node_t *links[2];
}heap_node_t;
```

There is one problem with this approach for a heap allocator however. I could not think of a good way to coalesce nodes with the above struct. The problem arises with duplicates. When we insert duplicates into a red black tree, we know that they will always follow the rules of a binary search tree in relation to their parent, but we do not know where rotation may put them over the lifetime of the heap. This is a problem for a coalescing operation that needs to find the exact address of the node it needs.

When we search the tree by block size, as we normally do, there is no garuntee the first node of the correct size we find will be the one we are coalescing. This would result in a non sequential block of memory and corrupt the heap. So, we will simply take from our previous allocator, and store duplicates in a doubly linked list. Then we are free to use the stack, because we know that all nodes in the tree will be unique. We also have immediate access to coalesce any duplicate safely in constant time from its linked list. Here is the node. Here is the new tree scheme without a parent field. Now, the only way we know we are in the linked list is by setting the `list_start` field to `NULL` rather than `black_null`.

![rb-stack-scheme](/images/rb-duplicates-no-parent.png)

```c
typedef struct heap_node_t {
    // block size, allocation status, left neighbor status, and node color.
    header_t header;
    struct heap_node_t *links[2];
    struct heap_node_t *list_start;
}heap_node_t;
```

This allocator will be slightly slower than the previous allocator with a parent field, but more space efficient. This was also the most challenging of all the allocators for me to implement. It requires a strong handle on a spacial understanding of the changes that occur in a tree during rotations and transplants. We have to manage the array ourselves, accurately updating the nodes in the stack if they move or rotate during fixup operations and keep a good track of our index. It was challenging for me to apply the same structure of CLRS over a stack implmentation. I might recommend looking over the [`libval`](https://adtinfo.org/libavl.html/Red_002dBlack-Trees.html) implementation of a red black tree with a stack if you were to do this from scratch. It is a different approach and I struggled to read the code due to the naming convenients and lack of comments. However, I am sure it would be a good starting point to implement this technique. So, I stuck with the past implementations I had done and reworked them for this technique.

### Unified Stack Analysis

- **`free()`**- Freeing an allocated heap block takes one $\Theta(lgN)$ search to find the place for a new node. In this implementation however, we have a significant best case. If we find a node of the same size that we are inserting, we can insert it into the doubly linked list in $\Theta(1)$ time and we do not need to rotate or recolor the tree. If the node does not exist in the tree, we have the normal $\Theta(lgN)$ fixup operations.
- **`malloc()`**- Searching the tree for the best fitting block requires one $\Theta(lgN)$ search of the tree. Again, however, we can eliminate fixups if the best fit node is a duplicate. Then we have $\Theta(1)$ operations to take the duplicate from a doubly linked list.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. If the coalescing operation finds a duplicate node, it will be freed in $\Theta(1)$ time because I have setup the doubly linked list such that we know we are in the list and not the tree. We also have the node in the tree serving as the head and the sentinel black tree node serving as the tail. This makes coalescing a duplicate a constant time operation most of the time. However, there is one bad case that can slow us down without the parent field. If we coalesce the node that is in the tree and serves as the head of the doubly linked list, we have a problem. To delete that node, we need to find its parent so that we can link up the next node in the list as the tree node correctly. This requires a $\Theta(lgN)$ search of the tree to locate the parent node.
- **`style`**- This was a challenging implementation to complete, but it is able to loosely follow the structure of all other implementations up to this point. The greatest concern for this allocator is the readability of the stack management. To improve the style of this code it would be important to come up with a consistent method for rearranging and managing the stack during fixup operations. There are specific adjustments to the path in the stack that are required for rotations and deciding where and when to adjust the nodes in the array is an important style question.

## Unified Topdown

> **Read my code here ([`rbtree_topdown.c`](/src/rbtree_topdown.c)).**

Just when I thought that a stack was a space efficient implementation, but sacrificed on some speed, it turns out there is an optimal solution that mitigates the cost of forgoing a parent field. We can consider repairing a Red Black tree on the way down to a node that we need to insert or remove. This is an amazing idea that I read on Julienne Walker's [**`Eternally Confuzzled`**](https://web.archive.org/web/20141129024312/http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx) website. I could only find the archived website at this point, but I am glad I did. This algorithm solves most of the problems that come with forgoing the parent field in a red black tree. There are, however, significant modifications to Walker's implementation required to make the topdown algorithms work with a heap allocator.

This implementation will use the same array based struct to represent the `LEFT/RIGHT` and `NEXT/PREV`. We will also use a pointer to point to a doubly linked list of duplicate nodes, but more on that later. While in our previous stack based implementation we used an array to remember the path down to a node that we were inserting or deleting, a topdown approach only requires at most four pointers: `ancestor`, `grandparent`, `parent`, and `child`. These represent the greatest lineage that will be in need of repair as we go down the tree fixing rule violations that may arise. So, immediately we save space complexity by reducing our path representation to a constant factor.

The challenge to this approach is that it is not designed to handle duplicates. In fact, Walker explicitly states that insertion and deletion cannot accomodate duplicates. Another challenge arises in how Walker handles deletions. Instead of rewiring pointers to replace a node that needs to be transplanted in, Walker simply swaps the data in the node to be removed with its inorder predecessor. All of these factors present a challenge to this implementation. I need to be able to add and remove duplicates from the tree and I must repair all pointers for any insertion or removal request because users expect my functions to handle exactly the memory that they need or refer to. My heap also is made up of unique addresses that are finite and must be managed in memory exactly.

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
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. If the coalescing operation finds a duplicate node, it will be freed in $\Theta(1)$ time because I have setup the doubly linked list such that we know we are in the list and not the tree. We also have the node in the tree serving as the head and the sentinel black tree node serving as the tail. This makes coalescing a duplicate a constant time operation most of the time. This implmentation makes what was previously the worst possible case of coalescing, a much faster operation. If we coalesce a node that is unique, we simply perform a normal deletion fixing the tree on the way down. Once we remove the node we need and transplant it with the inorder predecessor, we are done and the tree is valid. If we coalesce a node that is in the tree but the head of the doubly linked list of duplicates, we only need to perform one $\Theta(lgN)$ search to find its parent in order to transplant the next node in the doubly linked list into the tree. No fixup operations are necessary on the way down.
- **`style`**- The ideas of this algorithm are beautiful. The implmentation is not. This code is currently riddled with long functions, an infinite loop, `break` and `continue` statements, and numerous `if`'s for a multitude of scenarios. If you ever have a chance to read Walker's original work, I would recommend it. Walker's implementation is elegant because Walker has constraints on duplicates and can copy data between nodes. Because I have to manage memory addresses directly and reconnect pointers, the cases I consider increase and functions are more dificult to decompose. I will try to discuss this with more experienced developers to see what strategies are possible to simplify the code. The problems this code solves for this particular implmentation are great. I never have more than one $\Theta(lgN)$ operation for any given request to the heap, versus the two $\Theta(lgN)+\Theta(lgN)$ operations that all other implementations up to this point had in the worst case. However, I need to consider how to improve the style and readability of the code.

## Runtime Analysis

Now that we have gone over the basics for each heap allocator, we can see how they perform in relation to one another. Let's start by gaining some perspective in terms of how much faster a Red Black Tree implementation is compared to a simple doubly linked list. While we have not discussed the doubly linked list implementation in detail, it is a simple implementation that involves maintaining a sorted doubly linked list by block size and taking the best fit block for any given request. It is a common introduction to allocators, and if you wish for more details there are numerous articles and college assignments that will start with this implementation. A doubly linked list has a worst case of $\Theta(N)$ for any single request from `free()` to insert a block into the list and $\Theta(N)$ for any request to `malloc()` to free the block from the list.

![list-v-tree](/images/list-v-tree.png)

*Pictured Above: A chart representing the time to complete (N) insert delete requests. The first implementation is a simple doubly linked list that maintains sorted order by free block size to acheive best fit. The second red line that is hardly visible at the bottom is a Red Black Tree that maintains sorted order to acheive best fit.*

Here are the key details from the above graph.

- The time complexity of inserting and deleting N elements into and out of the list is $\Theta(N^2)$.
  - There may also be some intermittent extra work every time we delete a node from the list because we split of any extra part of a block that is too big and re-insert it into the list.
- As the number of free blocks grows in size the list becomes increasingly impractical taking over a minute to cycle through all requests at the peak of requests.
- The tree implementation is hardly measureable in comparison to the list. Not only do we never exceed one second to service any number of requests on this chart, but also the time scale of seconds is not informative.

So, for further analysis of our tree implementations, we need to reduce our time scale by a factor of 1,000 and compare implementations in milliseconds. This will reveal interesting differences between the five Red Black Tree allocators.

### Testing Methodology

The following test methods and analysis are not targeted towards any specific applications. If these allocators were to be implemented in any serious application more testing would need to occur. These tests, instead, are targeted towards the general performance of the Red Black Tree algorithms and help address some areas of interest I hypothesized about while trying the different strategies outlined in each allocator. Mainly, I am most curious about the following.

- Is there any cost or benefit to uniting the left and right cases of Red Black trees into one case with the use of an array of pointers?
- Does the approach of storing duplicate node sizes in a linked list benefit performance by trimming the tree?
- When we abandon the parent field of the tree node in exchange for the linked list field, do we sacrifice performance?
- Does a topdown approach to fixing a Red Black Tree reduce the number of $\Theta(lgN)$ operations enough to make a difference in performance?

In order to answer these questions I wrote a `time-harness.c` program based upon the `test-harness.c` program the Stanford course staff wrote for us. The original `test-harness.c` program was focussed on correctness. Along with our own debugging and printing functions, the goal of the test harness was to be as thorough as possible in finding bugs quickly and ensuring our allocators functioned properly. This meant that the programs produced for each allocator were painfully slow. There were numerous $\Theta(N)$ operations built into the provided program and I added many more $\Theta(N)$ checks into my own implementations. For a red-black tree these checks only grow as the necessity to ensure the rules of a Red Black Tree are followed becomes critical.

For the `time-harness.c` implementation, we abandon all safety and correctness checks. We should only use this program once we know beyond a doubt that the implementations are correct. We acheive faster testing by leaving all "client" available memory uninitialized, performing no checks on the boundaries of the blocks we give to the user, and cutting out any unecessary $\Theta(N)$ operations from the program while it calls upon the functions in our allocators. We do not want $\Theta(N)$ work clouding the runtime efficiency of what would otherwise be $\Theta(lgN)$ operations, for example. This results in the ability to check the performance of requests ranging anywhere from 5,000 to 1,000,000 and beyond quickly.

While it would have been nice to time our code with a simple `time` command and call it a day, we need a custom program like this is because timing specific functions of an allocator can be tricky for two main reasons: allocations and coalescing. In order to create a tree of $N$ nodes of free memory for the user, you may think we could just create $N$ allocated blocks of memory and then free them all, thus inserting them into the tree. This would not work. Because we coalesce blocks to our left and right by address in memory, when we went to free the next allocation, it would absorb its left neighbor and simply grow in size with each `free()` call and be inserted back into the tree. We would end up with our original large pool of free memory by the end of all `free()` requests.

Instead, we will allocate $2N$ blocks of memory and then call `free()` on every other block of allocated memory to measure the performance of $N$ insertions into a tree. Coalescing will not occur on the two adjacent blocks because they remain allocated.

The time it takes to make all of these allocations is also not of interest to us if we want to measure insertion and removal from our tree, so we need to be able to time our code only when the insertions and removals begin. To do this, we need to rely on the `time.h` C library and start a `clock()` on the exact range of requests we are interested in. We can acheive this by looking at our scripts and asking the `time-harness.c` program to time only a specific line numbers representing requests.

```bash
$ ./obj/time_rbtree_clrs -s 10001 -e 20000 scripts/time-05kinsertdelete.script
```

- The above command is for timing five thousand insertions, `free()`, and five thousand removals, `malloc()`, from our tree.
- Notice that we use the `-s` flag to start timing on a line number and the `-e` flag to end our timer.
- We also must start our timer after ten thousand `malloc()` requests to set up a scenario where we can insert five thousand nodes into the tree.
- This program can time multiple sections of a script as long as they do not overlap and will output the timing result of all requested sections.

We would then get output such as the following.

```output
Evaluating allocator on time-05kinsertdelete.script...
Execution time for script lines 10001-20000 (seconds): 0.000760
...successfully serviced 30000 requests. (payload/segment = 2514877/2969215)
Utilization averaged 84%
```

For more details about how the timing functions work or how the program parses arguments please see the code.

> **Read my code here ([`time_harness.c`](/src/time_harness.c)).**

### Inserting and Deleting

To set up a measurement of inserting and deleting into a Red-Black Tree we start by forming a script with $2N$ allocations and then measure the time to insert $N$ elements, `free()`, and delete $N$ elements, `malloc()` from our tree. Remember, we have now dropped down to the milliseconds level, compared to the seconds level we used when analyzing a doubly linked list allocator. Here are the results across allocators.

![insert-delete](/images/chart-insert-delete.png)

*Pictured Above: The five allocators compared on a runtime graph for insert delete requests. The time is now measured in milliseconds, while the number of requests remains the same as our previous comparison.*

Here are the key details from the above graph.

- Lower time in milliseconds is better.
- The time complexity of inserting and deleting N elements into our tree is $\Theta(NlgN)$.
  - We may also see some extra time cost from breaking off extra space from a block that is too large and reinserting it into the tree. However, this is hard to predictably measure and does not seem to have an effect on the Big-O time complexity.
- We see an improvement exceeding 50% in runtime speed when we manage duplicates with a doubly linked list to trim the tree.
- The topdown approach to managing a Red-Black tree is not the fastest in terms of inserting and deleting from the tree.

Recall from the allocator summaries that we are required to manage duplicates in a Red-Black tree if we decide to abandon the parent field of the traditional node. The two approaches that trade the parent field for the pointer to a linked list of duplicates perform admirably: `rbtree_topdown` and `rbtree_stack`. The topdown approach is still an improvement over a traditional Red-Black tree, but surprisingly, the stack implementation that relies on the logical structure of the CLRS Red-Black tree is the winner in this competition. While I thought the `rbtree_linked` approach that uses a field for the `*parent`, `links[LEFT]`, `links[RIGHT]`, and `*list_start` would be the worst in terms of space efficiency, I thought we would gain some speed overall. We still need to measure coalescing performance, but this makes the `rbtree_linked` implmentation seem the most pointless at this point if the same speed can be acheived with more space efficiency.

Finally, the performance gap scales as a percentage, meaning we will remain over 50% faster when we manage duplicates to trim the tree even when we are dealing with millions of elements and our times have increased greatly. Managing duplicates in a linked list with only the use of one pointer is a worthwile optimization. Revisit any summary of the last three allocators if you want more details on how this optimization works.

### Coalescing

Coalescing left and right in all implementations occurs whenever we `free()` or `realloc()` a block of memory. One of my initial reasons for implementing doubly linked duplicate nodes was to see if we could gain speed when we begin coalescing on a large tree. I hypothesized that heap allocators have many duplicate sized nodes and that if we reduce the time to coalesce duplicates to $\Theta(1)$ whenever possible, we might see some speedup in an otherwise slow operation.

In order to test the speed of coalescing across implementations we can set up a similar heap to the one we used for our insertion and deletion test. If we want to test the speed of $N$ coalescing operations, we will create $2N$ allocations. Then, we will free $N$ blocks of memory to insert into the tree such that every other allocation is freed to prevent premature coalescing. Finally, we will call `realloc()` on every allocation that we have remaining, $N$ blocks. My implementation of `realloc()` and `free()` coalesce left and right every time, whether the reallocation request is for more or less space. I have set up a random distribution of calls for more or less space, with a slight skew towards asking for more space. This means we are garunteed to left and right coalesce for every call to `realloc()` giving us the maximum possible number of encounters with nodes that are in the tree. We will find a number of different coalescing scenarios this way as mentioned in the allocator summaries.

If we are using a normal Red Black Tree with a parent field, we treat all coalescing the same. We free our node from the tree and then use the parent field to perform the necessary fixups. At worst this will cost $\Theta(lgN)$ operations to fix the colors of the tree. For the `rbtree_linked` implementation, where we kept the parent field and added the linked list as well, any scenario in which we coalesce a duplicate node will result in an $\Theta(1)$ operation. No rotations, fixups, or searches are necessary. This is great! However, if we have abandoned the parent field and are managing duplicates in a linked list we have a few scenarios that are possible.

- Case 1: We coalesce a normal node in the tree.
  - Remove the node and repair the tree.
- Case 2: We coalesce a duplicate node that is the head of the doubly linked list.
  - Find the node's parent and link it to the next node in the list.
- Case 3: We coalesce a duplicate node that is somewhere in the doubly linked list.
  - Remove the node from the list. No searches or fixups necessary.

As you can see, cases 1 and 2 will cost us some time, even with the optimization of a doubly linked list. Case 2 is the only downside to abandoning the parent field when we must coalesce blocks of memory. We can see these costs and benefits reflected in the runtime plot of the data.

![chart-realloc-free](/images/chart-realloc-free.png)

Here are the key details from the above graph.

- The runtime of $N$ reallocations is $\Theta(NlgN)$.
- The `rbtree_linked` implementation with nodes that have a `*parent`, `links[LEFT-RIGHT]`, and `*list_start` field is the clear winner. The number of $\Theta(1)$ encounters with duplicates reduces the overall runtime of the allocator significantly.
- The stack approach is again a solid balance of space-efficiency and speed.
- The topdown approach must fix the tree while it goes down to remove a node, thus costing time due to extra work.
- The `rbtree_unified` implementation only differs from the `rbtree_clrs` implementation in that unifies the left and right cases of a Red Black tree using an array in one of the node fields. Yet, it is faster in this application.

These results make the most wasteful implementation in terms of space, `rbtree_linked`, earn some credit it terms of its raw speed. In these artificial tests, we can claim that it is the fastest implementation in terms of inserting, deleting, and coalescing. The only problem is that it requires four fields in any node to be implemented.

I also have lingering questions about why the `rbtree_unified` implementation is slower than the traditional `rbtree_clrs` implementation in insertions and deltions, but faster here. However, an undeniable conclusion from both sets of tests so far is that it is worth managing duplicate nodes in a linked list. The fact that we are required to do so when we abandon the parent field is a blessing in disguise in terms of speed. To get an idea of just how many duplicates the allocators may encounter in these artificial workloads, here is a picture of the tree configuration for 50,000 insert delete requests at the peak of the tree size.

![50k-insert-delete](/images/rb-tree-50k-insertdelete.png)

*Pictured Above: A Red Black tree after 50,000 insertions of nodes into the tree. Notice how many duplicates we have and imagine thest duplicates spread across an entire tree if we did not manage them in a list.*

### Tracing Programs

While the initial artificial tests were fun for me to lab and consider in terms of how to best measure and execute parts of scripts, we are leaving out a major testing component. Real world testing is key! I will reiterate, I have no specific application in mind for any of these allocators. This makes choosing the programs to test somewhat arbitrary. I am choosing programs or Linux commands that are interesting to me and I will seek some variety wherever possible. If this were a more serious piece of research, this would be a key moment to begin forming the cost and benefit across a wide range of applications.

To trace programs we use the `ltrace` command (thank you to my professor of CS107 at Stanford, Nick Troccoli for giving me the tutorial on how to trace memory usage in Linux). This command has many options and one set of instructions will allow you to trace `malloc`, `calloc`, `realloc`, and `free` calls like this.

```bash
ltrace -e malloc+realloc+calloc -e free-@libc.so* --output [FILENAME] [COMMAND]
```

If we then write a simple parsing program we can turn the somewhat verbose output of the ltrace command into a cleaned up and more simple `.script` file. If you would like to see how that parsing is done, read my implementation in Python.

> **Read my code here ([`parsing.py`](/pysrc/parsing.py)).**

#### Linux Tree Command

The Linux `tree` command will print out all directory contents in a tree structure from the current directory as root. This is similar to my tree printing function you have seen me use to illustrate Red Black trees throughout this write up. There are many options for this command but I am choosing to start in the `home/` directory of Ubuntu on WSL2 and use the `-f` and `-a` flags which will output full paths and hidden files for all directories. Again, for perspective here is the full comparison with the list allocator.

![chart-tracetree](/images/chart-tracetree.png)

It is again unproductive to discuss a speed comparison between the Red Black Tree allocators on this scale, so we will drop the list allocator for further comparison.

![chart-rbtracetree](/images/chart-rbtracetree.png)

Here are the key details from the above graph.

- Requests for the `tree` command consist mostly of `malloc()` and `free()` and the free tree remains small over the course of the program's lifetime.
- The `rbtree_unified` implementation is surprisingly slow in this scenario.
- Again, no compromises in the `rbtree_linked` implementation make it fast, even if it is not space efficient.

#### Neovim

Neovim is my editor of choice for this project. It is a text editor based off of Vim and we can actually trace its heap usage while we use it to edit a document. We will trace the heap usage while we edit the README.md for the project. This program usage is similar to the Linux Tree command with many `malloc()` and `free()` calls. However, among the roughly one million requests to the heap for a short editing session, there are also twenty-eight thousand calls to realloc, so we have more diverse heap usage.

![chart-nvim](/images/chart-tracenvim.png)

Here are the key observations from the above graph.

- The traditional CLRS implementation of a Red Black tree will perform well in any application where there are many calls `malloc()` and `free()` and the tree remains small.
- The extra complexity of managing duplicates speeds up the tree slightly when the tree remains small.

#### Utilization

While it was not productive to compare speeds of the list allocator to the tree allocator, the list allocator can show value in its high utilization due to low overhead to maintain the list. Let's compare utilization across the insertion/deletion, coalescing, and tracing applications.

![chart-utilization](/images/chart-utilization.png)

Here are the key details from the above graph.

- While slow, the doubly linked list allocator has excellent utilization.
- The implementations of the `rbtree_clrs`, `rbtree_unified`, `rbtree_stack`, and `rbtree_topdown` are different in terms of the details of their code and algorithms. However, they have the exact same space used in their nodes, therefore the utilization is identical.
- Perhaps this view makes the cost of the speed gained by the `rbtree_linked` approach more apparent.

### Conclusions

Let's revisit the questions I was most interested in when starting the runtime analysis of these allocators.

- Is there any cost or benefit to uniting the left and right cases of Red Black trees into one case with the use of an array of pointers?
  - **Our initial runtime informations suggests that accessing pointers to other nodes in the tree from an array is slightly slower. We can explore the assembly of the traditional node versus the array based node to see for sure where the instruction counts expand. This is a possible extension to pursue in this project.**
- Does the approach of storing duplicate node sizes in a linked list benefit performance by trimming the tree?
  - **Absolutely. In fact, if you wish to make no compromises in terms of performance then simply add the linked list pointer to the traditional node. However, if you want a more space efficient approach use an explicit stack to track lineage of the tree, and store duplicates in a linked list pointer.**
- When we abandon the parent field of the tree node in exchange for the linked list field, do we sacrifice performance?
  - **If we use the CLRS algorithm to fix the tree with a bottom up approach, we do not lose much speed. However when we fix the tree with a topdown approach, we will lose some time even though we have eliminated the need for a parent field. This seems to be because we do extra work whenever we fix the tree on the way down, and our bottom up approach may not perform the worst case number of fixups on every execution.**
- Does a topdown approach to fixing a Red Black Tree reduce the number of $\Theta(lgN)$ operations enough to make a difference in performance?
  - **Yes. The difference is that the topdown algorithm is slower than the bottom up algorithm. Perhaps the added logic of handling duplicates and needing to rewire pointers slows down this implementation of an allocator initially intended for no duplicates and transplanting removed nodes by copying data from the replacement node to the one being removed.**

For further exploration, I would be interested in finding real world programs with more diverse heap calls, such as `realloc()`. The current real world tests were mostly calls to `malloc()` and `free()`, keeping our tree small.

