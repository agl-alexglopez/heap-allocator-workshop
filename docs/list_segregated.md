# Segregated List

## A Simple List

Let's first start with one of the simplest schemes we can use for a heap allocator.

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

While the previous doubly linked list will work in many cases and is simple to implement, we can do better. With a strategy called Segregated Fits, we can massively decrease our runtime speed while improving utilization in many cases. When we begin managing our heap, we can decide to make an array of doubly linked lists. There are many ways to organize this array, but one approach is to organize it by increasing size. We can store nodes that fit in a size range for a particular list. For smaller sizes we can choose to maintain a list of one size. However, once we reach a size of 8 bytes, we can start doubling our sizes that serve as the minimum for each list. While I won't go over the details here, this doubling in base 2 allows us to make some interesting optimizations you can check out in the code. Here is the high level scheme of this list of lists.

![list-segregated](/images/list-segregated.png)

*Pictured Above: An abstract layout of our lookup table. Notice that the nodes in the actual lists are not sorted. We rely on our loose sorting across all lists and add nodes to the front of each list to speed things up.*

However, this list is somewhat long. There are 20 slots and they start at one byte. This is not practical in the scheme of a heap allocator because our minimum block size we can accommodate or store starts at 32 bytes. So let's take a look at how this scheme actually looks in a heap allocator with properly sized segregated lists.

![list-segregated-real](/images/list-segregated-real.png)

*Pictured Above: The segregated lists that store various block sizes in our heap. Notice that the first four lists increment by 8 because that is the alignment we hold all blocks to for performance.*

Read the runtime analysis section for a detailed breakdown of how this more clever take on a doubly linked list stacks up against other allocators. I am still considering modifying this implementation to try to bring it more in parity with the performance of the `libc` default allocator.

## Performance Analysis

- **`free()`**- Freeing an allocated is a constant time operation because I add nodes to the beggining of a bucket. I may implement an inorder version later and this runtime would change if I had to maintain sorted lists. The operation is constant because we don't even need to search all of the buckets. We use a built in intrinsic to find the leading zeros of our block size and jump directly to the correct bucket.
- **`malloc()`**- Searching is variable. My bucket lists are not sorted and we take the first fit we can find. Our worst case is O(N) where N is the number of nodes in a bucket as divided by our size delineations.
- **`coalesce()`**- Both `free()` and `realloc()` coalesce left and right to maximize on reclaimed space. Luckily because we use doubly linked lists splicing is constant time and inserting is constant time.
- **`style`**- This implementation is readable and clear but has the most room for improvement. I know that `libc` uses something similar but I am not sure the exact details. However, I should be maintaining better performance than I am now and will try to improve this allocator in the future.


