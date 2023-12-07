## Testing Methodology

The following test methods and analysis are not targeted towards any specific applications. If these allocators were to be implemented in any serious application more testing would need to occur. These tests, instead, are targeted towards the general performance of the Red Black Tree algorithms and help address some areas of interest I hypothesized about while trying the different strategies outlined in each allocator. Mainly, I am most curious about the following.

- Is there any cost or benefit to uniting the left and right cases of tree based allocators into one case with the use of an array of pointers?
- Does the approach of storing duplicate node sizes in a linked list for tree based allocators benefit performance by trimming the tree?
- When we abandon the parent field of the tree node in exchange for the linked list field, do we sacrifice performance?
- Does a top-down approach to fixing a Red Black or Splay Tree reduce the number of O(lgN) operations enough to make a difference?

In order to answer these questions I wrote a `stats.cc` and `plot.cc` program based on the `ctest.cc` file. The original `ctest.cc` program was focussed on correctness. Along with our own debugging and printing functions, the goal of the test harness was to be as thorough as possible in finding bugs quickly and ensuring our allocators functioned properly. This meant that the programs produced for each allocator were painfully slow. There were numerous O(N) operations built into the provided program and I added many more O(N) checks into my own implementations. For a tree based allocators these checks only grow as the necessity to ensure invariants are upheld.

For the `stat.cc` implementation, we abandon all safety and correctness checks. We should only use this program once we know beyond a doubt that the implementations are correct. We acheive faster testing by leaving all "client" available memory uninitialized, performing no checks on the boundaries of the blocks we give to the user, and cutting out any unecessary O(N) operations from the program while it calls upon the functions in our allocators. We do not want O(N) work clouding the runtime efficiency of what would otherwise be O(lgN) operations, for example. This results in the ability to check the performance of requests ranging anywhere from 5,000 to 1,000,000 and beyond quickly.

While it would have been nice to time our code with a simple `time` command and call it a day, we need a custom program like this is because timing specific functions of an allocator can be tricky for two main reasons: allocations and coalescing. In order to create a pool of N nodes of free memory for the user, you may think we could just create N allocated blocks of memory and then free them all, thus inserting them into the data structure. This would not work. Because we coalesce blocks to our left and right by address in memory, when we went to free the next allocation, it would absorb its left neighbor and simply grow in size with each `free()` call and be inserted back into the pool. We would end up with our original large pool of free memory by the end of all `free()` requests.

Instead, we will allocate 2N blocks of memory and then call `free()` on every other block of allocated memory to measure the performance of N insertions into a tree. Coalescing will not occur on the two adjacent blocks because they remain allocated.

The time it takes to make all of these allocations is also not of interest to us if we want to measure insertion and removal from our tree, so we need to be able to time our code only when the insertions and removals begin. To do this, we start a `clock()` on the exact range of requests we are interested in. We can acheive this by looking at our scripts and asking the `stats.cc` program to time only specific line numbers representing requests.

```bash
 ./build/rel/stats_rbtree_clrs -r 200000 300000 scripts/time-insertdelete-100k.script
```

- The above command is for timing 100 thousand insertions, `free()`, and 100 thousand removals, `malloc()`, from our tree.
- Notice that we use the `-r` flag to starts a timing range. An unterminated range assumes we should time the remainder of the script from that point.
- We also must start our timer after many `malloc()` requests to set up a scenario where we can insert 100 thousand nodes into the tree.
- This program can time multiple sections of a script as long as they do not overlap and will output the timing result of all requested sections.
- We will get a helpful graphs that highlight key statistics about the heap as well.

I then hardcoded all of these commands that I wanted into the `plot.cc` file and ran each command as a subprocess with a parent thread communicating with the child via a pipe. We then get output related to time to complete the interval range, average response time, and overall utilization. Here is a response time plot for example.

![plot-output](/images/plot-output.png)

Notice also that the default allocator of `libc` is included for comparison which is fun. However, to be clear, **regardless of performance metrics you may see here, `libc` implements a superior allocator**. This analysis is not trying to claim that any allocator *beats* `libc`. The default allocator is thread safe and probably one of the most well tested, robust pieces of software in use today. Its ubiquity puts far more stringent requirements on it to be solid than anything I wrote and even with all of those requirements it is still **very fast**. It's just fun to see how different ideas stack up. For more details about how the timing functions work or how the program parses arguments please see the code.

## Inserting and Deleting

To set up a measurement of inserting and deleting into a Red-Black Tree we start by forming a script with 2N allocations and then measure the time to insert N elements, `free()`, and delete N elements, `malloc()` from our tree. Remember, we have now dropped down to the milliseconds level, compared to the seconds level we used when analyzing a doubly linked list allocator. Here are the results across allocators.

