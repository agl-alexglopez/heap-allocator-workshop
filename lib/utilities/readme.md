# Utilities

This folder contains the `extern` declarations for all functions I inlined in my `[ALLOCATOR]_utilities.h` file. In order to inline a function in a library that you intend for another translation unit to use, you need to declare the function in the `.c` file and define it in the `.h` file, which is somewhat counterintuitive.

