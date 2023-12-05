"""
Author: Alex G. Lopez

parsing.py

This file contains the python program for generating scripts for heap allocators. This program
can generate artificial scripts or process the heap traces of real programs.
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

We may also generate our own custom scripts with the script generation functions. This is helpful for runtime analysis
of heap allocators on artificial workloads. We can manage the size of our free data structure of heap memory. This
usage is more robust and allows for many patterns. Please see the readme for detailed instructions.
"""

import enum
import random
import sys

ALL_IDS = -2

class Heap_Strings():
    malloc = 'malloc'
    calloc = 'calloc'
    realloc = 'realloc'
    free = 'free'


class Heap_Call(enum.Enum):
    alloc = 1
    realloc = 2
    free = 3


class Print_Call():
    alloc = 'a'
    realloc = 'r'
    free = 'f'
    leak = 'l'


def get_heap_address(line):
    """
    Given a line of text, determines the hexadecimal address of the heap request. However, we will
    just take the number as a unique integer to put into our map, base 10.
    >>> get_heap_address('gcc->malloc(48)=0x18102a0')
    25232032
    >>> get_heap_address('gcc->free(0x1836e50)=<void>')
    25390672
    >>> get_heap_address('gcc-g3-O0-std=gnu99-Wall$warnflagstriangle.c-otriangle')
    >>> get_heap_address('+++exited(status0)+++')
    >>> get_heap_address('---SIGCHLD(Childexited)---')
    >>> get_heap_address('nvim->free(0x22e8f10<noreturn...>')
    36605712
    """
    # The arrow prepends any malloc, calloc, realloc, or free.
    call_index = line.find('->')
    if call_index == -1:
        return None
    open_paren = line.find('(', call_index)
    # Some edgecase errors can interrupt a normal free line, so find free id by name rather than = sign.
    if line[call_index + 2:open_paren] == 'free':
        i = open_paren + 1
        while i < len(line) and line[i].isalnum():
            i += 1
        try:
            return int(line[open_paren + 1: i], 16)
        except ValueError:
            return None
    # Now we have a more simple search because we know ID is on the right side of equals sign.
    equals = line.find('=', call_index)
    second_half = line[equals + 1:]
    try:
        return int(second_half, 16)
    except ValueError:
        return None


def get_heap_call(line):
    """
    Given a line of text from an ltrace output, determines the type of call to heap we will place in our script file:
    alloc, realloc, or free. Returns the appropriate call.

    >>> get_heap_call('make->calloc(8192,1)=0x56167a22c440')
    <Heap_Call.alloc: 1>
    >>> get_heap_call('make->malloc(8192)=0x56167a22c440')
    <Heap_Call.alloc: 1>
    >>> get_heap_call('make->free(0x56167a22c440)=<void>')
    <Heap_Call.free: 3>
    >>> get_heap_call('make->realloc(0x56167a22c440,8192)=0x56167a22c440')
    <Heap_Call.realloc: 2>
    >>> get_heap_address('gcc-g3-O0-std=gnu99-Wall$warnflagstriangle.c-otriangle')
    >>> get_heap_address('+++exited(status0)+++')
    >>> get_heap_address('---SIGCHLD(Childexited)---')
    >>> libc.so.6->calloc(94045449572224, 240)  = 0x5588a99d4f80
    """
    if line.startswith("libc."):
        return None
    call_index = line.find('->')
    if call_index == -1:
        return None
    i = call_index + 2
    while i < len(line) and line[i] != '(':
        i += 1
    call = line[call_index + 2: i]
    if call == Heap_Strings.calloc or call == Heap_Strings.malloc:
        return Heap_Call.alloc
    elif call == Heap_Strings.realloc:
        return Heap_Call.realloc
    elif call == Heap_Strings.free:
        return Heap_Call.free


