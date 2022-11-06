# Runtime Analysis

## Navigation

1. Home
   - Documentation **([`README.md`](/README.md))**
2. The CLRS Standard
   - Documentation **([`rbtree_clrs.md`](/docs/rbtree_clrs.md))**
   - Design **([`rbtree_clrs_utilities.h`](/lib/rbtree_clrs_utilities.h))**
   - Implementation **([`rbtree_clrs_algorithm.c`](/lib/rbtree_clrs_algorithm.c))**
3. Unified Symmetry
   - Documentation **([`rbtree_unified.md`](/docs/rbtree_unified.md))**
   - Design **([`rbtree_unified_utilities.h`](/lib/rbtree_unified_utilities.h))**
   - Implementation **([`rbtree_unified_algorithm.c`](/lib/rbtree_unified_algorithm.c))**
4. Doubly Linked Duplicates
   - Documentation **([`rbtree_linked.md`](/docs/rbtree_linked.md))**
   - Design **([`rbtree_linked_utilities.h`](/lib/rbtree_linked_utilities.h))**
   - Implementation **([`rbtree_linked_algorithm.c`](/lib/rbtree_linked_algorithm.c))**
5. Stack Based
   - Documentation **([`rbtree_stack.md`](/docs/rbtree_stack.md))**
   - Design **([`rbtree_stack_utilities.h`](/lib/rbtree_stack_utilities.h))**
   - Implementation **([`rbtree_stack_algorithm.c`](/lib/rbtree_stack_algorithm.c))**
6. Top-down Fixups
   - Documentation **([`rbtree_topdown.md`](/docs/rbtree_topdown.md))**
   - Design **([`rbtree_topdown_utilities.h`](/lib/rbtree_topdown_utilities.h))**
   - Implementation **([`rbtree_topdown_algorithm.c`](/lib/rbtree_topdown_algorithm.c))**
7. List Allocators
   - Documentation **([`list_segregated.md`](/docs/list_segregated.md))**
   - Design **([`list_addressorder_utilities.h`](/lib/list_addressorder_utilities.h))**
   - Implementation **([`list_addressorder_algorithm.c`](/lib/list_addressorder_algorithm.c))**
   - Design **([`list_bestfit_utilities.h`](/lib/list_bestfit_utilities.h))**
   - Implementation **([`list_bestfit_algorithm.c`](/lib/list_bestfit_algorithm.c))**
   - Design **([`list_segregated_utilities.h`](/lib/list_segregated_utilities.h))**
   - Implementation **([`list_segregated_algorithm.c`](/lib/list_segregated_algorithm.c))**
8. Runtime Analysis
   - Documentation **([`rbtree_analysis.md`](/docs/rbtree_analysis.md))**
9. The Programs
   - Documentation **([`programs.md`](/docs/programs.md))**

## Overview

Now that we have gone over the basics for each heap allocator, we can see how they perform in relation to one another. Let's start by gaining some perspective in terms of how much faster a Red Black Tree implementation is compared to a simple doubly linked list. While we have not discussed the doubly linked list implementation in detail, it is a simple implementation that involves maintaining a sorted doubly linked list by block size and taking the best fit block for any given request. It is a common introduction to allocators, and if you wish for more details there are numerous articles and college assignments that will start with this implementation. A doubly linked list has a worst case of O(N) for any single request from `free()` to insert a block into the list and O(N) for any request to `malloc()` to free the block from the list.

![list-v-tree](/images/list-v-tree.png)

*Pictured Above: A chart representing the time to complete (N) insert delete requests. The first implementation is a simple doubly linked list that maintains sorted order by free block size to achieve best fit. The second red line that is hardly visible at the bottom is a Red Black Tree that maintains sorted order to achieve best fit.*

Here are the key details from the above graph.

- The time complexity of inserting and deleting N elements into and out of the list is O(N^2).
  - There may also be some intermittent extra work every time we delete a node from the list because we split of any extra part of a block that is too big and re-insert it into the list.
- As the number of free blocks grows in size the list becomes increasingly impractical taking over a minute to cycle through all requests at the peak of requests.
- The tree implementation is hardly measurable in comparison to the list. Not only do we never exceed one second to service any number of requests on this chart, but also the time scale of seconds is not informative.

For a further explanation of why we see such a drastic performance difference we can look at the time another way. I added monitoring to the `time_harness.c` program that will use Gnuplot to output information about heap allocator performance. We can observe the number of free nodes the allocator had to manage and also the time it takes to complete a single request. Observe the relationship between request speed and free nodes in the list based allocator.

