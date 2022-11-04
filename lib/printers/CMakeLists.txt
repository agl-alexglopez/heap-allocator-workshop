
add_library(printer_compiler_flags INTERFACE)
target_compile_features(printer_compiler_flags INTERFACE c_std_99)
set(gcc_like_c "$<COMPILE_LANG_AND_ID:C,ARMClang,AppleClang,Clang,GNU>")
target_compile_options(printer_compiler_flags INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-g3;-Wall;-Wextra;-Wfloat-equal;-Wtype-limits;-Wpointer-arith;-Wshadow;-Winit-self;-fno-diagnostics-show-option;-fcf-protection=none;-fno-pic;-fpie>>"
)


add_library(print_utility print_utility.h print_utility.c)


#############################   List Addressorder Printer  ########################################


add_library(list_addressorder_printer list_addressorder_printer.h list_addressorder_printer.c)
target_link_libraries(list_addressorder_printer PUBLIC
    list_addressorder_design
    print_utility
    printer_compiler_flags
)
target_include_directories(list_addressorder_printer PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_addressorder_printer BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################     List Bestfit Printer     ########################################


add_library(list_bestfit_printer list_bestfit_printer.h list_bestfit_printer.c)
target_link_libraries(list_bestfit_printer PUBLIC
    list_bestfit_design
    print_utility
    printer_compiler_flags
)
target_include_directories(list_bestfit_printer PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_bestfit_printer BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################   List Segregated Printer    ########################################


add_library(list_segregated_printer list_segregated_printer.h list_segregated_printer.c)
target_link_libraries(list_segregated_printer PUBLIC
    list_segregated_design
    print_utility
    printer_compiler_flags
)
target_include_directories(list_segregated_printer PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(list_segregated_printer BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################     RBTree CLRS Printer      ########################################


add_library(rbtree_clrs_printer rbtree_clrs_printer.h rbtree_clrs_printer.c)
target_link_libraries(rbtree_clrs_printer PUBLIC
    rbtree_clrs_design
    print_utility
    printer_compiler_flags
)
target_include_directories(rbtree_clrs_printer PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_clrs_printer BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################    RBTree Unified Printer    ########################################


add_library(rbtree_unified_printer rbtree_unified_printer.h rbtree_unified_printer.c)
target_link_libraries(rbtree_unified_printer PUBLIC
    rbtree_unified_design
    print_utility
    printer_compiler_flags
)
target_include_directories(rbtree_unified_printer PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_unified_printer BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################    RBTree Linked Printer     ########################################


add_library(rbtree_linked_printer rbtree_linked_printer.h rbtree_linked_printer.c)
target_link_libraries(rbtree_linked_printer PUBLIC
    rbtree_linked_design
    print_utility
    printer_compiler_flags
)
target_include_directories(rbtree_linked_printer PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_linked_printer BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################    RBTree Stack Printer      ########################################


add_library(rbtree_stack_printer rbtree_stack_printer.h rbtree_stack_printer.c)
target_link_libraries(rbtree_stack_printer PUBLIC
    rbtree_stack_design
    print_utility
    printer_compiler_flags
)
target_include_directories(rbtree_stack_printer PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_stack_printer BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)


#############################    RBTree Topdown Printer    ########################################


add_library(rbtree_topdown_printer rbtree_topdown_printer.h rbtree_topdown_printer.c)
target_link_libraries(rbtree_topdown_printer PUBLIC
    rbtree_topdown_design
    print_utility
    printer_compiler_flags
)
target_include_directories(rbtree_topdown_printer PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/designs
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(rbtree_topdown_printer BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3>>"
)

