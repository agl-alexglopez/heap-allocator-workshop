################################# TIME HARNESS PROGRAM ############################################


########## List Segregated #############


add_executable(time_list_segregated time_harness.c)
target_link_libraries(time_list_segregated PUBLIC
                      segment
                      print_utility
                      script
                      list_segregated
)

############ RBTree ClRS ###############


add_executable(time_rbtree_clrs time_harness.c)
target_link_libraries(time_rbtree_clrs PUBLIC
                      segment
                      print_utility
                      script
                      rbtree_clrs
)

############ RBTree Unified ###############


add_executable(time_rbtree_unified time_harness.c)
target_link_libraries(time_rbtree_unified PUBLIC
                      segment
                      print_utility
                      script
                      rbtree_unified
)

############ RBTree Linked ###############


add_executable(time_rbtree_linked time_harness.c)
target_link_libraries(time_rbtree_linked PUBLIC
                      segment
                      print_utility
                      script
                      rbtree_linked
)

############ RBTree Stack ###############


add_executable(time_rbtree_stack time_harness.c)
target_link_libraries(time_rbtree_stack PUBLIC
                      segment
                      print_utility
                      script
                      rbtree_stack
)

############ RBTree Topdown ###############


add_executable(time_rbtree_topdown time_harness.c)
target_link_libraries(time_rbtree_topdown PUBLIC
                      segment
                      print_utility
                      script
                      rbtree_topdown
)

############ Splay Tree Stack ###############


add_executable(time_splaytree_stack time_harness.c)
target_link_libraries(time_splaytree_stack PUBLIC
                      segment
                      print_utility
                      script
                      splaytree_stack
)

############ Splay Tree Topdown ###############


add_executable(time_splaytree_topdown time_harness.c)
target_link_libraries(time_splaytree_topdown PUBLIC
                      segment
                      print_utility
                      script
                      splaytree_topdown
)

################################# QUICK STAT PROGRAM ############################################

########## Libc Default #############


add_executable(stats_libc_default stats.c)
target_link_libraries(stats_libc_default PUBLIC
                      segment
                      script
                      libc_default
)

########## List Segregated #############


add_executable(stats_list_segregated stats.c)
target_link_libraries(stats_list_segregated PUBLIC
                      segment
                      script
                      list_segregated
)

############ RBTree ClRS ###############


add_executable(stats_rbtree_clrs stats.c)
target_link_libraries(stats_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
)

############ RBTree Unified ###############


add_executable(stats_rbtree_unified stats.c)
target_link_libraries(stats_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
)

############ RBTree Linked ###############


add_executable(stats_rbtree_linked stats.c)
target_link_libraries(stats_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
)

############ RBTree Stack ###############


add_executable(stats_rbtree_stack stats.c)
target_link_libraries(stats_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
)

############ RBTree Topdown ###############


add_executable(stats_rbtree_topdown stats.c)
target_link_libraries(stats_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
)

############ Splay Tree Stack ###############


add_executable(stats_splaytree_stack stats.c)
target_link_libraries(stats_splaytree_stack PUBLIC
                      segment
                      script
                      splaytree_stack
)

############ Splay Tree Topdown ###############


add_executable(stats_splaytree_topdown stats.c)
target_link_libraries(stats_splaytree_topdown PUBLIC
                      segment
                      script
                      splaytree_topdown
)


################################# PRINTER PROGRAM ############################################


####### List Segregated ############


add_executable(print_peaks_list_segregated print_peaks.c)
target_link_libraries(print_peaks_list_segregated PUBLIC
                      segment
                      print_utility
                      script
                      list_segregated
)

######## RBTree CLRS ###########


add_executable(print_peaks_rbtree_clrs print_peaks.c)
target_link_libraries(print_peaks_rbtree_clrs PUBLIC
                      segment
                      print_utility
                      script
                      rbtree_clrs
)

######## RBTree Unified ###########


add_executable(print_peaks_rbtree_unified print_peaks.c)
target_link_libraries(print_peaks_rbtree_unified PUBLIC
                      segment
                      print_utility
                      script
                      rbtree_unified
)

######## RBTree Linked ###########


add_executable(print_peaks_rbtree_linked print_peaks.c)
target_link_libraries(print_peaks_rbtree_linked PUBLIC
                      segment
                      print_utility
                      script
                      rbtree_linked
)

######## RBTree Stack ###########


add_executable(print_peaks_rbtree_stack print_peaks.c)
target_link_libraries(print_peaks_rbtree_stack PUBLIC
                      segment
                      print_utility
                      script
                      rbtree_stack
)

######## RBTree Topdown ###########


add_executable(print_peaks_rbtree_topdown print_peaks.c)
target_link_libraries(print_peaks_rbtree_topdown PUBLIC
                      segment
                      print_utility
                      script
                      rbtree_topdown
)

######## Splay Tree Stack ###########


add_executable(print_peaks_splaytree_stack print_peaks.c)
target_link_libraries(print_peaks_splaytree_stack PUBLIC
                      segment
                      print_utility
                      script
                      splaytree_stack
)

######## Splay Tree Topdown ###########


add_executable(print_peaks_splaytree_topdown print_peaks.c)
target_link_libraries(print_peaks_splaytree_topdown PUBLIC
                      segment
                      print_utility
                      script
                      splaytree_topdown
)

################################# OPTIONAL PROGRAM ############################################


####### List Segregated ############


add_executable(my_optional_program_list_segregated my_optional_program.c)
target_link_libraries(my_optional_program_list_segregated PUBLIC
                      segment
                      script
                      list_segregated
)

######## RBTree CLRS ###########


add_executable(my_optional_program_rbtree_clrs my_optional_program.c)
target_link_libraries(my_optional_program_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
)

######## RBTree Unified ###########


add_executable(my_optional_program_rbtree_unified my_optional_program.c)
target_link_libraries(my_optional_program_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
)

######## RBTree Linked ###########


add_executable(my_optional_program_rbtree_linked my_optional_program.c)
target_link_libraries(my_optional_program_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
)

#######  RBTree Stack ##########


add_executable(my_optional_program_rbtree_stack my_optional_program.c)
target_link_libraries(my_optional_program_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
)

#######  RBTree Topdown ##########


add_executable(my_optional_program_rbtree_topdown my_optional_program.c)
target_link_libraries(my_optional_program_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
)

#######  Splay Tree Stack ##########


add_executable(my_optional_program_splaytree_stack my_optional_program.c)
target_link_libraries(my_optional_program_splaytree_stack PUBLIC
                      segment
                      script
                      splaytree_stack
)

#######  Splay Tree Topdown ##########


add_executable(my_optional_program_splaytree_topdown my_optional_program.c)
target_link_libraries(my_optional_program_splaytree_topdown PUBLIC
                      segment
                      script
                      splaytree_topdown
)

