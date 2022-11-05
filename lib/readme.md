# The Library

In this directory you will find the core heap allocator logic written as if it were an allocator library. You will notice in reading the code that it focusses only on supporting and executing the algorithm for that allocator.

All low level setup for these allocators has been moved to the `utility/` folder so that it is easier to focus on how each allocator uses its data structure to manage the heap. If you would like to see the types and fundamental methods that help us create and navigate our nodes and blocks, please see the `utility/` folder.