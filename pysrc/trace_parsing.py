"""
Alex G. Lopez 2022.07.28

trace_parsing.py

This file contains the python program for generating scripts for heap allocators. This program
can generate artificial scripts or process the heap traces of real programs run in the terminal.
The driving program to generate real-world scripts is the 'ltrace' command. We process that
output here if necessary.

The four outputs of interest from the heap tracing files are as follows:

    make->calloc(8192, 1)                            = 0x56167a22c440
    make->malloc(48)                                 = 0x56167a2418e0
    make->realloc(0x56167a22c440, 8)                 = 0x56167a252a00
    make->free(0x56167a22c440)                       = <void>

Their outputs should be to a .script file with the following format:

    a 1 8192
    a 2 48
    r 1 8
    f 1

Malloc and calloc are the same in the view of this program and we will always produce a valid
allocation before a reallocation, even if we must quickly make one artificially. Freeing an
address that has not yet been allocated will be ignored.
"""

import sys
import enum


class Heap_Call(enum.Enum):
    alloc = 1
    realloc = 2
    free = 3


def get_heap_address(line):
    """
    Given a line of text, determines the hexadecimal address of the heap request. However, we will
    just take the number as a unique integer to put into our map, base 10.

    >>> get_heap_address('gcc->malloc(48)=0x18102a0')
    25232032
    >>> get_heap_address('gcc->free(0x1836e50)=<void>')
    25390672
    >>> get_heap_address('gcc -g3 -O0 -std=gnu99 -Wall $warnflags  triangle.c  -o triangle')
    >>> get_heap_address('+++ exited (status 0) +++')
    >>> get_heap_address('---SIGCHLD (Child exited)---')
    """
    # The arrow prepends any malloc, calloc, realloc, or free.
    if '->' not in line:
        return None
    equals = line.find('=')
    first_half = line[:equals]
    second_half = line[equals + 1:]
    # Free only has the hex id in the first half in the free() call.
    if second_half == '<void>':
        open_bracket = first_half.find('free(')
        i = open_bracket + 5
        while i < len(first_half) and first_half[i] != ')':
            i += 1
        address = first_half[open_bracket + 5: i]
        return int(address, 16)
    return int(second_half, 16)


def parse_file_heap_use(input_trace):
    """
    Given a file with the output from the ltrace command on unix like systems, add lines to a
    .script file corresponding to the requests to the heap.
    """
    memory_dict = {}
    memory_ids = 0

    # open the file
    with open(input_trace, encoding='utf-8') as f:
        # For each line in the file
        for line in f:
            line = line.strip()
            # Get the call and memory address
            heap_address = get_heap_address(line)

            # If the memory address already exists,
            if heap_address and heap_address in memory_dict:

                call = get_heap_call(line)
                # check if the call is free or realloc
                if call == Heap_Call.free:
                    memory_dict.pop(heap_address)
                    add_line(Heap_Call.free, memory_dict[heap_address], 0)
                elif call == Heap_Call.realloc:
                    add_line(Heap_Call.realloc, memory_dict[heap_address], get_heap_bytes(line))
            # If the call is new
            elif heap_address:
                # If the call is free and address not in dict, discard
                call = get_heap_call(line)
                # If the call is realloc
                if call == Heap_Call.realloc:
                    memory_dict[heap_address] = memory_ids
                    add_line(Heap_Call.alloc, memory_ids, 8)
                    add_line(Heap_Call.realloc, memory_ids, get_heap_bytes(line))
                    memory_ids += 1
                    # If the address is not in the dict, make a malloc then realloc call.
                elif call == Heap_Call.alloc:
                    memory_dict[heap_address] = memory_ids
                    add_line(Heap_Call.alloc, memory_ids, get_heap_bytes(line))
                    memory_ids += 1