![list-time-per-request](/images/list-time-per-request.png)

*Pictured Above: Two graphs. The top represents the number of free nodes over a script's lifetime and the bottom represents the time to complete a single request over the script's lifetime.*

Here are the key details from the above graphs.

- We can clearly observe that as the number of free nodes grows our time to complete a request grows linearly with it.
- This is the most direct representation I can show of the O(N) time it takes for a list based allocator to operate on a single request.
- Keep note of the y axis when we compare this to our tree allocator and how the timescale changes.

Now let's see how the same script operates with a tree based allocator. We will compare it to our naïve implementation of the standard CLRS implementation without any additional optimizations that later allocators include.

![rb-time-per-request](/images/rb-time-per-request.png)

*Pictured Above: Two graphs. The top represents the number of free nodes over a script's lifetime and the bottom represents the time to complete a single request over the script's lifetime.*

Here are the key details of the above graphs.

- The time to complete a request is relatively unphased by the growth of the red black tree of free nodes. In such a crude graph, small changes in time are hard to observe.
- This is what an O(lgN) runtime gets us in terms of how consistently fast it can operate on the tree.
- We have dropped to a maximum observable request time of just under 0.3 milliseconds compared to 20 milliseconds in a list allocator.

What's even more impressive is what happens when we step up the number of free nodes in the tree to 1 million nodes. The measurements remain consistently low. Remember this is also the naive implementation of a Red Black Tree from CLRS. Later optimizations in allocator implementations produce even tighter bounds on the worst case performance showcasing an impressively low time per request, consistently.

![rb-time-per-request-1m](/images/rb-time-per-request-1m.png)

*Pictured Above: Two graphs. The top represents the number of free nodes over a script's lifetime and the bottom represents the time to complete a single request over the script's lifetime.*

### A Better List

So far, it may seem that a list based allocator cannot compete with the Red-Black tree data structure. However, that is not the case. The simple list based allocator mentioned above is not the best representative for the list data structure. Many allocators, including the GNU Allocator to some extent, use something called a segregated fits allocator. This involves keeping an array of size ranges in which to store doubly linked lists of that size. We can get clever and mix storing singly sized lists with larger lists that are perfect base 2 power of 2 sizes. While a seemingly simple idea, it can yeild a major speed boost over the simple list. We can even obtain great utilization in some cases.

For example, here is the time per request for the segregated list allocator over the same range of requests we discussed earlier.

![seg-time-per-request](/images/seg-time-per-request.png)

*Pictured Above: Two graphs. The top represents the number of free nodes over a script's lifetime and the bottom represents the time to complete a single request over the script's lifetime.*

You may notice that this allocator resembles the O(lgN) runtime we like to see in our tree based allocators. However, it is slightly less resistant to growth in our number of free nodes. You will see this lack of resistance to the growth of the free lists as we continue our analysis. We will keep this allocator as a point of comparison in our different sections because it is competitive in terms of both space and speed.

For further analysis of our tree implementations, we will reduce our time scale by a factor of 1,000 and compare implementations in milliseconds. This will reveal interesting differences between the five Red Black Tree allocators and the Segregated Fits allocator that will serve as our baseline for performance. We will no longer include the simple list allocator in further comparisons as that is not productive.

## Testing Methodology

The following test methods and analysis are not targeted towards any specific applications. If these allocators were to be implemented in any serious application more testing would need to occur. These tests, instead, are targeted towards the general performance of the Red Black Tree algorithms and help address some areas of interest I hypothesized about while trying the different strategies outlined in each allocator. Mainly, I am most curious about the following.

- Is there any cost or benefit to uniting the left and right cases of Red Black trees into one case with the use of an array of pointers?
- Does the approach of storing duplicate node sizes in a linked list benefit performance by trimming the tree?
- When we abandon the parent field of the tree node in exchange for the linked list field, do we sacrifice performance?
- Does a top-down approach to fixing a Red Black Tree reduce the number of O(lgN) operations enough to make a difference in performance?

In order to answer these questions I wrote a `time_harness.c` program based upon the `test_harness.c` program the Stanford course staff wrote for us. The original `test_harness.c` program was focussed on correctness. Along with our own debugging and printing functions, the goal of the test harness was to be as thorough as possible in finding bugs quickly and ensuring our allocators functioned properly. This meant that the programs produced for each allocator were painfully slow. There were numerous O(N) operations built into the provided program and I added many more O(N) checks into my own implementations. For a red-black tree these checks only grow as the necessity to ensure the rules of a Red Black Tree are followed becomes critical.

