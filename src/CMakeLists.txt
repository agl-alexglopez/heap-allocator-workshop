
#################################    COMPILER FLAGS    ############################################

add_library(program_compiler_flags INTERFACE)
target_compile_features(program_compiler_flags INTERFACE c_std_99)
set(gcc_like_c "$<COMPILE_LANG_AND_ID:C,ARMClang,AppleClang,Clang,GNU>")
# We will use the -Og flag here until I learn more about how compilers optimize calls to the clock()
# utility in C. I would not want the compiler moving the code between calls to clock() that might
# interfere with consistent timing that is focussed on the allocator.
target_compile_options(program_compiler_flags INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-Og;-g3;-Wall;-Wextra;-Wfloat-equal;-Wtype-limits;-Wpointer-arith;-Wshadow;-Winit-self;-fno-diagnostics-show-option;-fcf-protection=none;-fno-pic;-fpie;>>"
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