def get_heap_bytes(line):
    """
    Given a line of text from an ltrace output file returns the number of bytes requested for the heap call if relevant.
    Calloc, malloc, and realloc will all require byte size information. Free requires no byte size. We return the bytes
    or None if no bytes are found.
    >>> get_heap_bytes('make->calloc(8192,1)=0x56167a22c440')
    '8192'
    >>> get_heap_bytes('make->calloc(8192,4)=0x56167a22c440')
    '32768'
    >>> get_heap_bytes('make->malloc(8192)=0x56167a22c440')
    '8192'
    >>> get_heap_bytes('make->free(0x56167a22c440)=<void>')
    >>> get_heap_bytes('make->realloc(0x56167a22c440,8192)=0x56167a22c440')
    '8192'
    >>> get_heap_bytes('make->calloc(8192,4<no_return>=...')
    >>> get_heap_bytes('make->malloc(8192<no_return>=...')
    >>> get_heap_bytes('make->realloc(0x56167a22c440,8192<noreturn>...=0x56167a22c440')
    >>> get_heap_bytes('make-><resuming>=...')
    >>> get_heap_bytes('gcc-g3-O0-std=gnu99-Wall$warnflagstriangle.c-otriangle')
    >>> get_heap_bytes('+++exited(status0)+++')
    >>> get_heap_bytes('---SIGCHLD(Childexited)---')
    """
    arrow = line.find('->')
    if arrow == -1:
        return None
    open_bracket = line.find('(', arrow)
    specific_call = line[arrow + 2: open_bracket]
    if specific_call == Heap_Strings.calloc:
        comma = line.find(',', open_bracket)
        number_of_items = line[open_bracket + 1: comma]
        closing_bracket = line.find(')', comma)
        byte_size = line[comma + 1: closing_bracket]
        # There are some strange calloc(1,16<noreturn> erors that can mess up int function.
        try:
            return str(int(number_of_items) * int(byte_size))
        except ValueError:
            return None

    elif specific_call == Heap_Strings.malloc:
        closing_bracket = line.find(')', open_bracket)
        full_bytes = line[open_bracket + 1: closing_bracket]
        try:
            return str(int(full_bytes))
        except ValueError:
            return None

    elif specific_call == Heap_Strings.realloc:
        comma = line.find(',', open_bracket)
        closing_bracket = line.find(')', comma)
        full_bytes = line[comma + 1: closing_bracket]
        try:
            return str(int(full_bytes))
        except ValueError:
            return None
    return None


def add_line(call_type, memory_id, total_bytes):
    """
    Given a heap call type, free, alloc, or realloc, returns a string formatted for .script files as appropriate.
    >>> add_line(Heap_Call.free, 4, '0')
    'f 4'
    >>> add_line(Heap_Call.alloc, 3, '900')
    'a 3 900'
    >>> add_line(Heap_Call.realloc, 3, '1200')
    'r 3 1200'
    """
    if call_type == Heap_Call.free:
        return f'{Print_Call.free} {memory_id}'
    elif call_type == Heap_Call.alloc:
        return f'{Print_Call.alloc} {memory_id} {total_bytes}'
    elif call_type == Heap_Call.realloc:
        return f'{Print_Call.realloc} {memory_id} {total_bytes}'


def print_heap_call(line, memory_dict, memory_ids):
    """
    Given a line of ltrace output to process, a dictionary of currently active memory ids, and our most recent memory
    id, print a text line for a script file of either allocate "a", reallocate "r", or free "f". Return the memory id
    which will remain the same on free, or possibly be updated upon realloc or alloc.

    >>> print_heap_call('gcc->malloc(48)=0x18102a0', {}, 0)
    a 0 48
    1
    >>> print_heap_call('gcc->free(0x1836e50)=<void>', {25390672:[0]}, 1)
    f 0
    1
    >>> print_heap_call('gcc->free(0x1836e50)=<void>', {25390672:[0,1,2,3]}, 4)
    f 3
    4
    >>> print_heap_call('gcc-g3-O0-std=gnu99-Wall$warnflagstriangle.c-otriangle', {25390672:[0]}, 1)
    1
    >>> print_heap_call('+++exited(status0)+++', {25390672:[0]}, 1)
    1
    >>> print_heap_call('---SIGCHLD(Childexited)---', {25390672:[0]}, 1)
    1
    >>> print_heap_call('nvim->free(0x22e8f10<noreturn...>', {25390672:[0]}, 1)
    1
    """
    line = line.strip()
    line = line.replace(" ", "")
    # Get the call and memory address
    heap_address = get_heap_address(line)

    # We will early return if the line is not related to heap calls and returns an empty string.
    if not heap_address:
        return memory_ids

    call = get_heap_call(line)
    # Ignore any frees that are not in our dictionary.
    if call == Heap_Call.free and heap_address in memory_dict:
        id_list = memory_dict[heap_address]
        to_del = id_list[len(id_list) - 1]
        print(add_line(Heap_Call.free, to_del, 0))
        id_list.pop()
        if (len(id_list) == 0):
            memory_dict.pop(heap_address)

    elif call == Heap_Call.realloc:
        if (heap_address not in memory_dict):
            memory_dict[heap_address] = [memory_ids]
            print(add_line(Heap_Call.alloc, memory_ids, 8))
            memory_ids += 1
        # We have multiple malloc callocs under same id so just choose one at random to realloc.
        to_realloc = random.choice(memory_dict[heap_address])
        print(add_line(Heap_Call.realloc, to_realloc, get_heap_bytes(line)))

    elif call == Heap_Call.alloc:
        if (heap_address not in memory_dict):
            memory_dict[heap_address] = []
        memory_dict[heap_address].append(memory_ids)
        print(add_line(Heap_Call.alloc, memory_ids, get_heap_bytes(line)))
        memory_ids += 1
    return memory_ids


