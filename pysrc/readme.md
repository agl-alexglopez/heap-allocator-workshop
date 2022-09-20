# Parsing and Generating Files

## Ltrace Output

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

## Ltrace Parsing

Once you have generated your ltrace output to the desired `.txt` file, feed it to this python program as follows.

```zsh
python3 parsing.py -parse [LTRACE_OUTPUT_FILE] [DESIRED_SCRIPT_FILE]
```

For example a command may look like this.

```zsh
python3 parsing.py -parse ltrace-output/ltrace-output.txt ../scripts/time-ltrace-parsed.script
```

## Script Generation

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

## Reallocations

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

## Free

To free blocks of memory in a script file use the following commands or options as needed. As a special note, freeing memory defaults to freeing every other block of allocated memory. This is done because my allocator coalesced the blocks to its left and right on every call to `realloc()` or `free()`. This made it hard to build large trees of free memory because I was just reforming one large pool of free memory as I made calls to free. To better test performance with more free nodes, freeing every other block will ensure that the desired number of nodes enters the tree. Be sure to have double the amount of allocated memory compared to your number of free requests. If you want a tree of N free nodes, make 2N allocations first.

Free all memory that has been allocated up to this point. This will free every other block and then go back and free the rest.

```zsh
python3 parsing.py -generate ../scripts/10k-frees.script -alloc 10000 -realloc 5000 -free
```

Free a specific number of blocks of memory. This is good for building a tree of free nodes and then making requests to `malloc()` to take nodes from that tree.

```zsh
python3 parsing.py -generate ../scripts/5k-insertdelete.script -alloc 10000 -free 5000 -alloc 5000
```

## Conclusions

These commands can be combined to create scripts that allow us to time specific operations on our heap. For example, here is the command I used to test the coalescing speed of different implementations of my heap allocator. I wanted to see how quickly we could reallocate 100,000 blocks of allocated memory, which would involve coalescing many nodes in the tree.

```zsh
python3 parsing.py -generate ../scripts/time-100kinsertdelete.script '-alloc(1,500)' 200000 -free 100000 '-alloc(1,500)' 100000
```

Here are two commands you can run if you would like to generate all the missing files I used to time my allocators. They are ignored by default because they are too large, so you must generate them yourself if you want to test the allocators on larger files.

> **Be warned, the `list_` based allocators will not be able to finish the larger files (more than about 50-80k alloc or realloc requests) in a reasonable amount of time.**

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

