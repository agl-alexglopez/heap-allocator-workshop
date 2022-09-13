# Runtime Analysis

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

Now that we have gone over the basics for each heap allocator, we can see how they perform in relation to one another. Let's start by gaining some perspective in terms of how much faster a Red Black Tree implementation is compared to a simple doubly linked list. While we have not discussed the doubly linked list implementation in detail, it is a simple implementation that involves maintaining a sorted doubly linked list by block size and taking the best fit block for any given request. It is a common introduction to allocators, and if you wish for more details there are numerous articles and college assignments that will start with this implementation. A doubly linked list has a worst case of $\Theta(N)$ for any single request from `free()` to insert a block into the list and $\Theta(N)$ for any request to `malloc()` to free the block from the list.

![list-v-tree](/images/list-v-tree.png)

*Pictured Above: A chart representing the time to complete (N) insert delete requests. The first implementation is a simple doubly linked list that maintains sorted order by free block size to acheive best fit. The second red line that is hardly visible at the bottom is a Red Black Tree that maintains sorted order to acheive best fit.*

Here are the key details from the above graph.

- The time complexity of inserting and deleting N elements into and out of the list is $\Theta(N^2)$.
  - There may also be some intermittent extra work every time we delete a node from the list because we split of any extra part of a block that is too big and re-insert it into the list.
- As the number of free blocks grows in size the list becomes increasingly impractical taking over a minute to cycle through all requests at the peak of requests.
- The tree implementation is hardly measureable in comparison to the list. Not only do we never exceed one second to service any number of requests on this chart, but also the time scale of seconds is not informative.

So, for further analysis of our tree implementations, we need to reduce our time scale by a factor of 1,000 and compare implementations in milliseconds. This will reveal interesting differences between the five Red Black Tree allocators.

## Testing Methodology

The following test methods and analysis are not targeted towards any specific applications. If these allocators were to be implemented in any serious application more testing would need to occur. These tests, instead, are targeted towards the general performance of the Red Black Tree algorithms and help address some areas of interest I hypothesized about while trying the different strategies outlined in each allocator. Mainly, I am most curious about the following.

- Is there any cost or benefit to uniting the left and right cases of Red Black trees into one case with the use of an array of pointers?
- Does the approach of storing duplicate node sizes in a linked list benefit performance by trimming the tree?
- When we abandon the parent field of the tree node in exchange for the linked list field, do we sacrifice performance?
- Does a topdown approach to fixing a Red Black Tree reduce the number of $\Theta(lgN)$ operations enough to make a difference in performance?

In order to answer these questions I wrote a `time_harness.c` program based upon the `test_harness.c` program the Stanford course staff wrote for us. The original `test_harness.c` program was focussed on correctness. Along with our own debugging and printing functions, the goal of the test harness was to be as thorough as possible in finding bugs quickly and ensuring our allocators functioned properly. This meant that the programs produced for each allocator were painfully slow. There were numerous $\Theta(N)$ operations built into the provided program and I added many more $\Theta(N)$ checks into my own implementations. For a red-black tree these checks only grow as the necessity to ensure the rules of a Red Black Tree are followed becomes critical.

For the `time_harness.c` implementation, we abandon all safety and correctness checks. We should only use this program once we know beyond a doubt that the implementations are correct. We acheive faster testing by leaving all "client" available memory uninitialized, performing no checks on the boundaries of the blocks we give to the user, and cutting out any unecessary $\Theta(N)$ operations from the program while it calls upon the functions in our allocators. We do not want $\Theta(N)$ work clouding the runtime efficiency of what would otherwise be $\Theta(lgN)$ operations, for example. This results in the ability to check the performance of requests ranging anywhere from 5,000 to 1,000,000 and beyond quickly.

While it would have been nice to time our code with a simple `time` command and call it a day, we need a custom program like this is because timing specific functions of an allocator can be tricky for two main reasons: allocations and coalescing. In order to create a tree of $N$ nodes of free memory for the user, you may think we could just create $N$ allocated blocks of memory and then free them all, thus inserting them into the tree. This would not work. Because we coalesce blocks to our left and right by address in memory, when we went to free the next allocation, it would absorb its left neighbor and simply grow in size with each `free()` call and be inserted back into the tree. We would end up with our original large pool of free memory by the end of all `free()` requests.

Instead, we will allocate $2N$ blocks of memory and then call `free()` on every other block of allocated memory to measure the performance of $N$ insertions into a tree. Coalescing will not occur on the two adjacent blocks because they remain allocated.

The time it takes to make all of these allocations is also not of interest to us if we want to measure insertion and removal from our tree, so we need to be able to time our code only when the insertions and removals begin. To do this, we need to rely on the `time.h` C library and start a `clock()` on the exact range of requests we are interested in. We can acheive this by looking at our scripts and asking the `time-harness.c` program to time only specific line numbers representing requests.

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

## Inserting and Deleting

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