def parse_file_heap_use(input_trace):
    """
    Given a file with the output from the ltrace command on unix like systems, add lines to a
    .script file corresponding to the requests to the heap.
    """
    memory_dict = {}
    memory_ids = 0
    with open(input_trace, encoding='utf-8') as f:
        for line in f:
            memory_ids = print_heap_call(line, memory_dict, memory_ids)
        for idx, val in enumerate(memory_dict.values()):
            for i, v in enumerate(val):
                if idx == len(memory_dict) - 1 and i == len(val) - 1:
                    print(add_line(Heap_Call.free, v, 0), end='')
                else:
                    print(add_line(Heap_Call.free, v, 0))


def identify_call(call_string):
    """
    Given a string containing a heap request determine the request and return the appropriate
    printable request type.
    >>> identify_call('realloc ')
    'r'
    >>> identify_call('alloc(200) ')
    'a'
    >>> identify_call('free 500')
    'f'
    """
    i = 0
    while i < len(call_string) and call_string[i].isalpha():
        i += 1
    call = call_string[:i]
    if call == 'alloc':
        return Print_Call.alloc
    elif call == 'realloc':
        return Print_Call.realloc
    elif call == 'free':
        return Print_Call.free
    elif call == 'leak':
        return Print_Call.leak


def identify_byte_range(call_str):
    """
    Given a string of a heap request, returns a tuple of the byte
    range the user desires as an integer. The user may not specify
    the single size or range they desire in which case a random
    allocation will be generated for them.
    >>> identify_byte_range('alloc(500) ')
    (500, None)
    >>> identify_byte_range('alloc(50,500) ')
    (50, 500)
    >>> identify_byte_range('realloc(500,1200) ')
    (500, 1200)
    """
    i = 0
    while i < len(call_str) and call_str[i] != '(' and call_str[i] != ' ':
        i += 1
    # The user has not made any specification on range so we choose random
    if i == len(call_str) or call_str[i] == ' ':
        return random.randint(1, 50), random.randint(200, 1200)
    j = i + 1
    while j < len(call_str) and call_str[j] != ',' and call_str[j] != ')':
        j += 1
    # The user has requested uniform sizes of all requests.
    if call_str[j] == ')':
        uniform_byte_size = call_str[i + 1: j]
        return int(uniform_byte_size), None
    # The user has entered alloc(lower_bound,upper_bound)
    lower_bound = call_str[i + 1: j]
    closing_bracket = call_str.find(')', j)
    upper_bound = call_str[j + 1: closing_bracket]
    return int(lower_bound), int(upper_bound)


def identify_num_requests(call_str):
    """
    Given a string with a request pattern from the user, return the integer number of
    requests. If there is no number specifying the number of arguments, we will run over
    all current allocations with the given request by returning the ALL_IDS constant.
    >>> identify_num_requests('alloc(single_byte_size) 10000 -realloc')
    10000
    >>> identify_num_requests('-realloc(500) 600 -free')
    600
    >>> identify_num_requests('-realloc(500,1200) -free')
    -2
    >>> identify_num_requests('-realloc(500,1200) 5000')
    5000
    >>> identify_num_requests('-free')
    -2
    >>> identify_num_requests('-free 600')
    600
    >>> identify_num_requests('free')
    -2
    """
    space = call_str.find(' ')
    # We are out of arguments
    if space == -1 or call_str[space + 1] == '-':
        return ALL_IDS
    next_space = call_str.find(' ', space + 1)
    # Free a certain number was the last user request.
    if next_space == -1:
        return int(call_str[space + 1:])
    return int(call_str[space + 1: next_space])


