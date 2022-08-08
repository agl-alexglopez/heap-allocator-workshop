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
    >>> get_heap_address('gcc -g3 -O0 -std=gnu99 -Wall $warnflags  triangle.c  -o triangle')
    >>> get_heap_address('+++ exited (status 0) +++')
    >>> get_heap_address('---SIGCHLD (Child exited)---')
    """
    # The arrow prepends any malloc, calloc, realloc, or free.
    call_index = line.find('->')
    if call_index == -1:
        return None
    equals = line.find('=', call_index)
    first_half = line[:equals]
    second_half = line[equals + 1:]
    # Free only has the hex id in the first half in the free() call.
    if second_half == '<void>':
        open_bracket = first_half.find('free(', call_index + 2)
        i = open_bracket + 5
        while i < len(first_half) and first_half[i] != ')':
            i += 1
        address = first_half[open_bracket + 5: i]
        return int(address, 16)
    return int(second_half, 16)


def get_heap_call(line):
    """
    >>> get_heap_call('make->calloc(8192, 1)=0x56167a22c440')
    <Heap_Call.alloc: 1>
    >>> get_heap_call('make->malloc(8192)=0x56167a22c440')
    <Heap_Call.alloc: 1>
    >>> get_heap_call('make->free(0x56167a22c440)=<void>')
    <Heap_Call.free: 3>
    >>> get_heap_call('make->realloc(0x56167a22c440, 8192)=0x56167a22c440')
    <Heap_Call.realloc: 2>
    >>> get_heap_address('gcc -g3 -O0 -std=gnu99 -Wall $warnflags  triangle.c  -o triangle')
    >>> get_heap_address('+++ exited (status 0) +++')
    >>> get_heap_address('---SIGCHLD (Child exited)---')
    """
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
    >>> get_heap_bytes('make->calloc(8192,1)=0x56167a22c440')
    '8192'
    >>> get_heap_bytes('make->calloc(8192,4)=0x56167a22c440')
    '32768'
    >>> get_heap_bytes('make->malloc(8192)=0x56167a22c440')
    '8192'
    >>> get_heap_bytes('make->free(0x56167a22c440)=<void>')
    >>> get_heap_bytes('make->realloc(0x56167a22c440,8192)=0x56167a22c440')
    '8192'
    >>> get_heap_bytes('gcc-g3-O0-std=gnu99-Wall$warnflagstriangle.c-otriangle')
    >>> get_heap_bytes('+++exited(status 0)+++')
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
        full_bytes = str(int(number_of_items) * int(byte_size))
        return full_bytes
    elif specific_call == Heap_Strings.malloc:
        closing_bracket = line.find(')', open_bracket)
        full_bytes = line[open_bracket + 1: closing_bracket]
        return full_bytes
    elif specific_call == Heap_Strings.realloc:
        comma = line.find(',', open_bracket)
        closing_bracket = line.find(')', comma)
        full_bytes = line[comma + 1: closing_bracket]
        return full_bytes
    return None


def add_line(call_type, memory_id, total_bytes):
    """
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
                    id_list = memory_dict[heap_address]
                    to_del = id_list[len(id_list) - 1]
                    print(add_line(Heap_Call.free, to_del, 0))
                    memory_dict.pop(heap_address)
                elif call == Heap_Call.realloc:
                    # We have multiple malloc callocs under same id so just choose one at random to realloc.
                    memory_list = memory_dict[heap_address]
                    to_realloc = random.choice(memory_list)
                    print(add_line(Heap_Call.realloc, to_realloc, get_heap_bytes(line)))
                elif call == Heap_Call.alloc:
                    id_list = memory_dict[heap_address]
                    id_list.append(memory_ids)
                    print(add_line(Heap_Call.alloc, memory_ids, get_heap_bytes(line)))
                    memory_ids += 1
            # If the call is new
            elif heap_address:
                # If the call is free and address not in dict, discard
                call = get_heap_call(line)
                # If the call is realloc
                if call == Heap_Call.realloc:
                    memory_dict[heap_address] = [memory_ids]
                    print(add_line(Heap_Call.alloc, memory_ids, 8))
                    print(add_line(Heap_Call.realloc, memory_ids, get_heap_bytes(line)))
                    memory_ids += 1
                    # If the address is not in the dict, make a malloc then realloc call.
                elif call == Heap_Call.alloc:
                    memory_dict[heap_address] = [memory_ids]
                    print(add_line(Heap_Call.alloc, memory_ids, get_heap_bytes(line)))
                    memory_ids += 1
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
        return random.randint(8, 50), random.randint(200, 1200)
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
    >>> identify_num_requests('-free')
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
                num_requests = identify_num_requests(arg_string[1:])
                if num_requests == ALL_IDS:
                    num_requests = len(id_byte_map)
                for i in range(free_ids, num_requests):
                    id_byte_map.pop(free_ids)
                    print(f'{Print_Call.free} {free_ids}')
                    free_ids += 2
            elif arg_call == Print_Call.alloc:
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
            elif arg_call == Print_Call.realloc:
                byte_tuple = identify_byte_range(arg_string[1:])
                num_requests = identify_num_requests(arg_string[1:])
                if num_requests == ALL_IDS:
                    num_requests = len(id_byte_map)
                for i in range(num_requests):
                    # You cannot reallocate a block that is not allocated.
                    if realloc_ids in id_byte_map:
                        if byte_tuple[0] and byte_tuple[1]:
                            print(f'{Print_Call.realloc} {realloc_ids} {random.randint(byte_tuple[0], byte_tuple[1])}')
                        else:
                            print(f'{Print_Call.realloc} {realloc_ids} {byte_tuple[0]}')
                    # If we just want to have a certain number of reallocs as a test they will just wrap.
                    realloc_ids = (realloc_ids + 1) % len(id_byte_map)
                    while realloc_ids not in id_byte_map:
                        realloc_ids = (realloc_ids + 1) % len(id_byte_map)
            # Heap calls will be left as is and we will not automatically free all allocated memory.
            elif arg_call == Print_Call.leak:
                return
    for idx, item in enumerate(id_byte_map):
        if idx == len(id_byte_map) - 1:
            print(f'{Print_Call.free} {item}', end='')
        else:
            print(f'{Print_Call.free} {item}')


def validate_script(file, leak=False):
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

