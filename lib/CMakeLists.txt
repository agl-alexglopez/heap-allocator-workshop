
# These are initial flags that came with the project so I am keeping them. The most important is
# -g3 that gives us the maximum possible debug information for gdb debugger. Set the level of
# optimization for each allocator while debugging (-O0 or -Og) and then for efficiency testing
# (-O2 or -O3).
set(project_flags "-g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -fpie")

add_library(script script.h script.c)
add_library(segment segment.h segment.c)


############################# List Addressorder Allocator ########################################


add_library(list_addressorder allocator.h list_addressorder.c)
target_link_libraries(list_addressorder PUBLIC
    list_utility
)
target_include_directories(list_addressorder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(list_addressorder PROPERTIES
                      COMPILE_FLAGS
                      "-O3 ${project_flags}"
)


############################# List Bestfit Allocator ########################################


add_library(list_bestfit allocator.h list_bestfit.c)
target_link_libraries(list_bestfit PUBLIC
    list_utility
)
target_include_directories(list_bestfit PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(list_bestfit PROPERTIES
                      COMPILE_FLAGS
                      "-O3 ${project_flags}"
)


############################# List Segregated Allocator ########################################


add_library(list_segregated allocator.h list_segregated.c)
target_link_libraries(list_segregated PUBLIC
    list_utility
)
target_include_directories(list_segregated PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(list_segregated PROPERTIES
                      COMPILE_FLAGS
                      "-O3 ${project_flags}"
)


############################# RBTree CLRS Allocator ########################################


add_library(rbtree_clrs allocator.h rbtree_clrs.c)
target_link_libraries(rbtree_clrs PUBLIC
                      debug_break
)
target_include_directories(rbtree_clrs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(rbtree_clrs PROPERTIES
                      COMPILE_FLAGS
                      "-O3 ${project_flags}"
)


############################# RBTree Unified Allocator ########################################


add_library(rbtree_unified allocator.h rbtree_unified.c)
target_link_libraries(rbtree_unified PUBLIC
                      debug_break
)
target_include_directories(rbtree_unified PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(rbtree_unified PROPERTIES
                      COMPILE_FLAGS
                      "-O3 ${project_flags}"
)


############################# RBTree Linked Allocator ########################################


add_library(rbtree_linked allocator.h rbtree_linked.c)
target_link_libraries(rbtree_linked PUBLIC
                      debug_break
)
target_include_directories(rbtree_linked PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(rbtree_linked PROPERTIES
                      COMPILE_FLAGS
                      "-O3 ${project_flags}"
)


############################# RBTree Stack Allocator ########################################


add_library(rbtree_stack allocator.h rbtree_stack.c)
target_link_libraries(rbtree_stack PUBLIC
                      debug_break
)
target_include_directories(rbtree_stack PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(rbtree_stack PROPERTIES
                      COMPILE_FLAGS
                      "-O3 ${project_flags}"
)


############################# RBTree Topdown Allocator ########################################


add_library(rbtree_topdown allocator.h rbtree_topdown.c)
target_link_libraries(rbtree_topdown PUBLIC
                      debug_break
)
target_include_directories(rbtree_topdown PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(rbtree_topdown PROPERTIES
                      COMPILE_FLAGS
                      "-O3 ${project_flags}"
)

