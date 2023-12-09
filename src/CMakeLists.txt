
################################# SANDBOX PROGRAM ############################################


####### List Segregated ############


add_executable(sandbox_list_segregated sandbox.c)
target_link_libraries(sandbox_list_segregated PUBLIC
                      segment
                      script
                      list_segregated
)

######## RBTree CLRS ###########


add_executable(sandbox_rbtree_clrs sandbox.c)
target_link_libraries(sandbox_rbtree_clrs PUBLIC
                      segment
                      script
                      rbtree_clrs
)

######## RBTree Unified ###########


add_executable(sandbox_rbtree_unified sandbox.c)
target_link_libraries(sandbox_rbtree_unified PUBLIC
                      segment
                      script
                      rbtree_unified
)

######## RBTree Linked ###########


add_executable(sandbox_rbtree_linked sandbox.c)
target_link_libraries(sandbox_rbtree_linked PUBLIC
                      segment
                      script
                      rbtree_linked
)

#######  RBTree Stack ##########


add_executable(sandbox_rbtree_stack sandbox.c)
target_link_libraries(sandbox_rbtree_stack PUBLIC
                      segment
                      script
                      rbtree_stack
)

#######  RBTree Topdown ##########


add_executable(sandbox_rbtree_topdown sandbox.c)
target_link_libraries(sandbox_rbtree_topdown PUBLIC
                      segment
                      script
                      rbtree_topdown
)

#######  Splay Tree Stack ##########


add_executable(sandbox_splaytree_stack sandbox.c)
target_link_libraries(sandbox_splaytree_stack PUBLIC
                      segment
                      script
                      splaytree_stack
)

#######  Splay Tree Topdown ##########


add_executable(sandbox_splaytree_topdown sandbox.c)
target_link_libraries(sandbox_splaytree_topdown PUBLIC
                      segment
                      script
                      splaytree_topdown
)