def generate_frees(arg_string, id_byte_map, free_ids):
    """
    Given an argument string from the command line, a dict of ids to manage, and our last free id, print the appropriate
    free calls, manage the dict, and return the new free_id. Free id's default to freeing every other block of allocated
    memory for the purposes of artificial workloads. This prevents coalescing from absorbing all memory into one large
    block if we want to create a large data structure of free memory.
    >>> generate_frees('free', {0:2,1:10,2:400}, 0)
    f 0
    f 2
    4
    >>> generate_frees('free 2', {0:2,1:10,2:400,3:500,4:600}, 0)
    f 0
    f 2
    4
    >>> generate_frees('free 3', {0:2,1:10,2:400,3:500,4:600}, 0)
    f 0
    f 2
    f 4
    6
    """
    num_requests = identify_num_requests(arg_string[1:])
    if num_requests == ALL_IDS:
        num_requests = len(id_byte_map)
    for i in range(free_ids, num_requests):
        # Prevent a key error here if the user enters more frees than they have allocated memory.
        if free_ids in id_byte_map:
            id_byte_map.pop(free_ids)
            print(f'{Print_Call.free} {free_ids}')
            free_ids += 2
        # No point in continuing useless loop. We have mismatched allocation and free quantities.
        else:
            break
    return free_ids


def generate_allocs(arg_string, id_byte_map, alloc_ids):
    """
    Given an argument string from the command line, a dict of ids to manage, the last alloc id, print the appropriate
    alloc calls, manage the dict, and return the new alloc_id. We allow the user to define one sized set of allocations,
    a size range, or no sizes, in which case we will choose random sizes for them.
    >>> generate_allocs('alloc(20) 3', {}, 0)
    a 0 20
    a 1 20
    a 2 20
    3
    """
    byte_tuple = identify_byte_range(arg_string[1:])
    num_requests = identify_num_requests(arg_string[1:])
    for i in range(num_requests):
        if byte_tuple[0] and byte_tuple[1]:
            id_byte_map[alloc_ids] = random.randint(byte_tuple[0], byte_tuple[1])
            print(f'{Print_Call.alloc} {alloc_ids} {id_byte_map[alloc_ids]}')
        else:
            id_byte_map[alloc_ids] = byte_tuple[0]
            print(f'{Print_Call.alloc} {alloc_ids} {id_byte_map[alloc_ids]}')
        alloc_ids += 1
    return alloc_ids


def generate_reallocs(arg_string, id_byte_map, realloc_ids):
    """
    Given an argument string from the command line, a dict of ids to manage, the last realloc id, print the appropriate
    realloc calls, manage the dict, and return the new realloc_id. We allow the user to define one sized set of
    reallocations, a size range, or no sizes, in which case we will choose random sizes for them.
    >>> generate_reallocs('realloc(20) 3', {0:1,1:5,2:10}, 0)
    r 0 20
    r 1 20
    r 2 20
    0
    """
    byte_tuple = identify_byte_range(arg_string[1:])
    num_requests = identify_num_requests(arg_string[1:])
    if num_requests == ALL_IDS:
        num_requests = len(id_byte_map)
    for i in range(num_requests):

        # We also need to skip over gaps that may have formed in the map from frees.
        while realloc_ids not in id_byte_map:
            realloc_ids = (realloc_ids + 1) % len(id_byte_map)

        if byte_tuple[0] and byte_tuple[1]:
            print(f'{Print_Call.realloc} {realloc_ids} {random.randint(byte_tuple[0], byte_tuple[1])}')
        else:
            print(f'{Print_Call.realloc} {realloc_ids} {byte_tuple[0]}')

        # If we just want to have a certain number of reallocs as a test they will just wrap.
        realloc_ids = (realloc_ids + 1) % len(id_byte_map)
    return realloc_ids


