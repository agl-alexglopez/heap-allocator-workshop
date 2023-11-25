################################# TEST HARNESS PROGRAM ############################################

########## List Addressorder #############


add_executable(test_list_addressorder test_harness.c)
target_link_libraries(test_list_addressorder PUBLIC
                      segment
                      script
                      list_addressorder
)

########## List Bestfit #############


add_executable(test_list_bestfit test_harness.c)
target_link_libraries(test_list_bestfit PUBLIC
                      segment
                      script
                      list_bestfit
)

########## List Segregated #############


add_executable(test_list_segregated test_harness.c)
target_link_libraries(test_list_segregated PUBLIC
                      segment
                      script
                      list_segregated
)

########## RBTree CLRS #############


add_executable(test_rbtree_clrs test_harness.c)
target_link_libraries(test_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
)

########## RBTree Unified #############


add_executable(test_rbtree_unified test_harness.c)
target_link_libraries(test_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
)

########## RBTree Linked #############


add_executable(test_rbtree_linked test_harness.c)
target_link_libraries(test_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
)

########## RBTree Stack #############


add_executable(test_rbtree_stack test_harness.c)
target_link_libraries(test_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
)

########## RBTree Topdown #############


add_executable(test_rbtree_topdown test_harness.c)
target_link_libraries(test_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
)

########## Splay Tree #############


add_executable(test_splaytree test_harness.c)
target_link_libraries(test_splaytree PUBLIC
                      segment
                      script
                      splaytree
)

########## Splay Tree Topdown #############


add_executable(test_splaytree_topdown test_harness.c)
target_link_libraries(test_splaytree_topdown PUBLIC
                      segment
                      script
                      splaytree_topdown
)


################################# TIME HARNESS PROGRAM ############################################


########## List Addressorder #############


add_executable(time_list_addressorder time_harness.c)
target_link_libraries(time_list_addressorder PUBLIC
                      segment
                      print_utility
                      script
                      list_addressorder
)

########## List Bestfit #############


add_executable(time_list_bestfit time_harness.c)
target_link_libraries(time_list_bestfit PUBLIC
                      segment
                      print_utility
                      script
                      list_bestfit
)

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

############ Splay Tree ###############


add_executable(time_splaytree time_harness.c)
target_link_libraries(time_splaytree PUBLIC
                      segment
                      print_utility
                      script
                      splaytree
)

############ Splay Tree Topdown ###############


add_executable(time_splaytree_topdown time_harness.c)
target_link_libraries(time_splaytree_topdown PUBLIC
                      segment
                      print_utility
                      script
                      splaytree_topdown
)


################################# PRINTER PROGRAM ############################################


########## List Addressorder #############


add_executable(print_peaks_list_addressorder print_peaks.c)
target_link_libraries(print_peaks_list_addressorder PUBLIC
                      segment
                      print_utility
                      script
                      list_addressorder
)

########## List Bestfit #############


add_executable(print_peaks_list_bestfit print_peaks.c)
target_link_libraries(print_peaks_list_bestfit PUBLIC
                      segment
                      print_utility
                      script
                      list_bestfit
)

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

######## Splay Tree ###########


add_executable(print_peaks_splaytree print_peaks.c)
target_link_libraries(print_peaks_splaytree PUBLIC
                      segment
                      print_utility
                      script
                      splaytree
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


########## List Addressorder #############


add_executable(my_optional_program_list_addressorder my_optional_program.c)
target_link_libraries(my_optional_program_list_addressorder PUBLIC
                      segment
                      script
                      list_addressorder
)

########## List Bestfit #############


add_executable(my_optional_program_list_bestfit my_optional_program.c)
target_link_libraries(my_optional_program_list_bestfit PUBLIC
                      segment
                      script
                      list_bestfit
)

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

#######  Splay Tree ##########


add_executable(my_optional_program_splaytree my_optional_program.c)
target_link_libraries(my_optional_program_splaytree PUBLIC
                      segment
                      script
                      splaytree
)

#######  Splay Tree Topdown ##########


add_executable(my_optional_program_splaytree_topdown my_optional_program.c)
target_link_libraries(my_optional_program_splaytree_topdown PUBLIC
                      segment
                      script
                      splaytree_topdown
)

