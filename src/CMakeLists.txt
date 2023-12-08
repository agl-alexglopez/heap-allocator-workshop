
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

