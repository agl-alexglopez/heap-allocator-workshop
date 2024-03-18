add_library(debug_break INTERFACE debug_break.h)
add_library(print_utility INTERFACE print_utility.h)
add_library(segment segment.h segment.c)

macro(add_alloc ALLOCATOR_NAME)
  add_library(${ALLOCATOR_NAME} allocator.h ${ALLOCATOR_NAME}.c)
endmacro()

# Enter the name of the .c file in the lib/ folder without the .c.

add_alloc(libc_default)
add_alloc(list_segregated)
add_alloc(rbtree_clrs)
add_alloc(rbtree_unified)
add_alloc(rbtree_linked)
add_alloc(rbtree_stack)
add_alloc(rbtree_topdown)
add_alloc(splaytree_stack)
add_alloc(splaytree_topdown)

