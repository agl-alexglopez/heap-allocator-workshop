##########################  Script Parsing for Heap Execution  ####################################

add_library(script script.h script.c)
target_compile_options(script PUBLIC "-O3")


#########################  Preparing Controlled Memory Segment  ###################################

add_library(segment segment.h segment.c)
target_compile_options(segment PUBLIC "-O3")

############################# List Addressorder Allocator ########################################


add_library(list_addressorder allocator.h list_addressorder_algorithm.c)
target_link_libraries(list_addressorder PRIVATE
    list_addressorder_utilities
)
target_compile_options(list_addressorder PUBLIC "-O3")


############################# List Bestfit Allocator ########################################


add_library(list_bestfit allocator.h list_bestfit_algorithm.c)
target_link_libraries(list_bestfit PRIVATE
    list_bestfit_utilities
)
target_compile_options(list_bestfit PUBLIC "-O3")


############################# List Segregated Allocator ########################################


add_library(list_segregated allocator.h list_segregated_algorithm.c)
target_link_libraries(list_segregated PRIVATE
    list_segregated_utilities
)
target_compile_options(list_segregated PUBLIC "-O3")

############################# RBTree CLRS Allocator ########################################


add_library(rbtree_clrs allocator.h rbtree_clrs_algorithm.c)
target_link_libraries(rbtree_clrs PRIVATE
    rbtree_clrs_utilities
)
target_compile_options(rbtree_clrs PUBLIC "-O3")


############################# RBTree Unified Allocator ########################################


add_library(rbtree_unified allocator.h rbtree_unified_algorithm.c)
target_link_libraries(rbtree_unified PRIVATE
    rbtree_unified_utilities
)
target_compile_options(rbtree_unified PUBLIC "-O3")


############################# RBTree Linked Allocator ########################################


add_library(rbtree_linked allocator.h rbtree_linked_algorithm.c)
target_link_libraries(rbtree_linked PRIVATE
    rbtree_linked_utilities
)
target_compile_options(rbtree_linked PUBLIC "-O3")


############################# RBTree Stack Allocator ########################################


add_library(rbtree_stack allocator.h rbtree_stack_algorithm.c)
target_link_libraries(rbtree_stack PRIVATE
    rbtree_stack_utilities
)
target_compile_options(rbtree_stack PUBLIC "-O3")


############################# RBTree Topdown Allocator ########################################


add_library(rbtree_topdown allocator.h rbtree_topdown_algorithm.c)
target_link_libraries(rbtree_topdown PRIVATE
    rbtree_topdown_utilities
)
target_compile_options(rbtree_topdown PUBLIC "-O3")