![insert-delete-full](/images/insert-delete-full.png)

*Pictured Above: The six allocators compared on a runtime graph for insert delete requests. The time is now measured in milliseconds.*

As you can see, the `list_segregated` allocator has strong performance, suffering from more erratic runtime speed as the size of the lists in the lookup table grows. I think I can implement some improvements later as the default `libc` allocator is roughly based on a segregated fits table as well and they are doing great! Let's exclude the outlier.

![insert-delete](/images/insert-delete-zoom.png)

*Pictured Above: The six allocators compared on a runtime graph for insert delete requests with a focus on the tree allocators.*

Here are the key details from the above graph.

- Lower time in milliseconds is better.
- The time complexity of inserting and deleting N elements into our tree is O(NlgN).
  - We may also see some extra time cost from breaking off extra space from a block that is too large and reinserting it into the tree. However, this is hard to predictably measure and does not seem to have an effect on the Big-O time complexity.
- We see a slight improvement in runtime speed when we manage duplicates with a doubly linked list to trim the tree.
- The top-down approach to managing a Red-Black and Splay tree is not the fastest in terms of inserting and deleting from the tree, but it is extremely competitive.
- These allocators clearly outperform the standard segregated fits allocator. However, it is tricky to pin down the runtime time complexity of the segregated list allocator exactly. It approaches an O(N^2) time complexity.

Recall from the allocator summaries that we are required to manage duplicates in a Red-Black and Splay tree if we decide to abandon the parent field of the traditional node. The approaches that trade the parent field for the pointer to a linked list of duplicates perform admirably: `rbtree_top-down` and `rbtree_stack`. The top-down approach is still an improvement over a traditional trees, but surprisingly, the stack implementation that relies on the logical structure of the CLRS Red-Black tree is also close. While I thought the `rbtree_linked` approach that uses a field for the `*parent`, `links[LEFT]`, `links[RIGHT]`, and `*list_start` would be the worst in terms of space efficiency, I thought we would gain some speed overall and that seems to be the case. We still need to measure coalescing performance, but this makes the `rbtree_linked` implementation seem the most pointless at this point if the same speed can be achieved with more space efficiency or other approaches. It is truly back and forth for first place among the three top allocators.

Managing duplicates in a linked list with only the use of one pointer is a worthwhile optimization. Revisit any summary of the last three allocators if you want more details on how this optimization works.

On a per request basis, we can observe the speedup in an allocator like `rbtree_linked`. Here is a graph illustrating the average response time over the same interval.

![response-zoom](/images/response-zoom.png)

*Pictured Above: Average response time over a malloc/free interval.*

## Coalescing

Coalescing left and right in all implementations occurs whenever we `free()` or `realloc()` a block of memory. One of my initial reasons for implementing doubly linked duplicate nodes was to see if we could gain speed when we begin coalescing on a large tree. I hypothesized that heap allocators have many duplicate sized nodes and that if we reduce the time to coalesce duplicates to O(1) whenever possible, we might see some speedup in an otherwise slow operation due to the many other details it must manage in the process.

In order to test the speed of coalescing across implementations we can set up a similar heap to the one we used for our insertion and deletion test. If we want to test the speed of N coalescing operations, we will create 2N allocations. Then, we will free N blocks of memory to insert into the tree such that every other allocation is freed to prevent premature coalescing. Finally, we will call `realloc()` on every allocation that we have remaining, N blocks. My implementation of `realloc()` and `free()` coalesce left and right every time, whether the reallocation request is for more or less space. I have set up a random distribution of calls for more or less space, with a slight skew towards asking for more space. This means we are garunteed to left and right coalesce for every call to `realloc()` giving us the maximum possible number of encounters with nodes that are in the tree. We will find a number of different coalescing scenarios this way as mentioned in the allocator summaries.

If we are using a normal Red Black Tree with a parent field, we treat all coalescing the same. We free our node from the tree and then use the parent field to perform the necessary fixups. At worst this will cost O(lgN) operations to fix the colors of the tree. For the `rbtree_linked` implementation, where we kept the parent field and added the linked list as well, any scenario in which we coalesce a duplicate node will result in an O(1) operation. No rotations, fixups, or searches are necessary. This is great! For the `rbtree_stack`, `rbtree_top-down`, and both `splaytree_` implementations, we were able to acheive the same time complexity by thinking creatively about how we view nodes and how they store the parent field if they are a duplicate. Both of those allocators represent advanced optimizations in speed and space efficiency. The only disadvantage to the `rbtree_stack` allocator, when compared to the `rbtree_linked` allocator, is that if we coalesce a unique node we must launch into a tree search to build its path in a stack in preparation for fixups. The splay tree allocators also have some interesting tradeoffs with regards to coalescing which you can read about in their design documents. The `rbtree_linked` implementation is able to immediately start fixups because it still has the parent field, even in a unique node.