For the `time_harness.c` implementation, we abandon all safety and correctness checks. We should only use this program once we know beyond a doubt that the implementations are correct. We acheive faster testing by leaving all "client" available memory uninitialized, performing no checks on the boundaries of the blocks we give to the user, and cutting out any unecessary O(N) operations from the program while it calls upon the functions in our allocators. We do not want O(N) work clouding the runtime efficiency of what would otherwise be O(lgN) operations, for example. This results in the ability to check the performance of requests ranging anywhere from 5,000 to 1,000,000 and beyond quickly.

While it would have been nice to time our code with a simple `time` command and call it a day, we need a custom program like this is because timing specific functions of an allocator can be tricky for two main reasons: allocations and coalescing. In order to create a tree of N nodes of free memory for the user, you may think we could just create N allocated blocks of memory and then free them all, thus inserting them into the tree. This would not work. Because we coalesce blocks to our left and right by address in memory, when we went to free the next allocation, it would absorb its left neighbor and simply grow in size with each `free()` call and be inserted back into the tree. We would end up with our original large pool of free memory by the end of all `free()` requests.

Instead, we will allocate 2N blocks of memory and then call `free()` on every other block of allocated memory to measure the performance of N insertions into a tree. Coalescing will not occur on the two adjacent blocks because they remain allocated.

The time it takes to make all of these allocations is also not of interest to us if we want to measure insertion and removal from our tree, so we need to be able to time our code only when the insertions and removals begin. To do this, we need to rely on the `time.h` C library and start a `clock()` on the exact range of requests we are interested in. We can acheive this by looking at our scripts and asking the `time-harness.c` program to time only specific line numbers representing requests.

```bash
 ./build/bin/time_rbtree_clrs -s 200000 -e 300000 -s 300001 scripts/time-insertdelete-100k.script
```

- The above command is for timing 100 thousand insertions, `free()`, and 100 thousand removals, `malloc()`, from our tree.
- Notice that we use the `-s` flag to start timing on a line number and the `-e` flag to end our timer.
- We also must start our timer after ten thousand `malloc()` requests to set up a scenario where we can insert 100 thousand nodes into the tree.
- This program can time multiple sections of a script as long as they do not overlap and will output the timing result of all requested sections.
- We will get a helpful graphs that highlight key statistics about the heap as well.

We would then get output such as the following.

![time-harness-output](/images/time-harness-output.png)

For more details about how the timing functions work or how the program parses arguments please see the code.

> **Read my code here ([`time_harness.c`](/src/time_harness.c)).**

## Inserting and Deleting

To set up a measurement of inserting and deleting into a Red-Black Tree we start by forming a script with 2N allocations and then measure the time to insert N elements, `free()`, and delete N elements, `malloc()` from our tree. Remember, we have now dropped down to the milliseconds level, compared to the seconds level we used when analyzing a doubly linked list allocator. Here are the results across allocators.

![insert-delete-full](/images/chart-insert-delete-full.png)

*Pictured Above: The six allocators compared on a runtime graph for insert delete requests. The time is now measured in milliseconds, while the number of requests remains the same as our previous comparison.*

As you can see, the `list_segregated` allocator has strong performance, suffering from more erratic runtime speed as the size of the lists in the lookup table grows. Let's zoom in on the `rbtree` allocators.

![insert-delete](/images/chart-insert-delete.png)

*Pictured Above: The six allocators compared on a runtime graph for insert delete requests with a focus on the tree allocators.*

Here are the key details from the above graph.

- Lower time in milliseconds is better.
- The time complexity of inserting and deleting N elements into our tree is O(NlgN).
  - We may also see some extra time cost from breaking off extra space from a block that is too large and reinserting it into the tree. However, this is hard to predictably measure and does not seem to have an effect on the Big-O time complexity.
- We see an improvement of ~10% in runtime speed when we manage duplicates with a doubly linked list to trim the tree.
- The top-down approach to managing a Red-Black tree is not the fastest in terms of inserting and deleting from the tree, but it is extremely competitive.
- These allocators clearly outperform the standard segregated fits allocator. However, it is tricky to pin down the runtime time complexity of the segregated list allocator exactly. It approaches an O(N^2) time complexity.

