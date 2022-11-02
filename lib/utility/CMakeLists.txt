
add_library(debug_break debug_break.h debug_break.c)
add_library(print_utility print_utility.h print_utility.c)


############################# List Addressorder Utilities  ########################################


add_library(list_addressorder_utility list_addressorder_utility.h list_addressorder_utility.c)
target_link_libraries(list_addressorder_utility PUBLIC
    debug_break
    print_utility
)


#############################    List Bestfit Utilities    ########################################


add_library(list_bestfit_utility list_bestfit_utility.h list_bestfit_utility.c)
target_link_libraries(list_bestfit_utility PUBLIC
    debug_break
    print_utility
)


#############################   List Segregated Utilities  ########################################


add_library(list_segregated_utility list_segregated_utility.h list_segregated_utility.c)
target_link_libraries(list_segregated_utility PUBLIC
    debug_break
    print_utility
)


#############################    RBTree CLRS Utilities     ########################################


add_library(rbtree_clrs_utility rbtree_clrs_utility.h rbtree_clrs_utility.c)
target_link_libraries(rbtree_clrs_utility PUBLIC
    debug_break
    print_utility
)


#############################   RBTree Unified Utilities    #######################################


add_library(rbtree_unified_utility rbtree_unified_utility.h rbtree_unified_utility.c)
target_link_libraries(rbtree_unified_utility PUBLIC
    debug_break
    print_utility
)


#############################    RBTree Linked Utilities    #######################################


add_library(rbtree_linked_utility rbtree_linked_utility.h rbtree_linked_utility.c)
target_link_libraries(rbtree_linked_utility PUBLIC
    debug_break
    print_utility
)


#############################    RBTree Stack Utilities     #######################################


add_library(rbtree_stack_utility rbtree_stack_utility.h rbtree_stack_utility.c)
target_link_libraries(rbtree_stack_utility PUBLIC
    debug_break
    print_utility
)


#############################   RBTree Topdown Utilities    #######################################


add_library(rbtree_topdown_utility rbtree_topdown_utility.h rbtree_topdown_utility.c)
target_link_libraries(rbtree_topdown_utility PUBLIC
    debug_break
    print_utility
)

