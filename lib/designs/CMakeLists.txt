
add_library(design_compiler_flags INTERFACE)
target_compile_features(design_compiler_flags INTERFACE c_std_99)
set(gcc_like_c "$<COMPILE_LANG_AND_ID:C,ARMClang,AppleClang,Clang,GNU>")
target_compile_options(design_compiler_flags INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-g3;-Wall;-Wextra;-Wfloat-equal;-Wtype-limits;-Wpointer-arith;-Wshadow;-Winit-self;-fno-diagnostics-show-option;-fcf-protection=none;-fno-pic;-fpie>>"
)


##############################  List Addressorder Design   ########################################


add_library(list_addressorder_design list_addressorder_design.h list_addressorder_design.c)
target_link_libraries(list_addressorder_design PUBLIC
    design_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_addressorder_design BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


##############################  List Bestfit Design   ########################################


add_library(list_bestfit_design list_bestfit_design.h list_bestfit_design.c)
target_link_libraries(list_bestfit_design PUBLIC
    design_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_bestfit_design BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


##############################  List Segregated Design   ########################################


add_library(list_segregated_design list_segregated_design.h list_segregated_design.c)
target_link_libraries(list_segregated_design PUBLIC
    design_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_segregated_design BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


##############################     RBTree CLRS Design      ########################################


add_library(rbtree_clrs_design rbtree_clrs_design.h rbtree_clrs_design.c)
target_link_libraries(rbtree_clrs_design PUBLIC
    design_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_clrs_design BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


##############################    RBTree Unified Design    ########################################


add_library(rbtree_unified_design rbtree_unified_design.h rbtree_unified_design.c)
target_link_libraries(rbtree_unified_design PUBLIC
    design_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_unified_design BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


##############################    RBTree Linked Design    ########################################


add_library(rbtree_linked_design rbtree_linked_design.h rbtree_linked_design.c)
target_link_libraries(rbtree_linked_design PUBLIC
    design_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_linked_design BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


##############################    RBTree Stack Design    ########################################


add_library(rbtree_stack_design rbtree_stack_design.h rbtree_stack_design.c)
target_link_libraries(rbtree_stack_design PUBLIC
    design_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_stack_design BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


##############################    RBTree Topdown Design    ########################################


add_library(rbtree_topdown_design rbtree_topdown_design.h rbtree_topdown_design.c)
target_link_libraries(rbtree_topdown_design PUBLIC
    design_compiler_flags
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_topdown_design BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

