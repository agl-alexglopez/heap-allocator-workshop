
################################# SANDBOX PROGRAM ############################################

# The allocator name is that of the allocator found in the lib folder/

macro(add_sandbox ALLOCATOR_NAME)
  add_executable(sandbox_${ALLOCATOR_NAME} sandbox.c)
  target_link_libraries(sandbox_${ALLOCATOR_NAME} PUBLIC
    segment
    script
    ${ALLOCATOR_NAME}
  )
endmacro()

add_sandbox(list_segregated)
add_sandbox(rbtree_clrs)
add_sandbox(rbtree_unified)
add_sandbox(rbtree_linked)
add_sandbox(rbtree_stack)
add_sandbox(rbtree_topdown)
add_sandbox(splaytree_stack)
add_sandbox(splaytree_topdown)
add_sandbox(libc_default)

