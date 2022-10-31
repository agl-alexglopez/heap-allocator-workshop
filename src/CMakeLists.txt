
set(lib-dir lib)


################################# TEST HARNESS PROGRAM ############################################


add_executable(test_list_addressorder test_harness.c)
set_target_properties(test_list_addressorder PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(test_list_addressorder PUBLIC
                      segment
                      script
                      list_addressorder
)
target_include_directories(test_list_addressorder PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(test_list_bestfit test_harness.c)
set_target_properties(test_list_bestfit PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(test_list_bestfit PUBLIC
                      segment
                      script
                      list_bestfit
)
target_include_directories(test_list_bestfit PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(test_list_segregated test_harness.c)
set_target_properties(test_list_segregated PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(test_list_segregated PUBLIC
                      segment
                      script
                      list_bestfit
)
target_include_directories(test_list_segregated PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(test_rbtree_clrs test_harness.c)
set_target_properties(test_rbtree_clrs PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(test_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
)
target_include_directories(test_rbtree_clrs PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(test_rbtree_unified test_harness.c)
set_target_properties(test_rbtree_unified PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(test_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
)
target_include_directories(test_rbtree_unified PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(test_rbtree_linked test_harness.c)
set_target_properties(test_rbtree_linked PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(test_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
)
target_include_directories(test_rbtree_linked PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(test_rbtree_stack test_harness.c)
set_target_properties(test_rbtree_stack PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(test_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
)
target_include_directories(test_rbtree_stack PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(test_rbtree_topdown test_harness.c)
set_target_properties(test_rbtree_topdown PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(test_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
)
target_include_directories(test_rbtree_topdown PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)


################################# TIME HARNESS PROGRAM ############################################


add_executable(time_list_addressorder time_harness.c)
target_link_libraries(time_list_addressorder PUBLIC
                      segment
                      script
                      list_addressorder
)
set_target_properties(time_list_addressorder PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_include_directories(time_list_addressorder PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(time_list_bestfit time_harness.c)
set_target_properties(time_list_bestfit PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(time_list_bestfit PUBLIC
                      segment
                      script
                      list_bestfit
)
target_include_directories(time_list_bestfit PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(time_list_segregated time_harness.c)
set_target_properties(time_list_segregated PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(time_list_segregated PUBLIC
                      segment
                      script
                      list_bestfit
)
target_include_directories(time_list_segregated PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(time_rbtree_clrs time_harness.c)
set_target_properties(time_rbtree_clrs PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(time_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
)
target_include_directories(time_rbtree_clrs PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(time_rbtree_unified time_harness.c)
set_target_properties(time_rbtree_unified PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(time_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
)
target_include_directories(time_rbtree_unified PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(time_rbtree_linked time_harness.c)
set_target_properties(time_rbtree_linked PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(time_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
)
target_include_directories(time_rbtree_linked PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(time_rbtree_stack time_harness.c)
set_target_properties(time_rbtree_stack PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(time_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
)
target_include_directories(time_rbtree_stack PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(time_rbtree_topdown time_harness.c)
set_target_properties(time_rbtree_topdown PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(time_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
)
target_include_directories(time_rbtree_topdown PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)


################################# PRINTER PROGRAM ############################################


add_executable(print_peaks_list_addressorder print_peaks.c)
set_target_properties(print_peaks_list_addressorder PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(print_peaks_list_addressorder PUBLIC
                      segment
                      script
                      list_addressorder
)
target_include_directories(print_peaks_list_addressorder PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(print_peaks_list_bestfit print_peaks.c)
set_target_properties(print_peaks_list_bestfit PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(print_peaks_list_bestfit PUBLIC
                      segment
                      script
                      list_bestfit
)
target_include_directories(print_peaks_list_bestfit PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(print_peaks_list_segregated print_peaks.c)
set_target_properties(print_peaks_list_segregated PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(print_peaks_list_segregated PUBLIC
                      segment
                      script
                      list_bestfit
)
target_include_directories(print_peaks_list_segregated PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(print_peaks_rbtree_clrs print_peaks.c)
set_target_properties(print_peaks_rbtree_clrs PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(print_peaks_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
)
target_include_directories(print_peaks_rbtree_clrs PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(print_peaks_rbtree_unified print_peaks.c)
set_target_properties(print_peaks_rbtree_unified PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(print_peaks_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
)
target_include_directories(print_peaks_rbtree_unified PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(print_peaks_rbtree_linked print_peaks.c)
set_target_properties(print_peaks_rbtree_linked PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(print_peaks_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
)
target_include_directories(print_peaks_rbtree_linked PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(print_peaks_rbtree_stack print_peaks.c)
set_target_properties(print_peaks_rbtree_stack PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(print_peaks_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
)
target_include_directories(print_peaks_rbtree_stack PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

add_executable(print_peaks_rbtree_topdown print_peaks.c)
set_target_properties(print_peaks_rbtree_topdown PROPERTIES
                      COMPILE_FLAGS
                      "-g -O0 -Wall -Wextra"
)
target_link_libraries(print_peaks_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
)
target_include_directories(print_peaks_rbtree_topdown PUBLIC
                           ${PROJECT_SOURCE_DIR}/${lib-dir}
)