![chart-realloc-free-full](/images/realloc-plot.png)

*Pictured Above: The six allocators compared on a runtime graph for realloc requests.*

![chart-realloc-free](/images/realloc-response.png)

*Pictured Above: The allocators compared for average response time over increasing ranges of requests with increasing allocated nodes.*

Here we see the first clear cut case where these allocators beat the speed of `libc`. This makes sense because coalescing logic is where I put most of my attention towards. My list segregated allocator also should not have too much trouble with this operation because it currently inserts nodes into buckets at the front which is a constant time operation versus its slower first fit `malloc` search. It is also impotant to asses the response time of `realloc`. This is because `realloc` is often thought to be a bottleneck function due to the higher instruction counts you may be dealing with, especially regarding the requirement to possibly `memmove` or `memcpy` user data to a new location. So, I believe `realloc` is a great place to look to optimize and try to eliminate operations that are not constant time when possible.

Here are the key takeaways.

- The runtime of N reallocations is O(NlgN).
- The `splaytree_topdown` and rbtree_linked` implementations are the clear winners here. The number of O(1) encounters with duplicates reduces the overall runtime of the allocator significantly.
- The stack approach is again a solid balance of space-efficiency and speed. However, it seems that accessing an array to get the nodes you need is a factor slowing this implementation down compared to `rbtree_linked`. The search to build the stack when we coalesce a unique node also shows up as a time cost without the `parent` field. There is also added complexity in how `rbtree_stack` and `rbtree_top-down` store the parent field, increasing instruction counts.
- The top-down approach must fix the tree while it goes down to remove a node, thus costing time due to extra work, but it is no worse than a standard implementation in this context.
- The traditional `clrs` and `unified` red black trees slow down a little here because they do not benefit from any coalescing operations and will always perform their O(lgN) fixup operations.

These results make the most wasteful implementation in terms of space, `rbtree_linked`, more of a proof of concept. If the similar results can be achieved with a variety of more space efficient implementations.

A conclusion from both sets of tests so far is that it is worth managing duplicate nodes in a linked list. The fact that we are required to do so when we abandon the parent field is a fruitful challenge in terms of speed. To get an idea of just how many duplicates the allocators may encounter in these artificial workloads, here is a picture of the tree configuration for 50,000 insert delete requests at the peak of the tree size.

![50k-insert-delete](/images/rb-tree-50k-insertdelete.png)

*Pictured Above: A Red Black tree after 50,000 insertions of nodes into the tree. Notice how many duplicates we have and imagine these duplicates spread across an entire tree if we did not manage them in a list.*

## Tracing Programs

While the initial artificial tests were fun for me to lab and consider in terms of how to best measure and execute parts of scripts, we are leaving out a major testing component. Real world testing is key! I will reiterate, I have no specific application in mind for any of these allocators. This makes choosing the programs to test somewhat arbitrary. I am choosing programs that are interesting to me and I will seek some variety wherever possible. If this were a more serious piece of research, this would be a key moment to begin forming the cost and benefit across a wide range of applications.

To trace programs we use the `ltrace` command. This command has many options and one set of instructions will allow you to trace `malloc`, `calloc`, `realloc`, and `free` calls like this.

```bash
ltrace -e malloc+realloc+calloc -e free-@libc.so* --output [FILENAME] [COMMAND]
```

If we then write a simple parsing program we can turn the somewhat verbose output of the `ltrace` command into a cleaned up and more simple `.script` file. If you would like to see how that parsing is done, read my implementation in Python. I am having the most trouble with this section in finding programs that have interesting heap usage to trace. More often then not it will be a long series of `malloc` requests followed by `free` requests. It is more rare to find frequent `realloc` requests to really exercise the code paths.

### Cargo Build

Cargo is the package manager and build tool for the rust programming language. Luckily, it has *some* diverse heap usage when building a project with a decent chunk of `realloc` requests. However, overwhelmingly the majority of calls are to malloc. Yet, we can check out how our different allocators would be able to service the usage pattern of the Cargo Build tool. In a [maze-tui](https://github.com/agl-alexglopez/maze-tui) application I wrote in rust, I compiled the project with the `cargo build --release` flag and traced the results.

![cargo-interval](/images/cargo-interval.png)

And the average response time.

![cargo-response](/images/cargo-response.png)

I am most interested to see how the splaytree allocators perform on future scripts because their heuristic is that frequently requested items are brought closer to the root through splay operations. So, my hypothesis was that requests to the heap for consistently similar or the same sizes may improve performance.

### Utilization

There is not much interesting to report here. The utilization of most allocators is the same or very similar. Any allocators that have the same size for free nodes and headers will have the same utilization and other differences may be hard to spot. One note is that I currently do not have a way to fin the utilization of libc. However, I think they provide some opaque pointers or functions that may help with this. However, that requires more research. Here is the utilization for the `time-trace-cargobuild.script`.

![cargo-utilization](/images/cargo-utilization.png)

We do notice the overhead of the extra space used by `rbtree_linked` to have an impace on utilization and the simple doubly linked list of the `segregated_list` allocator is a benefit. All other tree based allocators use the same sized nodes, even if the fields sometimes represent different concepts such as duplicates versus parents.

## Conclusions

Let's revisit the questions I was most interested in when starting the runtime analysis of these allocators.

- Is there any cost or benefit to uniting the left and right cases of trees into one case with the use of an array of pointers?
  - **Our initial runtime information suggests that accessing pointers to other nodes in the tree from an array is slightly slower. We can explore the assembly of the traditional node versus the array based node to see for sure where the instruction counts expand. This is a possible extension to pursue in this project. However, the improvements to readability and the elimation of another branch where we can make implementation mistakes and typos is valueable.**
- Does the approach of storing duplicate node sizes in a linked list benefit performance by trimming the tree?
  - **Absolutely. In fact, if you wish to make no compromises in terms of performance then simply add the linked list pointer to the traditional node. However, if you want a more space efficient approach use an explicit stack to track lineage of the tree, and store duplicates in a linked list pointer. Or perhaps look to topdown splay trees and top down red black trees for some clever techniques to avoid the stack.**
- When we abandon the parent field of the tree node in exchange for the linked list field, do we sacrifice performance?
  - **If we use the CLRS algorithm to fix the tree with a bottom up approach, we do not lose much speed. However when we fix the tree with a top-down approach, we will lose some time even though we have eliminated the need for a parent field. This seems to be because we do extra work whenever we fix the tree on the way down, and our bottom up approach may not perform the worst case number of fix-ups on every execution. The topdown splay tree and splay trees overall are also confronted with some interesting challenges when we need to do a best fit rather than exact fit search.**
- Does a top-down approach to fixing a tree reduce the number of O(lgN) operations enough to make a difference in performance?
  - **This is still hard to definitively answer. In reading literature on topdown versions of algorithms, they are advertised as the clearly superior method for tree management becuase they eliminate a pass. However, in the context of heap allocators the benefits are not as clear cut. Both bottom up and topdown algorithms are extremely close to each other in terms of performance characteristics. In some cases, as in that of the splay tree we are also prevented from implementing a pure topdown algorithm becuase we need to accomodate a best fit search in a data structure that is not usually made to support duplicates or best fit searches. Overall, from the current data, the topdwon splaytree acheived the greatest performance gap when compared to its bottom up partner. However, the margin is still quite close.**

Finally, let us consider if these more advanced allocators are worth all the trouble in the first place. The implementations are challenge and it is true that their utilization may suffer due to the extra size of the nodes. However, as the runtime information suggests we are guaranteed tight bounds on our time complexity. The `list_segregated` allocator can become less consistent in terms of speed when the number of free nodes grows greatly (however I believe I can address the performance gap). Any tree based allocator does not suffer this variability. Perhaps a decision to use either of these designs could boil down to profiling your needs.

If you know you are requesting a large number of uniquely sized blocks of memory from the heap, then freeing them over a long unknown period of time a tree allocator would be helpful. It is resistant to the growth of any workload and will at worst cost some memory utilization. For large projects with long loosesly defined lifetimes this may be an appealing option.

The list based allocators are best for smaller scale programs or projects in which you have a good idea of the lifetime and scope of all memory you will need. With a medium to large number of heap requests and a need for memory efficiency you cannot go wrong with the `list_segregated` allocator. And of course the `libc` allocator does exactly what it advertises: perform well across all scenarios while not purporting to be superior or the perfect allocator. It is a great choice! Also, do not forget that the `libc` allocator is thread safe which allows programmers to use it how they wish across many different applications. Some of these tree based allocators would  have a terrible time with multithreading, especially the splay trees. I am not even sure what is possible in this regard because I cannot think of good scheme for concurrent acess and fixups.

For further exploration, I would be interested in finding real world programs with more diverse heap calls, such as `realloc()`. The current real world tests were mostly calls to `malloc()` and `free()`, keeping our tree small. I will also try to add to the trace program section of this writeup in the future.

> **Back to the ([`README.md`](/README.md))**.

