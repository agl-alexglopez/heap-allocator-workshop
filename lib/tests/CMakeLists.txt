
add_library(test_compiler_flags INTERFACE)
target_compile_features(test_compiler_flags INTERFACE c_std_99)
set(gcc_like_c "$<COMPILE_LANG_AND_ID:C,ARMClang,AppleClang,Clang,GNU>")
target_compile_options(test_compiler_flags INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-g3;-Wall;-Wextra;-Wfloat-equal;-Wtype-limits;-Wpointer-arith;-Wshadow;-Winit-self;-fno-diagnostics-show-option;-fcf-protection=none;-fno-pic;-fpie>>"
)

add_library(debug_break debug_break.h debug_break.c)


#############################   List Addressorder Tests    ########################################


add_library(list_addressorder_tests list_addressorder_tests.h list_addressorder_tests.c)
target_link_libraries(list_addressorder_tests PUBLIC
    list_addressorder_design
    debug_break
    test_compiler_flags
)
target_include_directories(list_addressorder_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_addressorder_tests BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################      List Bestfit Tests      ########################################


add_library(list_bestfit_tests list_bestfit_tests.h list_bestfit_tests.c)
target_link_libraries(list_bestfit_tests PUBLIC
    list_bestfit_design
    debug_break
    test_compiler_flags
)
target_include_directories(list_bestfit_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_bestfit_tests BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################    List Segregated Tests     ########################################


add_library(list_segregated_tests list_segregated_tests.h list_segregated_tests.c)
target_link_libraries(list_segregated_tests PUBLIC
    list_segregated_design
    debug_break
    test_compiler_flags
)
target_include_directories(list_segregated_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_segregated_tests BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################     RBTree CLRS Tests        ########################################


add_library(rbtree_clrs_tests rbtree_clrs_tests.h rbtree_clrs_tests.c)
target_link_libraries(rbtree_clrs_tests PUBLIC
    rbtree_clrs_design
    debug_break
    test_compiler_flags
)
target_include_directories(rbtree_clrs_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_clrs_tests BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################     RBTree Unified Tests      ########################################


add_library(rbtree_unified_tests rbtree_unified_tests.h rbtree_unified_tests.c)
target_link_libraries(rbtree_unified_tests PUBLIC
    rbtree_unified_design
    debug_break
    test_compiler_flags
)
target_include_directories(rbtree_unified_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_unified_tests BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################     RBTree Linked Tests      ########################################


add_library(rbtree_linked_tests rbtree_linked_tests.h rbtree_linked_tests.c)
target_link_libraries(rbtree_linked_tests PUBLIC
    rbtree_linked_design
    debug_break
    test_compiler_flags
)
target_include_directories(rbtree_linked_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_linked_tests BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################      RBTree Stack Tests      ########################################


add_library(rbtree_stack_tests rbtree_stack_tests.h rbtree_stack_tests.c)
target_link_libraries(rbtree_stack_tests PUBLIC
    rbtree_stack_design
    debug_break
    test_compiler_flags
)
target_include_directories(rbtree_stack_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_stack_tests BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################    RBTree Topdown Tests      ########################################


add_library(rbtree_topdown_tests rbtree_topdown_tests.h rbtree_topdown_tests.c)
target_link_libraries(rbtree_topdown_tests PUBLIC
    rbtree_topdown_design
    debug_break
    test_compiler_flags
)
target_include_directories(rbtree_topdown_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_topdown_tests BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

