# The Programs

## Compiling Programs

In order to compile programs in this repository, follow these steps.

1. Install Dependencies
   - I am trying to make this project more compatible with Clang, but for now, you must compile the project using GCC. Install GCC on MacOS and place it before clang in your path. For Linux, compiling with GCC should not cause any errors. For windows, I do not have a way to build this project. I would recommend using `WSL2` with Ubuntu on Windows if you have not tried it yet to build and run this project.
   - Install Gnuplot. This comes with some Linux distributions. If it is not installed on Linux use your package manager to install it. However, all programs will compile and run without it installed, you will just miss out on nice graphs in the terminal if it is not installed.
   - On MacOS, I would recommend installing Gnuplot with Homebrew like this.
     - `brew install gnuplot`
   - Use a terminal that supports ANSI escape sequences and rgb colors. I am considering adding portability with a library like `ncurses.h` but that might require another dependency installation and seems overkill.
2. Make the Programs
   - If you would like to explore how the programs and different versions of the allocators build together, explore the Makefile. The basics are that we make a different version of every program for each allocator.
   - Enter the `src/` directory and run `make`.
   - This compiles all programs to the `obj/` folder from which you will run each allocator's version of the program. Directions on the commands to run will follow in each program's description below.
3. Run the Programs
   - I will specify commands for each programs, but they all involve some variation on the following formula.
     - `.././obj/[PROGRAM]_[ALLOCATOR NAME] [PROGRAM ARGUMENTS] ../scripts/[.SCRIPT FILE TO RUN]`
   - We specify the program, the allocator version, any arguments valid for that program, and then the script file we want to run for our allocator to execute.
   - All directions assume you are running programs from the `src/` folder but this is not required.

## Test Harness

This program, built by Stanford CS professors and course staff, inspired all subsequent programs in this repository. It is a wrapper program for the heap allocators that runs various safety and correctness checks on the calls to our heap allocators to ensure correctness. It leverages our `validate_heap()` function that we wrote to rigorously check and debug our allocators.

After I submitted my work for this original list based allocator assignment and came back to the work over the summer to expand and complete all subsequent Red Black Tree heap allocators, this program helped me complete my work. There is no fancy output for this program. It is meant to be used while in the GDB debugger to fix errors in the allocators. Most of my time was spent in GDB for this project.

### How to Use the Test Harness

We can run this program on one or multiple `.script` files to ensure they run without error and check their utilization.

- All allocators have been prepended with the word `test_` and have been compiled to our `obj/` folder.
  - `test_rbtree_clrs`, `test_rbtree_unified`, `test_rbtree_linked`, `test_rbtree_stack`, `test_rbtree_topdown`, `test_list_bestfit`, `test_list_addressorder`
- Run an allocator on a single test script.
  - `.././obj/test_rbtree_clrs ../scripts/pattern-mixed.script`
- Run an allocator on multiple scripts with the additional wildcard symbol if there are multiple files with the same name format.
  - `.././obj/test_rbtree_clrs ../scripts/trace-emacs.script ../scripts/trace-gcc.script ../scripts/trace-chs.script`
  - Is the same as...
  - `.././obj/test_rbtree_clrs ../scripts/trace-*.script`
  - And then add as many scripts as you would like...
  - `.././obj/test_rbtree_clrs ../scripts/trace-*.script ../scripts/example*.script ../scripts/pattern-*.script`
- Run an allocator in quiet mode which will not validate the heap after every call, speeding up our ability to check utilization.
  - `.././obj/test_rbtree_clrs -q ../scripts/trace-*.script ../scripts/example*.script ../scripts/pattern-*.script`
- Output will include how many successful calls to the heap completed and the utilization averaged across all run scripts.

## Time Harness

I wrote a `time_harness.c` program based upon the `test_harness.c` program the Stanford course staff wrote for us. The original `test_harness.c` program was focussed on correctness. Along with our own debugging and printing functions, the goal of the test harness was to be as thorough as possible in finding bugs quickly and ensuring our allocators functioned properly. This meant that the programs produced for each allocator were painfully slow. There were numerous O(N) operations built into the provided program and I added many more O(N) checks into my own implementations. For a red-black tree these checks only grow as the necessity to ensure the rules of a Red Black Tree are followed becomes critical.

For the `time_harness.c` implementation, we abandon all safety and correctness checks. We should only use this program once we know beyond a doubt that the implementations are correct. We acheive faster testing by leaving all "client" available memory uninitialized, performing no checks on the boundaries of the blocks we give to the user, and cutting out any unecessary O(N) operations from the program while it calls upon the functions in our allocators. We do not want O(N) work clouding the runtime efficiency of what would otherwise be O(lgN) operations, for example. This results in the ability to check the performance of requests ranging anywhere from 5,000 to 1,000,000 and beyond quickly.

While it would have been nice to time our code with a simple `time` command and call it a day, we need a custom program like this is because timing specific functions of an allocator can be tricky for two main reasons: allocations and coalescing. In order to create a tree of N nodes of free memory for the user, you may think we could just create N allocated blocks of memory and then free them all, thus inserting them into the tree. This would not work. Because we coalesce blocks to our left and right by address in memory, when we went to free the next allocation, it would absorb its left neighbor and simply grow in size with each `free()` call and be inserted back into the tree. We would end up with our original large pool of free memory by the end of all `free()` requests.