## Coalescing

Coalescing left and right in all implementations occurs whenever we `free()` or `realloc()` a block of memory. One of my initial reasons for implementing doubly linked duplicate nodes was to see if we could gain speed when we begin coalescing on a large tree. I hypothesized that heap allocators have many duplicate sized nodes and that if we reduce the time to coalesce duplicates to $\Theta(1)$ whenever possible, we might see some speedup in an otherwise slow operation.

In order to test the speed of coalescing across implementations we can set up a similar heap to the one we used for our insertion and deletion test. If we want to test the speed of $N$ coalescing operations, we will create $2N$ allocations. Then, we will free $N$ blocks of memory to insert into the tree such that every other allocation is freed to prevent premature coalescing. Finally, we will call `realloc()` on every allocation that we have remaining, $N$ blocks. My implementation of `realloc()` and `free()` coalesce left and right every time, whether the reallocation request is for more or less space. I have set up a random distribution of calls for more or less space, with a slight skew towards asking for more space. This means we are garunteed to left and right coalesce for every call to `realloc()` giving us the maximum possible number of encounters with nodes that are in the tree. We will find a number of different coalescing scenarios this way as mentioned in the allocator summaries.

If we are using a normal Red Black Tree with a parent field, we treat all coalescing the same. We free our node from the tree and then use the parent field to perform the necessary fixups. At worst this will cost $\Theta(lgN)$ operations to fix the colors of the tree. For the `rbtree_linked` implementation, where we kept the parent field and added the linked list as well, any scenario in which we coalesce a duplicate node will result in an $\Theta(1)$ operation. No rotations, fixups, or searches are necessary. This is great! For the `rbtree_stack` and `rbtree_topdown` implementations, we were able to acheive the same time complexity by thinking creatively about how we view nodes and how they store the parent field if they are a duplicate. Both of those allocators represent advanced optimizations in speed and space efficiency. The only disadvantage to the `rbtree_stack` allocator, when compared to the `rbtree_linked` allocator, is that if we coalesce a unique node we must launch into a tree search to build its path in a stack in preparation for fixups. The `rbtree_linked` implementation is able to immediately start fixups because it still has the parent field, even in a unique node.

![chart-realloc-free](/images/chart-realloc-free.png)

Here are the key details from the above graph.

- The runtime of $N$ reallocations is $\Theta(NlgN)$.
- The `rbtree_linked` implementation with nodes that have a `*parent`, `links[LEFT-RIGHT]`, and `*list_start` field is the clear winner. The number of $\Theta(1)$ encounters with duplicates reduces the overall runtime of the allocator significantly.
- The stack approach is again a solid balance of space-efficiency and speed. However, it seems that accessing an array to get the nodes you need is a factor slowing this implementation down compared to `rbtree_linked`. The search to build the stack when we coalesce a unique node also shows up as a time cost without the `parent` field. There is also added complexity in how `rbtree_stack` and `rbtree_topdown` store the parent field, increasing instruction counts.
- The topdown approach must fix the tree while it goes down to remove a node, thus costing time due to extra work.
- The `rbtree_unified` implementation only differs from the `rbtree_clrs` implementation in that unifies the left and right cases of a Red Black tree using an array in one of the node fields. Yet, it is faster in this application.

These results make the most wasteful implementation in terms of space, `rbtree_linked`, more of a proof of concept. If the similar results can be acheived with more space efficiency, the implementation loses value.

I also have lingering questions about why the `rbtree_unified` implementation is slower than the traditional `rbtree_clrs` implementation in insertions and deltions, but faster here. However, an undeniable conclusion from both sets of tests so far is that it is worth managing duplicate nodes in a linked list. The fact that we are required to do so when we abandon the parent field is a fruitful challenge in terms of speed. To get an idea of just how many duplicates the allocators may encounter in these artificial workloads, here is a picture of the tree configuration for 50,000 insert delete requests at the peak of the tree size.

![50k-insert-delete](/images/rb-tree-50k-insertdelete.png)

*Pictured Above: A Red Black tree after 50,000 insertions of nodes into the tree. Notice how many duplicates we have and imagine these duplicates spread across an entire tree if we did not manage them in a list.*

## Tracing Programs

While the initial artificial tests were fun for me to lab and consider in terms of how to best measure and execute parts of scripts, we are leaving out a major testing component. Real world testing is key! I will reiterate, I have no specific application in mind for any of these allocators. This makes choosing the programs to test somewhat arbitrary. I am choosing programs or Linux commands that are interesting to me and I will seek some variety wherever possible. If this were a more serious piece of research, this would be a key moment to begin forming the cost and benefit across a wide range of applications.

To trace programs we use the `ltrace` command (thank you to my professor of CS107 at Stanford, Nick Troccoli for giving me the tutorial on how to trace memory usage in Linux). This command has many options and one set of instructions will allow you to trace `malloc`, `calloc`, `realloc`, and `free` calls like this.

