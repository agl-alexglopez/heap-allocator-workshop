
#################################    COMPILER FLAGS    ############################################

add_library(program_compiler_flags INTERFACE)
target_compile_features(program_compiler_flags INTERFACE c_std_99)
set(gcc_like_c "$<COMPILE_LANG_AND_ID:C,ARMClang,AppleClang,Clang,GNU>")
target_compile_options(program_compiler_flags INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-g3;-Wall;-Wextra;-Wfloat-equal;-Wtype-limits;-Wpointer-arith;-Wshadow;-Winit-self;-fno-diagnostics-show-option;-fcf-protection=none;-fno-pic;-fpie;>>"
)

set(libdir lib)


################################# TEST HARNESS PROGRAM ############################################


########## List Addressorder #############

add_executable(test_list_addressorder test_harness.c)
target_link_libraries(test_list_addressorder PUBLIC
                      segment
                      script
                      list_addressorder
                      program_compiler_flags
)
target_include_directories(test_list_addressorder PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(test_list_addressorder BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## List Bestfit #############

add_executable(test_list_bestfit test_harness.c)
target_link_libraries(test_list_bestfit PUBLIC
                      segment
                      script
                      list_bestfit
                      program_compiler_flags
)
target_include_directories(test_list_bestfit PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
# Change -OX(X meaning 2 or 3) to -Og if debugging is required.
target_compile_options(test_list_bestfit BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## List Segregated #############

add_executable(test_list_segregated test_harness.c)
target_link_libraries(test_list_segregated PUBLIC
                      segment
                      script
                      list_segregated
                      program_compiler_flags
)
target_include_directories(test_list_segregated PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(test_list_segregated BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## RBTree CLRS #############

add_executable(test_rbtree_clrs test_harness.c)
target_link_libraries(test_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
                      program_compiler_flags
)
target_include_directories(test_rbtree_clrs PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(test_rbtree_clrs BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## RBTree Unified #############

add_executable(test_rbtree_unified test_harness.c)
target_link_libraries(test_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
                      program_compiler_flags
)
target_include_directories(test_rbtree_unified PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(test_rbtree_unified BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## RBTree Linked #############

add_executable(test_rbtree_linked test_harness.c)
target_link_libraries(test_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
                      program_compiler_flags
)
target_include_directories(test_rbtree_linked PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(test_rbtree_linked BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## RBTree Stack #############

add_executable(test_rbtree_stack test_harness.c)
target_link_libraries(test_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
                      program_compiler_flags
)
target_include_directories(test_rbtree_stack PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(test_rbtree_stack BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## RBTree Topdown #############

add_executable(test_rbtree_topdown test_harness.c)
target_link_libraries(test_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
                      program_compiler_flags
)
target_include_directories(test_rbtree_topdown PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(test_rbtree_topdown BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)


################################# TIME HARNESS PROGRAM ############################################


########## List Addressorder #############

add_executable(time_list_addressorder time_harness.c)
target_link_libraries(time_list_addressorder PUBLIC
                      segment
                      script
                      list_addressorder
                      program_compiler_flags
)
target_include_directories(time_list_addressorder PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(time_list_addressorder BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## List Bestfit #############

add_executable(time_list_bestfit time_harness.c)
target_link_libraries(time_list_bestfit PUBLIC
                      segment
                      script
                      list_bestfit
                      program_compiler_flags
)
target_include_directories(time_list_bestfit PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(time_list_bestfit BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## List Segregated #############

add_executable(time_list_segregated time_harness.c)
target_link_libraries(time_list_segregated PUBLIC
                      segment
                      script
                      list_segregated
                      program_compiler_flags
)
target_include_directories(time_list_segregated PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(time_list_segregated BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

############ RBTree ClRS ###############

add_executable(time_rbtree_clrs time_harness.c)
target_link_libraries(time_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
                      program_compiler_flags
)
target_include_directories(time_rbtree_clrs PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(time_rbtree_clrs BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

############ RBTree Unified ###############

add_executable(time_rbtree_unified time_harness.c)
target_link_libraries(time_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
                      program_compiler_flags
)
target_include_directories(time_rbtree_unified PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(time_rbtree_unified BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

############ RBTree Linked ###############

add_executable(time_rbtree_linked time_harness.c)
target_link_libraries(time_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
                      program_compiler_flags
)
target_include_directories(time_rbtree_linked PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(time_rbtree_linked BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

############ RBTree Stack ###############

add_executable(time_rbtree_stack time_harness.c)
target_link_libraries(time_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
                      program_compiler_flags
)
target_include_directories(time_rbtree_stack PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(time_rbtree_stack BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

############ RBTree Topdown ###############

add_executable(time_rbtree_topdown time_harness.c)
target_link_libraries(time_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
                      program_compiler_flags
)
target_include_directories(time_rbtree_topdown PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(time_rbtree_topdown BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

################################# PRINTER PROGRAM ############################################


########## List Addressorder #############

add_executable(print_peaks_list_addressorder print_peaks.c)
target_link_libraries(print_peaks_list_addressorder PUBLIC
                      segment
                      script
                      list_addressorder
                      program_compiler_flags
)
target_include_directories(print_peaks_list_addressorder PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(print_peaks_list_addressorder BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## List Bestfit #############

add_executable(print_peaks_list_bestfit print_peaks.c)
target_link_libraries(print_peaks_list_bestfit PUBLIC
                      segment
                      script
                      list_bestfit
                      program_compiler_flags
)
target_include_directories(print_peaks_list_bestfit PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(print_peaks_list_bestfit BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

####### List Segregated ############

add_executable(print_peaks_list_segregated print_peaks.c)
target_link_libraries(print_peaks_list_segregated PUBLIC
                      segment
                      script
                      list_segregated
                      program_compiler_flags
)
target_include_directories(print_peaks_list_segregated PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(print_peaks_list_segregated BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

######## RBTree CLRS ###########

add_executable(print_peaks_rbtree_clrs print_peaks.c)
target_link_libraries(print_peaks_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
                      program_compiler_flags
)
target_include_directories(print_peaks_rbtree_clrs PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(print_peaks_rbtree_clrs BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

######## RBTree Unified ###########

add_executable(print_peaks_rbtree_unified print_peaks.c)
target_link_libraries(print_peaks_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
                      program_compiler_flags
)
target_include_directories(print_peaks_rbtree_unified PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(print_peaks_rbtree_unified BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

######## RBTree Linked ###########

add_executable(print_peaks_rbtree_linked print_peaks.c)
target_link_libraries(print_peaks_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
                      program_compiler_flags
)
target_include_directories(print_peaks_rbtree_linked PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(print_peaks_rbtree_linked BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

######## RBTree Stack ###########

add_executable(print_peaks_rbtree_stack print_peaks.c)
target_link_libraries(print_peaks_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
                      program_compiler_flags
)
target_include_directories(print_peaks_rbtree_stack PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(print_peaks_rbtree_stack BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

######## RBTree Topdown ###########

add_executable(print_peaks_rbtree_topdown print_peaks.c)
target_link_libraries(print_peaks_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
                      program_compiler_flags
)
target_include_directories(print_peaks_rbtree_topdown PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(print_peaks_rbtree_topdown BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

################################# OPTIONAL PROGRAM ############################################


########## List Addressorder #############

add_executable(my_optional_program_list_addressorder my_optional_program.c)
target_link_libraries(my_optional_program_list_addressorder PUBLIC
                      segment
                      script
                      list_addressorder
                      program_compiler_flags
)
target_include_directories(my_optional_program_list_addressorder PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(my_optional_program_list_addressorder BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

########## List Bestfit #############

add_executable(my_optional_program_list_bestfit my_optional_program.c)
target_link_libraries(my_optional_program_list_bestfit PUBLIC
                      segment
                      script
                      list_bestfit
                      program_compiler_flags
)
target_include_directories(my_optional_program_list_bestfit PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(my_optional_program_list_bestfit BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

####### List Segregated ############

add_executable(my_optional_program_list_segregated my_optional_program.c)
target_link_libraries(my_optional_program_list_segregated PUBLIC
                      segment
                      script
                      list_segregated
                      program_compiler_flags
)
target_include_directories(my_optional_program_list_segregated PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(my_optional_program_list_segregated BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

######## RBTree CLRS ###########

add_executable(my_optional_program_rbtree_clrs my_optional_program.c)
target_link_libraries(my_optional_program_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
                      program_compiler_flags
)
target_include_directories(my_optional_program_rbtree_clrs PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(my_optional_program_rbtree_clrs BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

######## RBTree Unified ###########

add_executable(my_optional_program_rbtree_unified my_optional_program.c)
target_link_libraries(my_optional_program_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
                      program_compiler_flags
)
target_include_directories(my_optional_program_rbtree_unified PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(my_optional_program_rbtree_unified BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

######## RBTree Linked ###########

add_executable(my_optional_program_rbtree_linked my_optional_program.c)
target_link_libraries(my_optional_program_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
                      program_compiler_flags
)
target_include_directories(my_optional_program_rbtree_linked PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(my_optional_program_rbtree_linked BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

#######  RBTree Stack ##########

add_executable(my_optional_program_rbtree_stack my_optional_program.c)
target_link_libraries(my_optional_program_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
                      program_compiler_flags
)
target_include_directories(my_optional_program_rbtree_stack PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(my_optional_program_rbtree_stack BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

#######  RBTree Topdown ##########

add_executable(my_optional_program_rbtree_topdown my_optional_program.c)
target_link_libraries(my_optional_program_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
                      program_compiler_flags
)
target_include_directories(my_optional_program_rbtree_topdown PUBLIC
                           ${PROJECT_SOURCE_DIR}/${libdir}
)
target_compile_options(my_optional_program_rbtree_topdown BEFORE INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og>>"
)