def generate_file_heap_use(arg_array):
    """
    Given an array of heap requests to process of the pattern '-request number_of_requests'
    generate a script that follows the requested pattern. By default requests to free
    memory will free every other memory address, otherwise coalescing would create one large
    free block.
    """
    id_byte_map = {}
    alloc_ids = 0
    free_ids = 0
    realloc_ids = 0
    for idx, arg in enumerate(arg_array):
        arg_string = ' '.join(arg_array[idx:])
        if arg_string[0] == '-':
            # Identify the argument
            arg_call = identify_call(arg_string[1:])
            if arg_call == Print_Call.free:
                free_ids = generate_frees(arg_string, id_byte_map, free_ids)
            elif arg_call == Print_Call.alloc:
                alloc_ids = generate_allocs(arg_string, id_byte_map, alloc_ids)
            elif arg_call == Print_Call.realloc:
                realloc_ids = generate_reallocs(arg_string, id_byte_map, realloc_ids)
            # Heap calls will be left as is and we will not automatically free all allocated memory.
            elif arg_call == Print_Call.leak:
                return

    # We will have a thorough cleanup in order to avoid possible leaks in user's heap allocator.
    for idx, item in enumerate(id_byte_map):
        if idx == len(id_byte_map) - 1:
            print(f'{Print_Call.free} {item}', end='')
        else:
            print(f'{Print_Call.free} {item}')


def validate_script(file):
    """
    Given a generated script file, verifies that the calls to the heap are logical. We are mainly concerned with frees
    and reallocations because we need to have existing allocations in place to complete those two operations.
    """
    # Track the most recent request for each memory id number. Make sure they are logical.
    memory_request_dict = {}
    with open(file, 'r', encoding='utf-8') as f:
        for idx, line in enumerate(f):
            line_lst = line.split()
            request_type = line_lst[0]
            memory_id = line_lst[1]
            if memory_id not in memory_request_dict:
                if request_type == 'r':
                    raise ValueError(f'line {idx + 1}. Did not properly add alloc before incoming realloc.')
                elif request_type == 'f':
                    raise ValueError(f'line {idx + 1}. Free request for memory id not in the script.')
                memory_request_dict[memory_id] = request_type
            else:
                # Keeping the last request should be helpful because we can spot use after free.
                if request_type == 'a' and memory_request_dict[memory_id] == 'a':
                    raise ValueError(f'line {idx + 1}. Two allocations with same memory id should not be possible.')
                elif request_type == 'f':
                    memory_request_dict.pop(memory_id)


def main():
    args = sys.argv[1:]

    # -parse trace_input parsed_file_output
    if len(args) == 3 and args[0] == '-parse':
        original_stdout = sys.stdout
        with open(args[2], 'w', encoding='utf-8') as f:
            sys.stdout = f
            parse_file_heap_use(args[1])
            sys.stdout = original_stdout
            validate_script(args[2])
            print('Ltrace successfully parsed!')

    # Requests to generate custom scripts must always begin with alloc.
    # Unspecified range of request size will be random.
    # -generate filename -alloc 10000 -free 5000

    # DO NOT USE SPACES FOR BYTE RANGES BETWEEN PARENTHESIS. WRAP IN 'QUOTES'
    # -generate filename '-alloc(smallest_byte_size,largest_byte_size)' 10000 -free 5000
    # -generate filename '-alloc(single_byte_size)' 10000 -free 5000

    # realloc will default to reallocing every alloc to a random size.
    # -generate filename '-alloc(single_byte_size)' 10000 -realloc
    # or it can realloc all to a specified size
    # -generate filename '-alloc(single_byte_size)' 10000 '-realloc(800)'
    # or it can realloc all within a random range.
    # -generate filename '-alloc(single_byte_size)' 10000 '-realloc(800,1200)'
    # or it can realloc a certain number of allocations.
    # -generate filename '-alloc(single_byte_size)' 10000 '-realloc(800,1200)' 500 -free
    # Finally, if you do not want to automatically free all memory at end of script use -leak flag.
    # -generate filename '-alloc(single_byte_size)' 10000 '-free(800,1200) 5000' -leak
    if len(args) >= 4 and args[0] == '-generate':
        original_stdout = sys.stdout
        with open(args[1], 'w', encoding='utf-8') as f:
            print('Generating script...')
            sys.stdout = f
            generate_file_heap_use(args[2:])
            sys.stdout = original_stdout
            validate_script(args[1])
            print('Script successfully generated!')


if __name__ == '__main__':
    main()