```bash
ltrace -e malloc+realloc+calloc -e free-@libc.so* --output [FILENAME] [COMMAND]
```

If we then write a simple parsing program we can turn the somewhat verbose output of the ltrace command into a cleaned up and more simple `.script` file. If you would like to see how that parsing is done, read my implementation in Python.

> **Read my code here ([`parsing.py`](/pysrc/parsing.py)).**

### Linux Tree Command

The Linux `tree` command will print out all directory contents in a tree structure from the current directory as root. This is similar to my tree printing function you have seen me use to illustrate Red Black trees throughout this write up. There are many options for this command but I am choosing to start in the `home/` directory of Ubuntu on WSL2 and use the `-f` and `-a` flags which will output full paths and hidden files for all directories. Again, for perspective here is the full comparison with the list allocator.

![chart-tracetree](/images/chart-tracetree.png)

It is again unproductive to discuss a speed comparison between the Red Black Tree allocators on this scale, so we will drop the list allocator for further comparison.

![chart-rbtracetree](/images/chart-rbtracetree.png)

Here are the key details from the above graph.

- Requests for the `tree` command consist mostly of `malloc()` and `free()` and the free tree remains small over the course of the program's lifetime.
- The `rbtree_unified` implementation is surprisingly slow in this scenario.
- Again, no compromises in the `rbtree_linked` implementation make it fast, even if it is not space efficient.

### Neovim

Neovim is my editor of choice for this project. It is a text editor based off of Vim and we can actually trace its heap usage while we use it to edit a document. We will trace the heap usage while we edit the README.md for the project. This program usage is similar to the Linux Tree command with many `malloc()` and `free()` calls. However, among the roughly one million requests to the heap for a short editing session, there are also twenty-eight thousand calls to realloc, so we have more diverse heap usage.

![chart-nvim](/images/chart-tracenvim.png)

Here are the key observations from the above graph.

- The traditional CLRS implementation of a Red Black tree will perform well in any application where there are many calls `malloc()` and `free()` and the tree remains small.
- The extra complexity of managing duplicates speeds up the tree slightly when the tree remains small.

### Utilization

While it was not productive to compare speeds of the list allocator to the tree allocator, the list allocator can show value in its high utilization due to low overhead to maintain the list. Let's compare utilization across the insertion/deletion, coalescing, and tracing applications.

![chart-utilization](/images/chart-utilization.png)

Here are the key details from the above graph.

- While slow, the doubly linked list allocator has excellent utilization.
- The implementations of the `rbtree_clrs`, `rbtree_unified`, `rbtree_stack`, and `rbtree_topdown` are different in terms of the details of their code and algorithms. However, they have the exact same space used in their nodes, therefore the utilization is identical.
- Perhaps this view makes the cost of the speed gained by the `rbtree_linked` approach more apparent.

## Conclusions

Let's revisit the questions I was most interested in when starting the runtime analysis of these allocators.

- Is there any cost or benefit to uniting the left and right cases of Red Black trees into one case with the use of an array of pointers?
  - **Our initial runtime informations suggests that accessing pointers to other nodes in the tree from an array is slightly slower. We can explore the assembly of the traditional node versus the array based node to see for sure where the instruction counts expand. This is a possible extension to pursue in this project.**
- Does the approach of storing duplicate node sizes in a linked list benefit performance by trimming the tree?
  - **Absolutely. In fact, if you wish to make no compromises in terms of performance then simply add the linked list pointer to the traditional node. However, if you want a more space efficient approach use an explicit stack to track lineage of the tree, and store duplicates in a linked list pointer.**
- When we abandon the parent field of the tree node in exchange for the linked list field, do we sacrifice performance?
  - **If we use the CLRS algorithm to fix the tree with a bottom up approach, we do not lose much speed. However when we fix the tree with a topdown approach, we will lose some time even though we have eliminated the need for a parent field. This seems to be because we do extra work whenever we fix the tree on the way down, and our bottom up approach may not perform the worst case number of fixups on every execution.**
- Does a topdown approach to fixing a Red Black Tree reduce the number of $\Theta(lgN)$ operations enough to make a difference in performance?
  - **Yes. The difference is that the topdown algorithm is slower than the bottom up algorithm. Perhaps the added logic of handling duplicates and needing to rewire pointers slows down this implementation of an allocator initially intended for no duplicates and transplanting removed nodes by copying data from the replacement node to the one being removed. We may also perform more fixups while going down the tree that involve expensive operations such as rotations than we would in a bottom up approach. In these real world tests, the bottom up algorithm must not perform the worst case number of fixups as frequently as the topdown algorithm.**

For further exploration, I would be interested in finding real world programs with more diverse heap calls, such as `realloc()`. The current real world tests were mostly calls to `malloc()` and `free()`, keeping our tree small.

> **Back to the ([`README.md`](/README.md))**.

