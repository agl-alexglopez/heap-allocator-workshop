# Heap Allocator Workshop

![workshop-collage](/images/workshop-collage.png)

 *Pictured Above: A sampling of the programs, tools, and analysis I used to complete this project. Explore the repository to see how I implemented the allocators, how I measured their performance, and how I interacted with them through the programs in this repository.*

> **Notice:** Heap allocators are common university CS assignments. **NO heap allocator implementation that I submitted for any course at any institution is included here**. All implementations are original and written in my own free time, with sources cited when appropriate. The data structures and algorithms exceed the requirements and complexity of any introductory classes of which I am aware, limiting their helpfulness in implementing one's own first allocator. This is simply an exploration of the tradeoffs in heap allocator implementations, a broad and interesting topic. I hope you enjoy! 

## Quick Start

The purpose of this repository is to put in place the tools needed to quickly develop and understand heap allocators. I find it enjoyable to learn about arguably the most convenient piece of software for day-to-day programming first hand, by implementing it yourself! There are also many interesting data structures and algorithms we can apply to managing the heap. And perhaps a need will arise for a custom allocator and this will serve as a great testing ground for new ideas. Here is what you can do with this repository.

## Explore

There are a number of heap allocators already implemented in this repository. To learn about them you can do the following.

1. **[Read the wiki](https://github.com/agl-alexglopez/heap-allocator-workshop/wiki)**. I have an accompanying writeup for every allocator implemented in this repository as well as more conceptual writeups for more abstract CS concepts like Red Black Trees, Splay Trees, and Segregated Lists.
2. Plot performance. Run the `plot.cc` program. It gathers all available allocators and compares their performance across, malloc, realloc, free, or any specific script you have in mind. We can even see how we stack up against `libc`! **Before trying to plot the allocators be sure to generate any extra scripts that are missing.** By default I don't include all the script files because some are very large. Follow directions in the script generation section below to generate the missing scripts.
3. Print internal data structures. Every allocator is responsible for implementing its own debugging functions, both logical and visual. The `peaks.cc` program provides a peak into these internal representations in a controlled manner. This is a fun program to run for the tree based allocators.
4. Read the tests. If you have ideas for your own allocator, read how they are tested. The Google Test framework provides a basic set of expectations for the core `malloc`, `realloc`, and `free` functions with additional behaviors enforced like coalescing. The `ctest.cc` program is the correctness check that is essential for debugging implementations.

## Create

If you want to implement your own idea for a heap allocator or just want to practice writing some C, go ahead. All that is required is one `.c` file with your implementation and as long as you **[follow the specification requirements and build instructions](/lib/readme.md)** your allocator will slot in with the rest and become available for testing and performance measurements by default.

If you would rather flush out performance characteristics of the allocators present, try generating some allocator scripts. There are instructions and a python program available to either create scripts from scratch or emulate real world programs.

## Programs

### Build

To build the project I use cmake with a convenience `Makefile` if you wish to use it. I have provided a `CMakePresets.json` and `CMakeUserPresets.json` file with configurations prepared for both `clang` and `gcc`. I can verify that the project builds with `cmake 3.28.1`, `clang 17.0.5`, and `gcc 12.3.0`. I left the `CMakeUserPresets.json` file in version control as an example of how one might change build generators and compiler settings to their liking. For example, my system has two versions of GCC available so I specify `gcc-12` and `g++-12` as the compilers I want for this project; I also specify that Unix Makefiles is the generator that CMake should use when compiling with GCC. However, I have Ninja set as the build generator when Clang is in use and I do not need to specify which Clang version I want because I only have one installed on my system. If you run into issues compiling the project the first place to look would be at the `CMakeUserPrests.json` and `CMakePresets.json` files. Here is how to build the project.

Clone the main branch. 

```bash
git clone https://github.com/agl-alexglopez/heap-allocator-workshop.git
```

Configure the release project on gcc or clang. Build from the root of the project.

GCC.

```bash
cmake --preset=mgcc-rel   
# or
make mgcc-rel
```

Clang.

```bash
cmake --preset=nclang-rel   
# or
make nclang-rel
```

To build the debug versions run `rm -rf build/` or `make clean` and then run the same commands, replacing `-rel` with `-deb`.

Then, regardless or your compiler, compile the project. If you used the Makefile this step is not necessary.

```bash
cmake --build build/
# If you edit files, you can recompile with the above command or use
make
```

Programs can then be found in the `build/bin/` folder if build in release mode or the `build/debug/bin/` folder if built in debug mode.

### Plot

Using `Matplot++`, we can plot the performance characteristics of each allocator at runtime. Each of the following commands will generate three plots: a total time taken to complete the specified interval for the operation, an average response time for an individual request over the interval of interest, and the overall utilization. You will be prompted at the command line to continue after closing the pop up graphs. To use the `plot.cc` program follow these instructions.

Observe the runtimes of `malloc/free` and `realloc/free` for all allocators. In other words, as the number of free blocks of memory we need to manage grows, what is the growth in runtime we see from our allocators? This question is answered with the following command.

```bash
./build/bin/plot -j8 -malloc
# or in debug mode
./build/debug/bin/plot -j8 -malloc
```

- The `-j[JOB COUNT]` flag lets you run the data gathering portion of the plotting program in parallel which can significantly speed things up. If you know the number of cores on your system enter that or fewer as the job count. I would not reccomend exceeding the number of physical cores on your system as that may interfere with timing data. The more jobs you specify the fewer other processes I would reccomend running on your computer. The default is 4 threads running in parallel.
- The `-malloc` flag specifies that we are interested in seeing the time it takes to insert `N` free nodes into our heap and then `malloc` them all out of the internal data structure we are using, where `N` increases across successive runs. Use the `-realloc` flag to generate the same data for the `realloc` function, assessing our speed in coalescing as well as inserting and removing free blocks of memory.

Compare performance of the allocators for a specific script.

```bash
./build/bin/plot -j8 scripts/time-trace-cargobuild.script
# or in debug mode
./build/debug/bin/plot -j8 scripts/time-trace-cargobuild.script
```

- The `.script` file can be found in the `scripts/` folder and specifies the specific script we want all of our allocators to run. We will then see a performance comparison plotted as a bar chart.

> **If you do not want the interactive pop up plots, enter the `-q` flag. All files are saved by default to the `output/` directory, whether or not the plots pop up. Quiet mode saves without showing anything.**

### Print Peaks

While not normally a part of a heap allocator API, I chose to enforce a printing function across all allocators so that we can see the internal representation they are using to store and manage free and allocated blocks in the heap. This started as a more advanced debugging function for helping with tricky tree rotations and operations, and stuck as a way to show state at any given moment.

This program acts as a mini GDB debugger, allowing you to see the state of the free node data structure at its peak. It will also allow you to set breakpoints on line numbers in the script and will show you the state of the free node data structure after that line has executed. At the end of execution, it will also show a graph of the number of free nodes over the lifetime of the heap and how the utilization may have changed during heap lifetime as well. For example, by default here is the output for a run on a smaller script. You will get the terminal printing output as well as a Matplot++ graph of free nodes over the heap lifetime as a pop up at the end.

![matplot-peaks](/images/matplot-peaks.png)

- All allocators have been prepended with the word `peaks_` and have been compiled to our `build/bin/` folder.
  - `peaks_rbtree_clrs`, `peaks_rbtree_unified`, `peaks_rbtree_linked`, etc.
- Run the default options to see what line of the script created the peak number of free nodes. Look at my printing debugger function for that allocator to see how the nodes are organized.
  - `./build/bin/peaks_rbtree_stack scripts/pattern-mixed.script`
- Run the default options in verbose mode with the `-v` flag. This flag can be included in any future options as well. This displays the free data structure with memory addresses included and extra information depending on the allocator. This is the printer I used because I needed to see memory addresses to better understand where errors were occurring.
  - `./build/bin/peaks_list_segregated -v scripts/pattern-mixed.script`
- Add breakpoints corresponding to line numbers in the script. This will show you how many free nodes existed after that line executes. You will also enter an interactive terminal session. You can decide if you want to continue to the next breakpoint, add a new future breakpoint, or continue to the end of the program execution. Be sure to follow the prompt directions.
  - `./build/bin/peaks_list_bestfit -v 100 200 450 -q scripts/pattern-mixed.script`
- Use the `-q` flag for quiet mode such that no Matplot++ graph pops up at the end.

### Correctness Tests

Check if an allocator passes a specific script or glob of scripts with the `ctest.cc` program. Use it as follows. This program runs extensive checks via its own logic and the internal validators of the heap allocators to ensure a correct implementation. Be careful running it on very large scripts, it may take quite a while!

```bash
./build/bin/ctest_splaytree_stack scripts/example* scripts/trace* scripts/pattern*
# or in debug mode
./build/debug/bin/ctest_splaytree_stack scripts/example* scripts/trace* scripts/pattern*
```

- Multiple scripts can be tested and we can use the `*` to expand out any scripts starting with the specified prefix.

If you want to run a batch test of all allocators use the `Makefile`.

```bash
make ctest-rel
# or in debug mode
make ctest-deb
```

### Unit Tests

The Google Test program provides more basic API unit tests regarding `malloc`, `realloc`, and `free`. There are also test for coalescing and some internal block management expectations. This program also uses an API enforced across all allocators to verify some basic internal state.

```bash
./build/bin/gtest_list_segregated
# or in debug mode
./build/debug/bin/gtest_list_segregated
```

Or, you can run all the allocators with the makefile.

```bash
make gtest-rel
# or in debug mode
make gtest-deb
```

### Python Script Generation

There are two important steps to measuring the performance of these heap allocators: tracing the heap usage of real world programs to emulate on my allocators, and generating artificial scripts for performance testing. The first step is the more straightforward. I assume you are running all commands for this section in the `pysrc/` directory.

#### Ltrace Output

The program parsing.py is capable of taking output from the Linux `ltrace` command and cleaning it up for a `.script` file. These `.script` files are then used to conveniently test a custom heap allocator. Here are the directions for the ltrace command.

Add the memory calls that we want to watch for and pick an output file. Then execute the desired command on the desired file as follows.

```bash
ltrace -e malloc+realloc+calloc -e free-@libc.so* --output [FILENAME] [COMMAND]
```

With an editor like emacs that would look like this.

```bash
ltrace -e malloc+realloc+calloc -e free-@libc.so* --output output.txt emacs myfile.txt
```

If you have problems with this command try excluding the `-e free-@libc.so*` portion. We will get output like this.

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

#### Ltrace Parsing

Once you have generated your ltrace output to the desired `.txt` file, feed it to this python program as follows.

```zsh
python3 parsing.py -parse [LTRACE_OUTPUT_FILE] [DESIRED_SCRIPT_FILE]
```

For example a command may look like this.

```zsh
python3 parsing.py -parse ltrace-output/ltrace-output.txt ../scripts/time-ltrace-parsed.script
```

#### Script Generation

Generating custom scripts for a heap allocator is more complex and allows for more options. By default, we can allocate, free, and reallocate as much memory as we want, and our program will free all remaining memory for the script file at the end so the allocators do not leak memory due to an oversite in the script.

#### Allocations

To allocate blocks of memory in a script file use the following commands or options as needed. These commands are run as if you are currently in the `pysrc/` directory, where the python program is located.

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

#### Reallocations

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

#### Free

To free blocks of memory in a script file use the following commands or options as needed. As a special note, freeing memory defaults to freeing every other block of allocated memory. This is done because my allocator coalesced the blocks to its left and right on every call to `realloc()` or `free()`. This made it hard to build large pools of free memory because I was just reforming one large pool of free memory as I made calls to free. To better test performance with more free nodes, freeing every other block will ensure that the desired number of nodes enters the tree. Be sure to have double the amount of allocated memory compared to your number of free requests. If you want to manage N free nodes, make 2N allocations first.

Free all memory that has been allocated up to this point. This will free every other block and then go back and free the rest.

```zsh
python3 parsing.py -generate ../scripts/10k-frees.script -alloc 10000 -realloc 5000 -free
```

Free a specific number of blocks of memory. This is good for building a pool of free nodes and then making requests to `malloc()` to take nodes from the pool.

```zsh
python3 parsing.py -generate ../scripts/5k-insertdelete.script -alloc 10000 -free 5000 -alloc 5000
```

#### Build Your Own

These commands can be combined to create scripts that allow us to time specific operations on our heap. For example, here is the command I used to test the coalescing speed of different implementations of my heap allocator. I wanted to see how quickly we could reallocate 100,000 blocks of allocated memory, which would involve coalescing many nodes.

```zsh
python3 parsing.py -generate ../scripts/time-100kinsertdelete.script '-alloc(1,500)' 200000 -free 100000 '-alloc(1,500)' 100000
```

Here are two commands you can run if you would like to generate all the missing files I used to time my allocators. They are ignored by default because they are too large, so you must generate them yourself if you want to test the allocators on larger files.

> **Be warned, depending on the runtime promises of an allocator, it may not be able to finish the larger files (more than about 50-80k alloc or realloc requests) in a reasonable amount of time. However, at this time, all allocators in this repo don't have problems with the largest scripts.**

Generate a decent range of files that test allocators' abilities to quicky `malloc()` and `free()` from and to free node structures of increasing size.

Run this from the `pysrc/` directory.

```bash
(python3 parsing.py -generate ../scripts/time-insertdelete-05k.script '-alloc(1,500)' 10000 -free 5000 '-alloc(1,500)' 5000) && (python3 parsing.py -generate ../scripts/time-insertdelete-10k.script '-alloc(1,500)' 20000 -free 10000 '-alloc(1,500)' 10000) && (python3 parsing.py -generate ../scripts/time-insertdelete-15k.script '-alloc(1,500)' 30000 -free 15000 '-alloc(1,500)' 15000) && (python3 parsing.py -generate ../scripts/time-insertdelete-20k.script '-alloc(1,500)' 40000 -free 20000 '-alloc(1,500)' 20000) && (python3 parsing.py -generate ../scripts/time-insertdelete-25k.script '-alloc(1,500)' 50000 -free 25000 '-alloc(1,500)' 25000) && (python3 parsing.py -generate ../scripts/time-insertdelete-30k.script '-alloc(1,500)' 60000 -free 30000 '-alloc(1,500)' 30000) && (python3 parsing.py -generate ../scripts/time-insertdelete-35k.script '-alloc(1,500)' 70000 -free 35000 '-alloc(1,500)' 35000) && (python3 parsing.py -generate ../scripts/time-insertdelete-40k.script '-alloc(1,500)' 80000 -free 40000 '-alloc(1,500)' 40000) && (python3 parsing.py -generate ../scripts/time-insertdelete-45k.script '-alloc(1,500)' 90000 -free 45000 '-alloc(1,500)' 45000) && (python3 parsing.py -generate ../scripts/time-insertdelete-50k.script '-alloc(1,500)' 100000 -free 50000 '-alloc(1,500)' 50000) && (python3 parsing.py -generate ../scripts/time-insertdelete-55k.script '-alloc(1,500)' 110000 -free 55000 '-alloc(1,500)' 55000) && (python3 parsing.py -generate ../scripts/time-insertdelete-60k.script '-alloc(1,500)' 120000 -free 60000 '-alloc(1,500)' 60000) && (python3 parsing.py -generate ../scripts/time-insertdelete-70k.script '-alloc(1,500)' 140000 -free 70000 '-alloc(1,500)' 70000) && (python3 parsing.py -generate ../scripts/time-insertdelete-80k.script '-alloc(1,500)' 160000 -free 80000 '-alloc(1,500)' 80000) && (python3 parsing.py -generate ../scripts/time-insertdelete-90k.script '-alloc(1,500)' 180000 -free 90000 '-alloc(1,500)' 90000) && (python3 parsing.py -generate ../scripts/time-insertdelete-100k.script '-alloc(1,500)' 200000 -free 100000 '-alloc(1,500)' 100000) && (python3 parsing.py -generate ../scripts/time-insertdelete-55k.script '-alloc(1,500)' 110000 -free 55000 '-alloc(1,500)' 55000) && (python3 parsing.py -generate ../scripts/time-insertdelete-65k.script '-alloc(1,500)' 130000 -free 65000 '-alloc(1,500)' 65000) && (python3 parsing.py -generate ../scripts/time-insertdelete-75k.script '-alloc(1,500)' 150000 -free 75000 '-alloc(1,500)' 75000) && (python3 parsing.py -generate ../scripts/time-insertdelete-85k.script '-alloc(1,500)' 170000 -free 85000 '-alloc(1,500)' 85000) && (python3 parsing.py -generate ../scripts/time-insertdelete-95k.script '-alloc(1,500)' 190000 -free 95000 '-alloc(1,500)' 95000) && (python3 parsing.py -generate ../scripts/time-insertdelete-75k.script '-alloc(1,500)' 150000 -free 75000 '-alloc(1,500)' 75000) && (python3 parsing.py -generate ../scripts/time-insertdelete-85k.script '-alloc(1,500)' 170000 -free 85000 '-alloc(1,500)' 85000) && (python3 parsing.py -generate ../scripts/time-insertdelete-95k.script '-alloc(1,500)' 190000 -free 95000 '-alloc(1,500)' 95000)
```

Generate files to test an allocator's ability to quickly `realloc()` allocated nodes in the heap. This a good test of coalescing speed.

Run this from the `pysrc/` directory.

```bash
(python3 parsing.py -generate ../scripts/time-reallocfree-05k.script '-alloc(1,500)' 10000 -free 5000 '-realloc(200,1000)' 5000) && (python3 parsing.py -generate ../scripts/time-reallocfree-10k.script '-alloc(1,500)' 20000 -free 10000 '-realloc(200,1000)' 10000) && (python3 parsing.py -generate ../scripts/time-reallocfree-15k.script '-alloc(1,500)' 30000 -free 15000 '-realloc(200,1000)' 15000) && (python3 parsing.py -generate ../scripts/time-reallocfree-20k.script '-alloc(1,500)' 40000 -free 20000 '-realloc(200,1000)' 20000) && (python3 parsing.py -generate ../scripts/time-reallocfree-25k.script '-alloc(1,500)' 50000 -free 25000 '-realloc(200,1000)' 25000) && (python3 parsing.py -generate ../scripts/time-reallocfree-30k.script '-alloc(1,500)' 60000 -free 30000 '-realloc(200,1000)' 30000) && (python3 parsing.py -generate ../scripts/time-reallocfree-35k.script '-alloc(1,500)' 70000 -free 35000 '-realloc(200,1000)' 35000) && (python3 parsing.py -generate ../scripts/time-reallocfree-40k.script '-alloc(1,500)' 80000 -free 40000 '-realloc(200,1000)' 40000) && (python3 parsing.py -generate ../scripts/time-reallocfree-45k.script '-alloc(1,500)' 90000 -free 45000 '-realloc(200,1000)' 45000) && (python3 parsing.py -generate ../scripts/time-reallocfree-50k.script '-alloc(1,500)' 100000 -free 50000 '-realloc(200,1000)' 50000) && (python3 parsing.py -generate ../scripts/time-reallocfree-55k.script '-alloc(1,500)' 110000 -free 55000 '-realloc(200,1000)' 55000) && (python3 parsing.py -generate ../scripts/time-reallocfree-60k.script '-alloc(1,500)' 120000 -free 60000 '-realloc(200,1000)' 60000) && (python3 parsing.py -generate ../scripts/time-reallocfree-65k.script '-alloc(1,500)' 130000 -free 65000 '-realloc(200,1000)' 65000) && (python3 parsing.py -generate ../scripts/time-reallocfree-70k.script '-alloc(1,500)' 140000 -free 70000 '-realloc(200,1000)' 70000) && (python3 parsing.py -generate ../scripts/time-reallocfree-75k.script '-alloc(1,500)' 150000 -free 75000 '-realloc(200,1000)' 75000) && (python3 parsing.py -generate ../scripts/time-reallocfree-80k.script '-alloc(1,500)' 160000 -free 80000 '-realloc(200,1000)' 80000) && (python3 parsing.py -generate ../scripts/time-reallocfree-85k.script '-alloc(1,500)' 170000 -free 85000 '-realloc(200,1000)' 85000) && (python3 parsing.py -generate ../scripts/time-reallocfree-90k.script '-alloc(1,500)' 180000 -free 90000 '-realloc(200,1000)' 90000) && (python3 parsing.py -generate ../scripts/time-reallocfree-95k.script '-alloc(1,500)' 190000 -free 95000 '-realloc(200,1000)' 95000) && (python3 parsing.py -generate ../scripts/time-reallocfree-100k.script '-alloc(1,500)' 200000 -free 100000 '-realloc(200,1000)' 100000)
```

### Sandbox Program

This was a program to help test the allocators in "real" `c` context. Ideally we would take a previous program that utilized the heap and exchange all standard library heap calls and replace them with our own, along with a few other steps such as initializing our heap segment. I left the some stubs in that initialize the allocator and then you can code up any program you wish with the workshop allocator!

#### How to Use the Sandbox

- All allocators have been prepended with the word `sandbox_` and have been compiled to our `build/bin/` or `build/debug/bin/` folder.
  - `sandbox_rbtree_clrs`, `sandbox_rbtree_unified`, `sandbox_splaytree_topdown`, etc.
- The rest is up to you. Design whatever program you would like.

