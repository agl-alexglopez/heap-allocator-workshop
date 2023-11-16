##########################  Script Parsing for Heap Execution  ####################################

add_library(script script.h script.c)


#########################  Preparing Controlled Memory Segment  ###################################

add_library(segment segment.h segment.c)

############################# List Addressorder Allocator ########################################


add_library(list_addressorder allocator.h list_addressorder_algorithm.c)
target_link_libraries(list_addressorder PRIVATE
    list_addressorder_utilities
)


############################# List Bestfit Allocator ########################################


add_library(list_bestfit allocator.h list_bestfit_algorithm.c)
target_link_libraries(list_bestfit PRIVATE
    list_bestfit_utilities
)


############################# List Segregated Allocator ########################################


add_library(list_segregated allocator.h list_segregated_algorithm.c)
target_link_libraries(list_segregated PRIVATE
    list_segregated_utilities
)

############################# RBTree CLRS Allocator ########################################


add_library(rbtree_clrs allocator.h rbtree_clrs_algorithm.c)
target_link_libraries(rbtree_clrs PRIVATE
    rbtree_clrs_utilities
)


############################# RBTree Unified Allocator ########################################


add_library(rbtree_unified allocator.h rbtree_unified_algorithm.c)
target_link_libraries(rbtree_unified PRIVATE
    rbtree_unified_utilities
)


############################# RBTree Linked Allocator ########################################


add_library(rbtree_linked allocator.h rbtree_linked_algorithm.c)
target_link_libraries(rbtree_linked PRIVATE
    rbtree_linked_utilities
)


############################# RBTree Stack Allocator ########################################


add_library(rbtree_stack allocator.h rbtree_stack_algorithm.c)
target_link_libraries(rbtree_stack PRIVATE
    rbtree_stack_utilities
)


############################# RBTree Topdown Allocator ########################################


add_library(rbtree_topdown allocator.h rbtree_topdown_algorithm.c)
target_link_libraries(rbtree_topdown PRIVATE
    rbtree_topdown_utilities
)

