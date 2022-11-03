
# These are initial flags that came with the project so I am keeping them. The most important is
# -g3 that gives us the maximum possible debug information for gdb debugger. Set the level of
# optimization for each allocator while debugging (-O0 or -Og) and then for efficiency testing
# (-O2 or -O3).
# set(project_flags "-g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -fpie")

add_library(allocator_compiler_flags INTERFACE)
target_compile_features(allocator_compiler_flags INTERFACE c_std_99)
set(gcc_like_c "$<COMPILE_LANG_AND_ID:C,ARMClang,AppleClang,Clang,GNU>")
target_compile_options(allocator_compiler_flags INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3;-Wall;-Wextra;-Wfloat-equal;-Wtype-limits;-Wpointer-arith;-Wshadow;-Winit-self;-fno-diagnostics-show-option;-fcf-protection=none;-fno-pic;-fpie>>"
)


add_library(script script.h script.c)
add_library(segment segment.h segment.c)


############################# List Addressorder Allocator ########################################


add_library(list_addressorder allocator.h list_addressorder.c)
target_link_libraries(list_addressorder PUBLIC
    list_addressorder_design
    list_addressorder_tests
    list_addressorder_printer
    allocator_compiler_flags
)
target_include_directories(list_addressorder PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)


############################# List Bestfit Allocator ########################################


add_library(list_bestfit allocator.h list_bestfit.c)
target_link_libraries(list_bestfit PUBLIC
    list_bestfit_design
    list_bestfit_tests
    list_bestfit_printer
    allocator_compiler_flags
)
target_include_directories(list_bestfit PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)


############################# List Segregated Allocator ########################################


add_library(list_segregated allocator.h list_segregated.c)
target_link_libraries(list_segregated PUBLIC
    list_segregated_design
    list_segregated_tests
    list_segregated_printer
    allocator_compiler_flags
)
target_include_directories(list_segregated PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)


############################# RBTree CLRS Allocator ########################################


add_library(rbtree_clrs allocator.h rbtree_clrs.c)
target_link_libraries(rbtree_clrs PUBLIC
    rbtree_clrs_design
    rbtree_clrs_tests
    rbtree_clrs_printer
    allocator_compiler_flags
)
target_include_directories(rbtree_clrs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)


############################# RBTree Unified Allocator ########################################


add_library(rbtree_unified allocator.h rbtree_unified.c)
target_link_libraries(rbtree_unified PUBLIC
    rbtree_unified_design
    rbtree_unified_tests
    rbtree_unified_printer
    allocator_compiler_flags
)
target_include_directories(rbtree_unified PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)


############################# RBTree Linked Allocator ########################################


add_library(rbtree_linked allocator.h rbtree_linked.c)
target_link_libraries(rbtree_linked PUBLIC
    rbtree_linked_design
    rbtree_linked_tests
    rbtree_linked_printer
    allocator_compiler_flags
)
target_include_directories(rbtree_linked PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(rbtree_linked PROPERTIES
                      COMPILE_FLAGS
                      "-O3"
)


############################# RBTree Stack Allocator ########################################


add_library(rbtree_stack allocator.h rbtree_stack.c)
target_link_libraries(rbtree_stack PUBLIC
    rbtree_stack_design
    rbtree_stack_tests
    rbtree_stack_printer
    allocator_compiler_flags
)
target_include_directories(rbtree_stack PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(rbtree_stack PROPERTIES
                      COMPILE_FLAGS
                      "-O3"
)


############################# RBTree Topdown Allocator ########################################


add_library(rbtree_topdown allocator.h rbtree_topdown.c)
target_link_libraries(rbtree_topdown PUBLIC
    rbtree_topdown_design
    rbtree_topdown_tests
    rbtree_topdown_printer
    allocator_compiler_flags
)
target_include_directories(rbtree_topdown PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)
set_target_properties(rbtree_topdown PROPERTIES
                      COMPILE_FLAGS
                      "-O3"
)

