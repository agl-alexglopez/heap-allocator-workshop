
# These are initial flags that came with the project so I am keeping them. The most important is
# -g3 that gives us the maximum possible debug information for gdb debugger. Set the level of
# optimization for each allocator while debugging (-O0 or -Og) and then for efficiency testing
# (-O2 or -O3). Finally, I do not want to configure a Debug and Release package for this project.
# Instead just go the the target_compile_options() for each allocator and set the level of
# optimization required. Use -Og while debugging then -O2 or -O3 when it is working.
add_library(allocator_compiler_flags INTERFACE)
target_compile_features(allocator_compiler_flags INTERFACE c_std_99)
set(gcc_like_c "$<COMPILE_LANG_AND_ID:C,ARMClang,AppleClang,Clang,GNU>")
target_compile_options(allocator_compiler_flags INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-g3;-Wall;-Wextra;-Wfloat-equal;-Wtype-limits;-Wpointer-arith;-Wshadow;-Winit-self;-fno-diagnostics-show-option;-fcf-protection=none;-fno-pic;-fpie;-flto>>"
)


##########################  Script Parsing for Heap Execution  ####################################


add_library(script script.h script.c)
target_link_libraries(script PRIVATE
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(script BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)


#########################  Preparing Controlled Memory Segment  ###################################


add_library(segment segment.h segment.c)
target_link_libraries(segment PRIVATE
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(segment BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


############################# List Addressorder Allocator ########################################

add_library(list_addressorder_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/designs/list_addressorder_design.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/list_addressorder_tests.c
    ${CMAKE_CURRENT_SOURCE_DIR}/printers/list_addressorder_printer.c
)
target_link_libraries(list_addressorder_utilities PRIVATE
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_addressorder_utilities BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

add_library(list_addressorder allocator.h list_addressorder.c)
target_link_libraries(list_addressorder PRIVATE
    list_addressorder_utilities
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_addressorder BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


############################# List Bestfit Allocator ########################################


add_library(list_bestfit_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/designs/list_bestfit_design.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/list_bestfit_tests.c
    ${CMAKE_CURRENT_SOURCE_DIR}/printers/list_bestfit_printer.c
)
target_link_libraries(list_bestfit_utilities PRIVATE
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_bestfit_utilities BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

add_library(list_bestfit allocator.h list_bestfit.c)
target_link_libraries(list_bestfit PRIVATE
    list_bestfit_utilities
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_bestfit BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


############################# List Segregated Allocator ########################################

add_library(list_segregated_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/designs/list_segregated_design.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/list_segregated_tests.c
    ${CMAKE_CURRENT_SOURCE_DIR}/printers/list_segregated_printer.c
)
target_link_libraries(list_segregated_utilities PRIVATE
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_segregated_utilities BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

add_library(list_segregated allocator.h list_segregated.c)
target_link_libraries(list_segregated PRIVATE
    list_segregated_utilities
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_segregated BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

############################# RBTree CLRS Allocator ########################################


add_library(rbtree_clrs_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/designs/rbtree_clrs_design.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/rbtree_clrs_tests.c
    ${CMAKE_CURRENT_SOURCE_DIR}/printers/rbtree_clrs_printer.c
)
target_link_libraries(rbtree_clrs_utilities PRIVATE
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_clrs_utilities BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

add_library(rbtree_clrs allocator.h rbtree_clrs.c)
target_link_libraries(rbtree_clrs PRIVATE
    rbtree_clrs_utilities
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_clrs BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


############################# RBTree Unified Allocator ########################################


add_library(rbtree_unified_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/designs/rbtree_unified_design.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/rbtree_unified_tests.c
    ${CMAKE_CURRENT_SOURCE_DIR}/printers/rbtree_unified_printer.c
)
target_link_libraries(rbtree_unified_utilities PRIVATE
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_unified_utilities BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

add_library(rbtree_unified allocator.h rbtree_unified.c)
target_link_libraries(rbtree_unified PRIVATE
    rbtree_unified_utilities
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_unified BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


############################# RBTree Linked Allocator ########################################


add_library(rbtree_linked_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/designs/rbtree_linked_design.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/rbtree_linked_tests.c
    ${CMAKE_CURRENT_SOURCE_DIR}/printers/rbtree_linked_printer.c
)
target_link_libraries(rbtree_linked_utilities PRIVATE
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_linked_utilities BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

add_library(rbtree_linked allocator.h rbtree_linked.c)
target_link_libraries(rbtree_linked PRIVATE
    rbtree_linked_utilities
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_linked BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


############################# RBTree Stack Allocator ########################################


add_library(rbtree_stack_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/designs/rbtree_stack_design.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/rbtree_stack_tests.c
    ${CMAKE_CURRENT_SOURCE_DIR}/printers/rbtree_stack_printer.c
)
target_link_libraries(rbtree_stack_utilities PRIVATE
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_stack_utilities BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

add_library(rbtree_stack allocator.h rbtree_stack.c)
target_link_libraries(rbtree_stack PRIVATE
    rbtree_stack_utilities
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_stack BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


############################# RBTree Topdown Allocator ########################################

add_library(rbtree_topdown_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/designs/rbtree_topdown_design.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/rbtree_topdown_tests.c
    ${CMAKE_CURRENT_SOURCE_DIR}/printers/rbtree_topdown_printer.c
)
target_link_libraries(rbtree_topdown_utilities PRIVATE
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_topdown_utilities BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

add_library(rbtree_topdown allocator.h rbtree_topdown.c)
target_link_libraries(rbtree_topdown PRIVATE
    rbtree_topdown_utilities
    allocator_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_topdown BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

