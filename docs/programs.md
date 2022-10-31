# The Programs

## Navigation

1. Home
   - Documentation **([`README.md`](/README.md))**
2. The CLRS Standard
   - Documentation **([`rbtree_clrs.md`](/docs/rbtree_clrs.md))**
   - Implementation **([`rbtree_clrs.c`](/lib/rbtree_clrs.c))**
3. Unified Symmetry
   - Documentation **([`rbtree_unified.md`](/docs/rbtree_unified.md))**
   - Implementation **([`rbtree_unified.c`](/lib/rbtree_unified.c))**
4. Doubly Linked Duplicates
   - Documentation **([`rbtree_linked.md`](/docs/rbtree_linked.md))**
   - Implementation **([`rbtree_linked.c`](/lib/rbtree_linked.c))**
5. Stack Based
   - Documentation **([`rbtree_stack.md`](/docs/rbtree_stack.md))**
   - Implementation **([`rbtree_stack.c`](/lib/rbtree_stack.c))**
6. Top-down Fixups
   - Documentation **([`rbtree_topdown.md`](/docs/rbtree_topdown.md))**
   - Implementation **([`rbtree_topdown.c`](/lib/rbtree_topdown.c))**
7. List Allocators
   - Documentation **([`list_segregated.md`](/docs/list_segregated.md))**
   - Implementation **([`list_addressorder.c`](/lib/list_addressorder.c))**
   - Implementation **([`list_bestfit.c`](/lib/list_bestfit.c))**
   - Implementation **([`list_segregated.c`](/lib/list_segregated.c))**
8. Runtime Analysis
   - Documentation **([`rbtree_analysis.md`](/docs/rbtree_analysis.md))**
9. The Programs
   - Documentation **([`programs.md`](/docs/programs.md))**

## Program Navigation

