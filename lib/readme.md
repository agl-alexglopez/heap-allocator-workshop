# The Library

In this directory you will find the core heap allocator logic written as if it were an allocator library.

As a design note, you may see similar code reproduced across implementations of the allocators. I thought about uniting these into one large utility library for all allocators, but that would present some challenges. Each allocator has slightly different types for their nodes. Yes, there are some cases in which the types are identical but the way headers store bytes might be different. For example, my list based allocators include their own headersize in the byte count while the Red Black Tree allocators do not. There are other small differences like this that make keeping every allocator encapsulated in its own logic simpler. This also has the added benefit of allowing me to change the design to try something new without worrying about breaking all allocators. I can also easily add new implementations with any design guidelines without needing to adhere to past design principles. As I learn more about allocators, this will make it easy to come back here and try new designs.