Recall from the allocator summaries that we are required to manage duplicates in a Red-Black tree if we decide to abandon the parent field of the traditional node. The two approaches that trade the parent field for the pointer to a linked list of duplicates perform admirably: `rbtree_top-down` and `rbtree_stack`. The top-down approach is still an improvement over a traditional Red-Black tree, but surprisingly, the stack implementation that relies on the logical structure of the CLRS Red-Black tree is the winner in this competition. While I thought the `rbtree_linked` approach that uses a field for the `*parent`, `links[LEFT]`, `links[RIGHT]`, and `*list_start` would be the worst in terms of space efficiency, I thought we would gain some speed overall. We still need to measure coalescing performance, but this makes the `rbtree_linked` implementation seem the most pointless at this point if the same speed can be achieved with more space efficiency.

Finally, the performance gap scales as a percentage, meaning we will remain about %10 faster when we manage duplicates to trim the tree even when we are dealing with millions of elements and our times have increased greatly. Managing duplicates in a linked list with only the use of one pointer is a worthwhile optimization. Revisit any summary of the last three allocators if you want more details on how this optimization works.

On a per request basis, we can observe the speedup in an allocator like `rbtree_linked`. Recall the earlier graph I showed of the CLRS implementation completing 1 million insertions and deletions from the tree. Here is the graph illustrating the speedup we gain with these optimizations. The times often fall too low to be consistently measurable and there is far less jumping up in time to complete a request.

![rb-time-per-request-1m-linked](/images/rb-time-per-request-1m-linked.png)

*Pictured Above: Two graphs. The top represents the number of free nodes over a script's lifetime and the bottom represents the time to complete a single request over the script's lifetime.*

## Coalescing

Coalescing left and right in all implementations occurs whenever we `free()` or `realloc()` a block of memory. One of my initial reasons for implementing doubly linked duplicate nodes was to see if we could gain speed when we begin coalescing on a large tree. I hypothesized that heap allocators have many duplicate sized nodes and that if we reduce the time to coalesce duplicates to O(1) whenever possible, we might see some speedup in an otherwise slow operation.

In order to test the speed of coalescing across implementations we can set up a similar heap to the one we used for our insertion and deletion test. If we want to test the speed of N coalescing operations, we will create 2N allocations. Then, we will free N blocks of memory to insert into the tree such that every other allocation is freed to prevent premature coalescing. Finally, we will call `realloc()` on every allocation that we have remaining, N blocks. My implementation of `realloc()` and `free()` coalesce left and right every time, whether the reallocation request is for more or less space. I have set up a random distribution of calls for more or less space, with a slight skew towards asking for more space. This means we are garunteed to left and right coalesce for every call to `realloc()` giving us the maximum possible number of encounters with nodes that are in the tree. We will find a number of different coalescing scenarios this way as mentioned in the allocator summaries.

If we are using a normal Red Black Tree with a parent field, we treat all coalescing the same. We free our node from the tree and then use the parent field to perform the necessary fixups. At worst this will cost O(lgN) operations to fix the colors of the tree. For the `rbtree_linked` implementation, where we kept the parent field and added the linked list as well, any scenario in which we coalesce a duplicate node will result in an O(1) operation. No rotations, fixups, or searches are necessary. This is great! For the `rbtree_stack` and `rbtree_top-down` implementations, we were able to acheive the same time complexity by thinking creatively about how we view nodes and how they store the parent field if they are a duplicate. Both of those allocators represent advanced optimizations in speed and space efficiency. The only disadvantage to the `rbtree_stack` allocator, when compared to the `rbtree_linked` allocator, is that if we coalesce a unique node we must launch into a tree search to build its path in a stack in preparation for fixups. The `rbtree_linked` implementation is able to immediately start fixups because it still has the parent field, even in a unique node.

![chart-realloc-free-full](/images/chart-realloc-free-full.png)

*Pictured Above: The six allocators compared on a runtime graph for realloc requests.*

Again, we see a competitive `list_segregated` allocator that approaches an O(N^2) runtime complexity. However, let's zoom in.

![chart-realloc-free](/images/chart-realloc-free.png)


*Pictured Above: The six allocators compared on a runtime graph for realloc requests.*

Here are the key details from the above graph.