- [The Test Harness](#test-harness)
  - **[`test_harness.c`](/src/test_harness.c)**
- [The Time Harness](#time-harness)
  - **[`time_harness.c`](/src/time_harness.c)**
- [Print Peaks](#print-peaks)
  - **[`print_peaks.c`](/src/print_peaks.c)**
- [Python Script Generation](#python-script-generation)
  - **[`parsing.py`](/pysrc/parsing.py)**

## Compiling Programs

In order to compile programs in this repository, follow these steps.

1. Install Dependencies
   - This project uses CMake to build and run. So far, I have tested this on multiple Linux distributions and the directions that follow should work with your compiler. I have tested this on Mac as well, using CMake, and it seems that CMake is able to manage Clang building the project on Mac. For windows, I do not have a way to build the project at this time. I would recommend using `WSL2` with Ubuntu on Windows to build the project.
   - Install Gnuplot. This comes with some Linux distributions. If it is not installed on Linux use your package manager to install it. However, all programs will compile and run without it installed, you will just miss out on nice graphs in the terminal if it is not installed.
   - On MacOS, I would recommend installing Gnuplot with Homebrew like this.
     - `brew install gnuplot`
   - Use a terminal that supports ANSI escape sequences and rgb colors. I am considering adding portability with a library like `ncurses.h` but that might require another dependency installation and seems overkill.
2. Make the Programs
   - If you would like to explore how the programs and different versions of the allocators build together, explore the `CMakeLists.txt` files in the root, `lib/` and `src/` directories. The basics are that we make a different version of every program for each allocator.
   - From the root directory run the following commands.
       - `cmake -S . -B build/`
           - This tells CMake to treat the root directory as the project source directory and generate the necessary build files and directories to the `build/` directory. I ignore the `build/` directory in my `.gitignore` so this command will create the folder if it is not present.
        - `cmake --build build`
            - This tells CMake to build the project. If all is well, everything will build and link together. This may be where you see errors if I have been unable to verify the project on your distribution or platform.
   - This compiles all programs to the `/build/src/` folder from which you will run each allocator's version of the program. Directions on the commands to run will follow in each program's description below.
3. Run the Programs
   - I will specify commands for each programs, but they all involve some variation on the following formula.
     - `./build/src/[PROGRAM]_[ALLOCATOR NAME] [PROGRAM ARGUMENTS] scripts/[.SCRIPT FILE TO RUN]`
   - We specify the program, the allocator version, any arguments valid for that program, and then the script file we want to run for our allocator to execute.
   - All directions assume you are running programs from the root directory of the repository but this is not required. However, be careful of current directory if you decide to run commands that follow in this document, especially script generation commands.
4. Generate Additional Scripts
   - Read the [Python Script Generation](#python-script-generation) section for details on how to trace your favorite Linux programs with `ltrace` or generate new `.script` files.
   - I tested my allocators on scripts ranging from a few thousand lines to millions of lines. However including all of these scripts in the repository would make it far too large. This repository ignores any files in the `scripts/` folder that begin with the word `time`.
   - Generate large `time*.script` files and test them out as you wish. See the [Python Script Generation](#python-script-generation) section for details on how to get all the large extra files you are missing that I tested allocators on.

## Test Harness

This program, built by Stanford CS professors and course staff, inspired all subsequent programs in this repository. It is a wrapper program for the heap allocators that runs various safety and correctness checks on the calls to our heap allocators to ensure correctness. It leverages our `validate_heap()` function that we wrote to rigorously check and debug our allocators.

After I submitted my work for this original list based allocator assignment and came back to the work over the summer to expand and complete all subsequent Red Black Tree heap allocators, this program helped me complete my work. There is no fancy output for this program. It is meant to be used while in the GDB debugger to fix errors in the allocators. Most of my time was spent in GDB for this project.

### How to Use the Test Harness

We can run this program on one or multiple `.script` files to ensure they run without error and check their utilization.

- All allocators have been prepended with the word `test_` and have been compiled to our `build/src/` folder.
  - `test_rbtree_clrs`, `test_rbtree_unified`, `test_rbtree_linked`, `test_rbtree_stack`, `test_rbtree_topdown`, `test_list_bestfit`, `test_list_addressorder`
- Run an allocator on a single test script.
  - `./build/src/test_rbtree_clrs scripts/pattern-mixed.script`
- Run an allocator on multiple scripts with the additional wildcard symbol if there are multiple files with the same name format.
  - `./build/src/test_rbtree_clrs scripts/trace-emacs.script scripts/trace-gcc.script scripts/trace-chs.script`
  - Is the same as...
  - `./build/src/test_rbtree_clrs scripts/trace-*.script`
  - And then add as many scripts as you would like...
  - `./build/src/test_rbtree_clrs scripts/trace-*.script scripts/example*.script scripts/pattern-*.script`
- Run an allocator in quiet mode which will not validate the heap after every call, speeding up our ability to check utilization.
  - `./build/src/test_rbtree_clrs -q scripts/trace-*.script scripts/example*.script scripts/pattern-*.script`
- Output will include how many successful calls to the heap completed and the utilization averaged across all run scripts.

## Time Harness

I wrote a `time_harness.c` program based upon the `test_harness.c` program the Stanford course staff wrote for us. The original `test_harness.c` program was focused on correctness. Along with our own debugging and printing functions, the goal of the test harness was to be as thorough as possible in finding bugs quickly and ensuring our allocators functioned properly. This meant that the programs produced for each allocator were painfully slow. There were numerous O(N) operations built into the provided program and I added many more O(N) checks into my own implementations. For a red-black tree these checks only grow as the necessity to ensure the rules of a Red Black Tree are followed becomes critical.

For the `time_harness.c` implementation, we abandon all safety and correctness checks. We should only use this program once we know beyond a doubt that the implementations are correct. We achieve faster testing by leaving all "client" available memory uninitialized, performing no checks on the boundaries of the blocks we give to the user, and cutting out any unnecessary O(N) operations from the program while it calls upon the functions in our allocators. We do not want O(N) work clouding the runtime efficiency of what would otherwise be O(lgN) operations, for example. This results in the ability to check the performance of requests ranging anywhere from 5,000 to 1,000,000 and beyond quickly.

While it would have been nice to time our code with a simple `time` command and call it a day, we need a custom program like this is because timing specific functions of an allocator can be tricky for two main reasons: allocations and coalescing. In order to create a tree of N nodes of free memory for the user, you may think we could just create N allocated blocks of memory and then free them all, thus inserting them into the tree. This would not work. Because we coalesce blocks to our left and right by address in memory, when we went to free the next allocation, it would absorb its left neighbor and simply grow in size with each `free()` call and be inserted back into the tree. We would end up with our original large pool of free memory by the end of all `free()` requests.

Instead, we will allocate 2N blocks of memory and then call `free()` on every other block of allocated memory to measure the performance of N insertions into a tree. Coalescing will not occur on the two adjacent blocks because they remain allocated.

The time it takes to make all of these allocations is also not of interest to us if we want to measure insertion and removal from our tree, so we need to be able to time our code only when the insertions and removals begin. To do this, we need to rely on the `time.h` C library and start a `clock()` on the exact range of requests we are interested in. We can achieve this by looking at our scripts and asking the `time-harness.c` program to time only specific line numbers representing requests.

```bash
 ./build/src/time_list_bestfit -s 200000 -e 300000 -s 300001 scripts/time-insertdelete-100k.script
```

- The above command is for timing one-hundred thousand insertions, `free()`, and one-hundred thousand removals, `malloc()`, from our tree.
- Notice that we use the `-s` flag to start timing on a line number and the `-e` flag to end our timer.
- We also must start our timer after two-hundred thousand `malloc()` requests to set up a scenario where we can insert one-hundred thousand nodes into the tree.
- This program can time multiple sections of a script as long as they do not overlap and will output the timing result of all requested sections.
- We will get a helpful graphs that highlight key statistics about the heap as well.

We would then get output such as the following.

![list-time-showcase](/images/list-time-showcase.png)

*Pictured Above: Output from the time harness with exucution time of line ranges, utilization, free node count, and timer per request graphs. Try running the program on the tree allocators to see the staggering speed difference.*

### How to Use the Time Harness

- All allocators have been prepended with the word `time_` and have been compiled to our `build/src/` folder.
  - `time_rbtree_clrs`, `time_rbtree_unified`, `time_rbtree_linked`, `time_rbtree_stack`, `time_rbtree_topdown`, `time_list_bestfit`, `time_list_addressorder`
- Time the entire execution time of the allocator over an entire script.
  - `./build/src/time_rbtree_clrs scripts/time-insertdelete-100k.script`
- Time only sections of the script according to line number ranges.
  - `./build/src/time_rbtree_clrs -s 200000 -e 300000 -s 300001 scripts/time-insertdelete-100k.script`

## Print Peaks

Implementing a Red Black Tree in the context of a heap allocator was challenging for me. In order to make debugging easier I developed heap and tree printing functions to dump the state of the heap and tree. This helped spot how the tree was changing or when nodes were being dropped or forgotten during complex rotations, insertions, or deletions. Seeing a good printed visual representation of a red black tree can also help make it more understandable why they are so fast. Seeing a black height of 4 in a tree full of thousands of nodes makes it clear how it acheives the O(lgN) searchtime.

This program acts as a mini GDB debugger, allowing you to see the state of the free node data structure at its peak. It will also allow you to set breakpoints on line numbers in the script and will show you the state of the free node data structure after that line has executed. At the end of execution, it will also show a graph of the number of free nodes over the lifetime of the heap and how the utilization may have changed during heap lifetime as well. For example, by default here is the output for a run on a smaller script.

![rb-print-peaks](/images/rb-print-peaks.png)

Here is the same command for a list based allocator. The list output is far less interesting, but perhaps makes it clear, from its simplicity, why the tree is the faster data structure. Please note that for smaller workloads the time measurements for any allocator will be very low and gnuplot may have trouble displaying them on a graph. For more stable graphs use larger data sets.

![list-print-peaks](/images/list-print-peaks.png)

### How to Use Print Peaks

- All allocators have been prepended with the word `print_peaks_` and have been compiled to our `build/src/` folder.
  - `print_peaks_rbtree_clrs`, `print_peaks_rbtree_unified`, `print_peaks_rbtree_linked`, `print_peaks_rbtree_stack`, `print_peaks_rbtree_topdown`, `print_peaks_list_bestfit`, `print_peaks_list_addressorder`
- Run the default options to see what line of the script created the peak number of free nodes. Look at my printing debugger function for that allocator to see how the nodes are organized.
  - `./build/src/print_peaks_list_bestfit scripts/pattern-mixed.script`
- Run the default options in verbose mode with the `-v` flag. This flag can be included in any future options as well. This displays the free data structure with memory addresses included and black heights for the tree allocators. This is the printer I used because I needed to see memory addresses to better understand where errors were occurring. Verbose should always be the first argument if it will be included.
  - `./build/src/print_peaks_list_bestfit -v scripts/pattern-mixed.script`
- Add breakpoints corresponding to line numbers in the script. This will show you how many free nodes existed after that line executes. You will also enter an interactive terminal session. You can decide if you want to continue to the next breakpoint, add a new future breakpoint, or continue to the end of the program execution. Be sure to follow the prompt directions.
  - `./build/src/print_peaks_list_bestfit -v -b 100 -b 200 -b 450 scripts/pattern-mixed.script`

## Python Script Generation

There are two important steps to measuring the performance of these heap allocators: tracing the heap usage of real world programs to emulate on my allocators, and generating artificial scripts for performance testing. The first step is the more straightforward.

### Ltrace Output

The program parsing.py is capable of taking output from the Linux `ltrace` command and cleaning it up for a `.script` file. These `.script` files are then used to conveniently test a custom heap allocator. Here are the directions for the ltrace command. Thanks to the CS107 professor Nick Troccoli for helping me with understanding the `ltrace` command.

Add the memory calls that we want to watch for and pick an output file. Then execute the desired command on the desired file as follows.

```bash
ltrace -e malloc+realloc+calloc -e free-@libc.so* --output [FILENAME] [COMMAND]
```

With an editor like emacs that would look like this.

```bash
ltrace -e malloc+realloc+calloc -e free-@libc.so* --output output.txt emacs myfile.txt
```

We will get output like this.

```output
gcc->malloc(48)                                  = 0x18102a0
gcc->free(0x1836e50)                             = <void>
gcc->realloc(0x1836f10, 176)                     = 0x1836f10
--- SIGCHLD (Child exited) ---
--- SIGCHLD (Child exited) ---
--- SIGCHLD (Child exited) ---
+++ exited (status 0) +++
```

This Python program then transforms this output into this.

```output
a 0 48
a 1 8
r 1 176
f 1
f 0
```

### Ltrace Parsing

Once you have generated your ltrace output to the desired `.txt` file, feed it to this python program as follows.

```zsh
python3 parsing.py -parse [LTRACE_OUTPUT_FILE] [DESIRED_SCRIPT_FILE]
```

For example a command may look like this.

```zsh
python3 parsing.py -parse ltrace-output/ltrace-output.txt ../scripts/time-ltrace-parsed.script
```

### Script Generation

Generating custom scripts for a heap allocator is more complex and allows for more options. By default, we can allocate, free, and reallocate as much memory as we want, and our program will free all remaining memory for the script file at the end so the allocators do not leak memory due to an oversite in the script.

### Allocations

To allocate blocks of memory in a script file use the following commands or options as needed. These commands are run as if you are currently int the `pysrc/` directory, where the python program is located.

Create a desired number of allocation requests. The program will pick random byte sizes for the requests.

```zsh
python3 parsing.py -generate ../scripts/10k-allocations.script -alloc 10000
```

Choose one byte size for all allocations. On Windows and Linux I must wrap the allocation request in extra quotation marks. I am not sure the requirements on all systems for this command style.

```zsh
python3 parsing.py -generate ../scripts/10k-allocations.script '-alloc(20)' 10000
```

Choose a byte size range and every call will be a random size within that range for the allocations. Do not use spaces within the parenthesis for the allocation request.

```zsh
python3 parsing.py -generate ../scripts/10k-allocations.script '-alloc(20,500)' 10000
```

### Reallocations

To reallocate blocks of memory in a script file use the following commands or options as needed.

Create a desired number of reallocation requests. The program will pick random byte sizes for the requests.

```zsh
python3 parsing.py -generate ../scripts/5k-reallocations.script -alloc 10000 -realloc 5000
```

Choose one byte size for all reallocations. On Windows and Linux I must wrap the allocation request in extra quotation marks. I am not sure the requirements on all systems for this command style.

```zsh
python3 parsing.py -generate ../scripts/5k-reallocations.script '-alloc(20)' 10000 '-realloc(500)' 5000
```

Choose a byte size range and every call will be a random size within that range for the reallocations. Do not use spaces within the parenthesis for the reallocation request.

```zsh
python3 parsing.py -generate ../scripts/5k-reallocations.script '-alloc(20,500)' 10000 '-realloc(800,1200)' 5000
```

### Free

To free blocks of memory in a script file use the following commands or options as needed. As a special note, freeing memory defaults to freeing every other block of allocated memory. This is done because my allocator coalesced the blocks to its left and right on every call to `realloc()` or `free()`. This made it hard to build large trees of free memory because I was just reforming one large pool of free memory as I made calls to free. To better test performance with more free nodes, freeing every other block will ensure that the desired number of nodes enters the tree. Be sure to have double the amount of allocated memory compared to your number of free requests. If you want a tree of N free nodes, make 2N allocations first.

Free all memory that has been allocated up to this point. This will free every other block and then go back and free the rest.

```zsh
python3 parsing.py -generate ../scripts/10k-frees.script -alloc 10000 -realloc 5000 -free
```

Free a specific number of blocks of memory. This is good for building a tree of free nodes and then making requests to `malloc()` to take nodes from that tree.

```zsh
python3 parsing.py -generate ../scripts/5k-insertdelete.script -alloc 10000 -free 5000 -alloc 5000
```

### Build Your Own

These commands can be combined to create scripts that allow us to time specific operations on our heap. For example, here is the command I used to test the coalescing speed of different implementations of my heap allocator. I wanted to see how quickly we could reallocate 100,000 blocks of allocated memory, which would involve coalescing many nodes in the tree.

```zsh
python3 parsing.py -generate ../scripts/time-100kinsertdelete.script '-alloc(1,500)' 200000 -free 100000 '-alloc(1,500)' 100000
```

Here are two commands you can run if you would like to generate all the missing files I used to time my allocators. They are ignored by default because they are too large, so you must generate them yourself if you want to test the allocators on larger files.

> **Be warned, the naive `list_` based allocators will not be able to finish the larger files (more than about 50-80k alloc or realloc requests) in a reasonable amount of time. However, the `list_segregated` allocator is quite capable of handling large scripts.**

Generate a decent range of files that test allocators' abilities to quicky `malloc()` and `free()` from and to free node structures of increasing size.

Run this from the `pysrc/` directory.

```bash
(python3 parsing.py -generate ../scripts/time-insertdelete-10k.script '-alloc(1,500)' 20000 -free 10000 '-alloc(1,500)' 10000) && (python3 parsing.py -generate ../scripts/time-insertdelete-20k.script '-alloc(1,500)' 40000 -free 20000 '-alloc(1,500)' 20000) && (python3 parsing.py -generate ../scripts/time-insertdelete-30k.script '-alloc(1,500)' 60000 -free 30000 '-alloc(1,500)' 30000) && (python3 parsing.py -generate ../scripts/time-insertdelete-40k.script '-alloc(1,500)' 80000 -free 40000 '-alloc(1,500)' 40000) && (python3 parsing.py -generate ../scripts/time-insertdelete-50k.script '-alloc(1,500)' 100000 -free 50000 '-alloc(1,500)' 50000) && (python3 parsing.py -generate ../scripts/time-insertdelete-60k.script '-alloc(1,500)' 120000 -free 60000 '-alloc(1,500)' 60000) && (python3 parsing.py -generate ../scripts/time-insertdelete-70k.script '-alloc(1,500)' 140000 -free 70000 '-alloc(1,500)' 70000) && (python3 parsing.py -generate ../scripts/time-insertdelete-80k.script '-alloc(1,500)' 160000 -free 80000 '-alloc(1,500)' 80000) && (python3 parsing.py -generate ../scripts/time-insertdelete-90k.script '-alloc(1,500)' 180000 -free 90000 '-alloc(1,500)' 90000) && (python3 parsing.py -generate ../scripts/time-insertdelete-100k.script '-alloc(1,500)' 200000 -free 100000 '-alloc(1,500)' 100000)
```

Generate files to test an allocator's ability to quickly `realloc()` allocated nodes in the heap. This a good test of coalescing speed.

Run this from the `pysrc/` directory.

```bash
(python3 parsing.py -generate ../scripts/time-reallocfree-05k.script '-alloc(1,500)' 10000 -free 5000 '-realloc(200,1000)' 5000) && (python3 parsing.py -generate ../scripts/time-reallocfree-10k.script '-alloc(1,500)' 20000 -free 10000 '-realloc(200,1000)' 10000) && (python3 parsing.py -generate ../scripts/time-reallocfree-15k.script '-alloc(1,500)' 30000 -free 15000 '-realloc(200,1000)' 15000) && (python3 parsing.py -generate ../scripts/time-reallocfree-20k.script '-alloc(1,500)' 40000 -free 20000 '-realloc(200,1000)' 20000) && (python3 parsing.py -generate ../scripts/time-reallocfree-25k.script '-alloc(1,500)' 50000 -free 25000 '-realloc(200,1000)' 25000) && (python3 parsing.py -generate ../scripts/time-reallocfree-30k.script '-alloc(1,500)' 60000 -free 30000 '-realloc(200,1000)' 30000) && (python3 parsing.py -generate ../scripts/time-reallocfree-35k.script '-alloc(1,500)' 70000 -free 35000 '-realloc(200,1000)' 35000) && (python3 parsing.py -generate ../scripts/time-reallocfree-40k.script '-alloc(1,500)' 80000 -free 40000 '-realloc(200,1000)' 40000) && (python3 parsing.py -generate ../scripts/time-reallocfree-45k.script '-alloc(1,500)' 90000 -free 45000 '-realloc(200,1000)' 45000) && (python3 parsing.py -generate ../scripts/time-reallocfree-50k.script '-alloc(1,500)' 100000 -free 50000 '-realloc(200,1000)' 50000) && (python3 parsing.py -generate ../scripts/time-reallocfree-55k.script '-alloc(1,500)' 110000 -free 55000 '-realloc(200,1000)' 55000) && (python3 parsing.py -generate ../scripts/time-reallocfree-60k.script '-alloc(1,500)' 120000 -free 60000 '-realloc(200,1000)' 60000) && (python3 parsing.py -generate ../scripts/time-reallocfree-65k.script '-alloc(1,500)' 130000 -free 65000 '-realloc(200,1000)' 65000) && (python3 parsing.py -generate ../scripts/time-reallocfree-70k.script '-alloc(1,500)' 140000 -free 70000 '-realloc(200,1000)' 70000) && (python3 parsing.py -generate ../scripts/time-reallocfree-75k.script '-alloc(1,500)' 150000 -free 75000 '-realloc(200,1000)' 75000) && (python3 parsing.py -generate ../scripts/time-reallocfree-80k.script '-alloc(1,500)' 160000 -free 80000 '-realloc(200,1000)' 80000) && (python3 parsing.py -generate ../scripts/time-reallocfree-85k.script '-alloc(1,500)' 170000 -free 85000 '-realloc(200,1000)' 85000) && (python3 parsing.py -generate ../scripts/time-reallocfree-90k.script '-alloc(1,500)' 180000 -free 90000 '-realloc(200,1000)' 90000) && (python3 parsing.py -generate ../scripts/time-reallocfree-95k.script '-alloc(1,500)' 190000 -free 95000 '-realloc(200,1000)' 95000) && (python3 parsing.py -generate ../scripts/time-reallocfree-100k.script '-alloc(1,500)' 200000 -free 100000 '-realloc(200,1000)' 100000)
```

## My Optional Program

This was a program we created to test our heap allocators in a more "real" context. Ideally we would take a previous program that utilized the heap and exchange all standard library heap calls and replace them with our own, along with a few other steps such as initializing our heap segment. I left the program I tested with, a simple text parsing program, but you could replace this with anything you would like. Just use my heap allocator as the backing for it.

### How to Use My Optional Program

- All allocators have been prepended with the word `my_optional_program_` and have been compiled to our `build/src/` folder.
  - `my_optional_program_rbtree_clrs`, `my_optional_program_rbtree_unified`, `my_optional_program_rbtree_linked`, `my_optional_program_rbtree_stack`, `my_optional_program_rbtree_topdown`, `my_optional_program_list_bestfit`, `my_optional_program_list_addressorder`
- The rest is up to you. Design whatever program you would like.

