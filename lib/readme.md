# The Library

In this directory you will find the core heap allocator logic written as if it were an allocator library. You will notice in reading the code that it focusses only on supporting and executing the algorithm for that allocator.

All low level setup for these allocators has been moved to the `[ALLOCATOR]_utilities.h` file so that it is easier to focus on how each allocator uses its data structure to manage the heap. If you would like to see the types and fundamental methods that help us create and navigate our nodes and blocks, please see the `[ALLOCATOR]_utilies.h` file.

As a design note, you may see similar code reproduced across implementations of the allocators. I thought about uniting these into one large utility library for all allocators, but that would present some challenges. Each allocator has slightly different types for their nodes. Yes, there are some cases in which the types are identical but the way headers store bytes might be different. For example, my list based allocators include their own headersize in the byte count while the Red Black Tree allocators do not. There are other small differences like this that make keeping every allocator encapsulated in its own logic simpler. This also has the added benefit of allowing me to change the design to try something new without worrying about breaking all allocators. I can also easily add new implementations with any design guidelines without needing to adhere to past design principles. As I learn more about allocators, this will make it easy to come back here and try new designs.

