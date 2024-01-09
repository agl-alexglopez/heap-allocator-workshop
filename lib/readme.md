# The Allocators

This folder contains all of the implementations of the heap allocators. If you wish to implement your own allocator follow this design document and build instructions.

## Building an Allocator

The first step to building a heap allocator is to create your `.c` file. Give the file a two part name that highlights some key implementation traits that make it unique among the rest. For example, I have many implementations of a Red Black tree allocator in this repository and I chose to distinguish them by their names; the `rbtree_stack.c` file implements a Red Black Tree with a stack. Or, the `splaytree_topdown.c` implements a splaytree with topdown fixups.

Next, add the file and function stubs to your `.c` file. Here is a starting template you can use for your allocator.

```c
#include "allocator.h"
#include "debug_break.h"
#include "print_utility.h"

#include <stdbool.h>
#include <stddef.h>

bool winit( void *heap_start, size_t heap_size )
{
    (void)heap_start;
    (void)heap_size;
    UNIMPLEMENTED();
}

void *wmalloc( size_t requested_size )
{
    (void)requested_size;
    UNIMPLEMENTED();
}

void *wrealloc( void *old_ptr, size_t new_size )
{
    (void)old_ptr;
    (void)new_size;
    UNIMPLEMENTED();
}

void wfree( void *ptr )
{
    (void)ptr;
    UNIMPLEMENTED();
}

bool wvalidate_heap( void )
{
    UNIMPLEMENTED();
}

size_t wheap_align( size_t request )
{
    (void)request;
    UNIMPLEMENTED();
}

size_t wheap_capacity( void )
{
    UNIMPLEMENTED();
}

void wheap_diff( const struct heap_block expected[], struct heap_block actual[], size_t len )
{
    (void)expected;
    (void)actual;
    (void)len;
    UNIMPLEMENTED();
}
size_t wget_free_total( void )
{
    UNIMPLEMENTED();
}

void wprint_free_nodes( print_style style )
{
    (void)style;
    UNIMPLEMENTED();
}

void wdump_heap( void )
{
    UNIMPLEMENTED();
}
```

Next, add the allocator to the CMake files where they are needed as libraries as you named it.

The `lib/CMakeLists.txt` file.

```cmake
############################# My Heap Allocator ########################################

add_library(allocator_name allocator.h allocator_name.c)
```

While testing add your allocator to the test files.

The `tests/CMakeLists.txt` file.

```cmake
###############    Correctness Tester    ##################

##########   My Allocator    #############

add_executable(ctest_allocator_name ctest.cc)
target_link_libraries(ctest_allocator_name PUBLIC
  segment
  allocator_name
)

###############   Generic Tests for all Allocators    ############

##########   My Allocator    #############

add_executable(gtest_allocator_name gtest.cc)
target_link_libraries(gtest_allocator_name
  GTest::gtest_main
  segment
  allocator_name
)
gtest_discover_tests(gtest_allocator_name)
```

Finally, once you know your implementation is correct and passes all tests and scripts, add your allocator to any other programs present in the repository such as `print_peaks` or otherwise. When in doubt follow the configuration of the other allocators present. Be sure to delete the build folder and reconfigure/compile the project when changing `CMakeLists.txt` files. As a final touch consider following the established `.clang-tidy` and `.clang-format` checks. To do this run the following.

Clang format will format your code to follow the style guidelines of this repository.

```bash
cmake --build build --target format
# or
make format
```

Clang tidy will run **extensive** checks on your code for easy to catch logical and stylistic bugs. It is pretty pedantic, but I like it. Do as you will. As a special note for Clang tidy, I have not had success running it with the ninja generator. In other words, if I want to run clang tidy, I run it with my GCC/Unix Makefiles preset. You could also use the Unix Makefile build generator if you prefer compiling with clang. Simply change the `CMakeUserPreset.json` file to use the tools you like.

```bash
cmake --build build --target tidy
# or
make tidy
```

The `UNIMPLEMENTED();` macro allows you to compile the code and even run it, and it will abort the program with an error telling you which function you need to implement to continue. See the header file `allocator.h` for the general requirements of the functions. Here are some internal specifications that must be met and are enforced by the unit tests.

- Roundup/Align all headers and blocks of memory to a multiple of the alignment constant specified in the `allocator.h` file.
- Coalesce both left and right. Coalescing should occur whenever possible. This means that upon any call to `free` or `realloc` we should check and coalesce other free nodes to our left and right. This creates the invariant that no two free nodes should be next to one another.
- Blocks that are too large for a given request should be split and the remaining free space should be inserted back into the free data structure. This is obvious at the beggining when you only have one large pool of memory, but remember to choose some splitting metrics when things get more subtle.

Those are all the internal details that cannot be specified in the `allocator.h` file. Read the header file for more general requirements for the remaining functions. The rest is up to the implementer. Do not modularize the code into multiple header files. Prefer a monolithic `.c` file with all types and helper functions included and use `static` helper functions. The reason for this is to aid in creative freedom for implementations. In reading the code already here, you may see some repetition of logic, data structures, or implementation across the allocator files. While we could try to abstract out some clever shared header files and helper logic I recommend against it because the differences in implementation details can be subtle. This would also force any new implementation to adhere to some shared header file and avoid conflicts with types or functions already in use by the other allocators. Instead, everything should be encapsulated to one `.c` file. Allocators are not that lengthy to implement anyway. Exclude the validation and printers and there are not that many lines of code for an allocator and one `.c` file is manageable. This also allows complete creative freedom and no breaking changes if you wish to change internal implementation details later. Lastly, this aids in compilation time. I tried a more modular approach in the past and due to the sheer number of allocators and techniques the compilation times suffered. One file is good for now.

Have fun!