Instead, we will allocate 2N blocks of memory and then call `free()` on every other block of allocated memory to measure the performance of N insertions into a tree. Coalescing will not occur on the two adjacent blocks because they remain allocated.

The time it takes to make all of these allocations is also not of interest to us if we want to measure insertion and removal from our tree, so we need to be able to time our code only when the insertions and removals begin. To do this, we need to rely on the `time.h` C library and start a `clock()` on the exact range of requests we are interested in. We can acheive this by looking at our scripts and asking the `time-harness.c` program to time only specific line numbers representing requests.

```bash
 .././obj/time_rbtree_clrs -s 200000 -e 300000 -s 300001 ../scripts/time-insertdelete-100k.script
```

- The above command is for timing one-hundred thousand insertions, `free()`, and one-hundred thousand removals, `malloc()`, from our tree.
- Notice that we use the `-s` flag to start timing on a line number and the `-e` flag to end our timer.
- We also must start our timer after two-hundred thousand `malloc()` requests to set up a scenario where we can insert one-hundred thousand nodes into the tree.
- This program can time multiple sections of a script as long as they do not overlap and will output the timing result of all requested sections.
- We will get a helpful graphs that highlight key statistics about the heap as well.

We would then get output such as the following.

![time-harness-output](/images/time-harness-output.png)

### How to Use the Time Harness

- All allocators have been prepended with the word `time_` and have been compiled to our `obj/` folder.
  - `time_rbtree_clrs`, `time_rbtree_unified`, `time_rbtree_linked`, `time_rbtree_stack`, `time_rbtree_topdown`, `time_list_bestfit`, `time_list_addressorder`
- Time the entire execution time of the allocator over an entire script.
  - `.././obj/time_rbtree_clrs ../scripts/time-insertdelete-100k.script`
- Time only sections of the script according to line number ranges.
  - `.././obj/time_rbtree_clrs -s 200000 -e 300000 -s 300001 ../scripts/time-insertdelete-100k.script`

## Print Peaks

Implementing a Red Black Tree in the context of a heap allocator was challenging for me. In order to make debugging easier I developed heap and tree printing functions to dump the state of the heap and tree. This helped spot how the tree was changing or when nodes were being dropped or forgotten during complex rotations, insertions, or deletions. Seeing a good printed visual representation of a red black tree can also help make it more understandable why they are so fast. Seeing a black height of 4 in a tree full of thousands of nodes makes it clear how it acheives the O(lgN) searchtime.

This program acts as a mini GDB debugger, allowing you to see the state of the free node data structure at its peak. It will also allow you to set breakpoints on line numbers in the script and will show you the state of the free node data structure after that line has executed. At the end of execution, it will also show a graph of the number of free nodes over the lifetime of the heap and how the utilization may have changed during heap lifetime as well. For example, by default here is the output for a run on a smaller script.

![rb-print-peaks](/images/rb-print-peaks.png)

Here is the same command for a list based allocator. The list output is far less interesting, but perhaps makes it clear, from its simplicity, why the tree is the faster data structure.

![list-print-peaks](/images/list-print-peaks.png)

### How to Use Print Peaks

- All allocators have been prepended with the word `print_peaks_` and have been compiled to our `obj/` folder.
  - `print_peaks_rbtree_clrs`, `print_peaks_rbtree_unified`, `print_peaks_rbtree_linked`, `print_peaks_rbtree_stack`, `print_peaks_rbtree_topdown`, `print_peaks_list_bestfit`, `print_peaks_list_addressorder`
- Run the default options to see what line of the script created the peak number of free nodes. Look at my printing debugger function for that allocator to see how the nodes are organized.
  - `.././obj/print_peaks_list_bestfit ../scripts/pattern-mixed.script`
- Run the default options in verbose mode with the `-v` flag. This flag can be included in any future options as well. This displays the free data structure with memory addresses included and black heights for the tree allocators. This is the printer I used because I needed to see memory addresses to better understand where errors were occuring. Verbose should always be the first argument if it will be included.
  - `.././obj/print_peaks_list_bestfit -v ../scripts/pattern-mixed.script`
- Add breakpoints corresponding to line numbers in the script. This will show you how many free nodes existed after that line executes. You will also enter an interactive terminal session. You can decide if you want to continue to the next breakpoint, add a new future breakpoint, or continue to the end of the program execution. Be sure to follow the prompt directions.
  - `.././obj/print_peaks_list_bestfit -v -b 100 -b 200 -b 450 ../scripts/pattern-mixed.script`

## My Optional Program

This was a program we created to test our heap allocators in a more "real" context. Ideally we would take a previous program that utilized the heap and exchange all standard library heap calls and replace them with our own, along with a few other steps such as initializing our heap segment. I left the program I tested with, a simple text parsing program, but you could replace this with anything you would like. Just use my heap allocator as the backing for it.

### How to Use My Optional Program

This is up to you. Design whatever program you would like.

## Python Script Generation