- The runtime of N reallocations is O(NlgN).
- The `rbtree_linked` implementation with nodes that have a `*parent`, `links[LEFT-RIGHT]`, and `*list_start` field is the clear winner. The number of O(1) encounters with duplicates reduces the overall runtime of the allocator significantly.
- The stack approach is again a solid balance of space-efficiency and speed. However, it seems that accessing an array to get the nodes you need is a factor slowing this implementation down compared to `rbtree_linked`. The search to build the stack when we coalesce a unique node also shows up as a time cost without the `parent` field. There is also added complexity in how `rbtree_stack` and `rbtree_top-down` store the parent field, increasing instruction counts.
- The top-down approach must fix the tree while it goes down to remove a node, thus costing time due to extra work, but it is no worse than a standard implementation in this context.
- The `rbtree_unified` implementation only differs from the `rbtree_clrs` implementation in that unifies the left and right cases of a Red Black tree using an array in one of the node fields. Yet, it is slower in this application.
- The `list_segregated` allocator is more competitive in this category, but is soon outclassed by the speed of the Red-Black tree allocators. This makes sense because freeing nodes from a doubly linked list to coalesce them will always be an O(1) operation. I hypothesize that the slowdown comes from the time it takes whenever we must search the lookup table and lists to find the first fist. All other operations are O(1) and should not be bottlenecks for this allocator.

These results make the most wasteful implementation in terms of space, `rbtree_linked`, more of a proof of concept. If the similar results can be achieved with more space efficiency, the implementation loses value.

I also have lingering questions about why the `rbtree_unified` implementation is slower than the traditional `rbtree_clrs` implementation in insertions and deletions. However, an undeniable conclusion from both sets of tests so far is that it is worth managing duplicate nodes in a linked list. The fact that we are required to do so when we abandon the parent field is a fruitful challenge in terms of speed. To get an idea of just how many duplicates the allocators may encounter in these artificial workloads, here is a picture of the tree configuration for 50,000 insert delete requests at the peak of the tree size.

![50k-insert-delete](/images/rb-tree-50k-insertdelete.png)

*Pictured Above: A Red Black tree after 50,000 insertions of nodes into the tree. Notice how many duplicates we have and imagine these duplicates spread across an entire tree if we did not manage them in a list.*

## Tracing Programs

While the initial artificial tests were fun for me to lab and consider in terms of how to best measure and execute parts of scripts, we are leaving out a major testing component. Real world testing is key! I will reiterate, I have no specific application in mind for any of these allocators. This makes choosing the programs to test somewhat arbitrary. I am choosing programs or Linux commands that are interesting to me and I will seek some variety wherever possible. If this were a more serious piece of research, this would be a key moment to begin forming the cost and benefit across a wide range of applications.

To trace programs we use the `ltrace` command (thank you to my professor of CS107 at Stanford, Nick Troccoli for giving me the tutorial on how to trace memory usage in Linux). This command has many options and one set of instructions will allow you to trace `malloc`, `calloc`, `realloc`, and `free` calls like this.

```bash
ltrace -e malloc+realloc+calloc -e free-@libc.so* --output [FILENAME] [COMMAND]
```

If we then write a simple parsing program we can turn the somewhat verbose output of the `ltrace` command into a cleaned up and more simple `.script` file. If you would like to see how that parsing is done, read my implementation in Python.

> **Read my code here ([`parsing.py`](/pysrc/parsing.py)).**

### Linux Tree Command

The Linux `tree` command will print out all directory contents in a tree structure from the current directory as root. This is similar to my tree printing function you have seen me use to illustrate Red Black trees throughout this write up. There are many options for this command but I am choosing to start in the `home/` directory of Ubuntu on WSL2 and use the `-f` and `-a` flags which will output full paths and hidden files for all directories. Again, for perspective here is the full comparison with the list allocators.

![chart-tracetree](/images/chart-tracetree.png)

While it may seem that the naive `list_bestfit` allocator is outclassed, this section will reveal interesting real-world test results that make the list allocators quite competitive. You may have already noticed that the `list_segregated` allocator is in the lead.

![chart-rbtracetree](/images/chart-rbtracetree.png)

Here are the key details from the above graph.

- Requests for the `tree` command consist mostly of `malloc()` and `free()` and the free tree remains small over the course of the program's lifetime.
- The `rbtree_unified` implementation is surprisingly slow in this scenario.
- Again, no compromises in the `rbtree_linked` implementation make it fast, even if it is not space efficient.
- The peak number of free nodes that the `list_segregated` had to manage was 2,381. This is a relatively small number to manage and therefore the more simple implementation is able to shine with its lower instruction counts.

### Neovim

Neovim is my editor of choice for this project. It is a text editor based off of Vim and we can actually trace its heap usage while we use it to edit a document. We will trace the heap usage while we edit the README.md for the project. This program usage is similar to the Linux Tree command with many `malloc()` and `free()` calls. However, among the roughly one million requests to the heap for a short editing session, there were 35,216 calls to realloc, so we have more diverse heap usage.

![chart-nvim](/images/chart-rbtracenvim.png)

Here are the key observations from the above graph.

- The traditional CLRS implementation of a Red Black tree will perform well in any application where there are many calls `malloc()` and `free()` and the tree remains small.
- The extra complexity of managing duplicates speeds up the tree slightly when the tree remains small.
- The number of free nodes in this test was small. There were only roughly 708, so the list allocators, even `list_bestfit`, can do well here.

### Utilization

The list allocators can again show value in their high utilization due to low overhead to maintain the list. We can compare utilization across the insertion/deletion, coalescing, and tracing applications.

![chart-utilization](/images/chart-utilization.png)

Here are the key details from the above graph.

- While slow, the doubly linked list allocator has excellent utilization.
- The implementations of the `rbtree_clrs`, `rbtree_unified`, `rbtree_stack`, and `rbtree_top-down` are different in terms of the details of their code and algorithms. However, they have the exact same space used in their nodes, therefore the utilization is identical.
- Perhaps this view makes the cost of the speed gained by the `rbtree_linked` approach more apparent.
- The more advanced Segregated Fits allocator can achieve an excellent balance of speed and utilization. This might be why it is a common suggestion for a general purpose allocator.

## Conclusions

Let's revisit the questions I was most interested in when starting the runtime analysis of these allocators.

- Is there any cost or benefit to uniting the left and right cases of Red Black trees into one case with the use of an array of pointers?
  - **Our initial runtime information suggests that accessing pointers to other nodes in the tree from an array is slightly slower. We can explore the assembly of the traditional node versus the array based node to see for sure where the instruction counts expand. This is a possible extension to pursue in this project.**
- Does the approach of storing duplicate node sizes in a linked list benefit performance by trimming the tree?
  - **Absolutely. In fact, if you wish to make no compromises in terms of performance then simply add the linked list pointer to the traditional node. However, if you want a more space efficient approach use an explicit stack to track lineage of the tree, and store duplicates in a linked list pointer.**
- When we abandon the parent field of the tree node in exchange for the linked list field, do we sacrifice performance?
  - **If we use the CLRS algorithm to fix the tree with a bottom up approach, we do not lose much speed. However when we fix the tree with a top-down approach, we will lose some time even though we have eliminated the need for a parent field. This seems to be because we do extra work whenever we fix the tree on the way down, and our bottom up approach may not perform the worst case number of fix-ups on every execution.**
- Does a top-down approach to fixing a Red Black Tree reduce the number of O(lgN) operations enough to make a difference in performance?
  - **Yes. The difference is that the top-down algorithm is slower than the bottom up algorithm. Perhaps the added logic of handling duplicates and needing to rewire pointers slows down this implementation of an allocator initially intended for no duplicates and transplanting removed nodes by copying data from the replacement node to the one being removed. We may also perform more fix-ups while going down the tree that involve expensive operations such as rotations than we would in a bottom up approach. In these real world tests, the bottom up algorithm must not perform the worst case number of fix-ups as frequently as the top-down algorithm.**

Finally, let us consider if the Red Black Tree allocators are worth all the trouble in the first place. The implementation was a challenge and it is true that their utilization may suffer due to the extra size of the nodes. However, as the runtime information suggests we are guaranteed tight bounds on our time complexity. The `list_segregated` allocator can become less consistent in terms of speed when the number of free nodes grows greatly. Any tree based allocator does not suffer this variability. Perhaps a decision to use either of these designs could boil down to profiling your needs.

If you knew you would requesting a large number of uniquely sized blocks of memory from the heap, then freeing them over a long period of time a tree allocator would be helpful. It is resistant to the growth of any workload and will at worst cost some memory utilization. For large projects with long lifetimes this may be an appealing option.

The list based allocators are best for smaller scale programs or projects in which you have a good idea of the lifetime and scope of all memory you will need. With a medium to large number of heap requests and a need for memory efficiency you cannot go wrong with the `list_segregated` allocator.

For further exploration, I would be interested in finding real world programs with more diverse heap calls, such as `realloc()`. The current real world tests were mostly calls to `malloc()` and `free()`, keeping our tree small.

> **Back to the ([`README.md`](/README.md))**.

