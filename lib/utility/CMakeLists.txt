
add_library(debug_break debug_break.h debug_break.c)
add_library(print_utility print_utility.h print_utility.c)


############################# List Addressorder Utilities  ########################################


add_library(list_addressorder_design list_addressorder_design.h list_addressorder_design.c)

add_library(list_addressorder_tests list_addressorder_tests.h list_addressorder_tests.c)
target_link_libraries(list_addressorder_tests PUBLIC
    list_addressorder_design
    debug_break
)

add_library(list_addressorder_printer list_addressorder_printer.h list_addressorder_printer.c)
target_link_libraries(list_addressorder_printer PUBLIC
    print_utility
    list_addressorder_design
)

#############################    List Bestfit Utilities    ########################################


add_library(list_bestfit_design list_bestfit_design.h list_bestfit_design.c)

add_library(list_bestfit_tests list_bestfit_tests.h list_bestfit_tests.c)
target_link_libraries(list_bestfit_tests PUBLIC
    list_bestfit_design
    debug_break
)

add_library(list_bestfit_printer list_bestfit_printer.h list_bestfit_printer.c)
target_link_libraries(list_bestfit_printer PUBLIC
    print_utility
    list_bestfit_design
)


#############################   List Segregated Utilities  ########################################


add_library(list_segregated_design list_segregated_design.h list_segregated_design.c)

add_library(list_segregated_tests list_segregated_tests.h list_segregated_tests.c)
target_link_libraries(list_segregated_tests PUBLIC
    list_segregated_design
    debug_break
)

add_library(list_segregated_printer list_segregated_printer.h list_segregated_printer.c)
target_link_libraries(list_segregated_printer PUBLIC
    print_utility
    list_segregated_design
)


#############################    RBTree CLRS Utilities     ########################################


add_library(rbtree_clrs_design rbtree_clrs_design.h rbtree_clrs_design.c)

add_library(rbtree_clrs_tests rbtree_clrs_tests.h rbtree_clrs_tests.c)
target_link_libraries(rbtree_clrs_tests PUBLIC
    rbtree_clrs_design
    debug_break
)

add_library(rbtree_clrs_printer rbtree_clrs_printer.h rbtree_clrs_printer.c)
target_link_libraries(rbtree_clrs_printer PUBLIC
    print_utility
    rbtree_clrs_design
)

#############################   RBTree Unified Utilities    #######################################


add_library(rbtree_unified_design rbtree_unified_design.h rbtree_unified_design.c)

add_library(rbtree_unified_tests rbtree_unified_tests.h rbtree_unified_tests.c)
target_link_libraries(rbtree_unified_tests PUBLIC
    rbtree_unified_design
    debug_break
)

add_library(rbtree_unified_printer rbtree_unified_printer.h rbtree_unified_printer.c)
target_link_libraries(rbtree_unified_printer PUBLIC
    print_utility
    rbtree_unified_design
)

#############################    RBTree Linked Utilities    #######################################


add_library(rbtree_linked_design rbtree_linked_design.h rbtree_linked_design.c)

add_library(rbtree_linked_tests rbtree_linked_tests.h rbtree_linked_tests.c)
target_link_libraries(rbtree_linked_tests PUBLIC
    rbtree_linked_design
    debug_break
)

add_library(rbtree_linked_printer rbtree_linked_printer.h rbtree_linked_printer.c)
target_link_libraries(rbtree_linked_printer PUBLIC
    print_utility
    rbtree_linked_design
)

#############################    RBTree Stack Utilities     #######################################


add_library(rbtree_stack_design rbtree_stack_design.h rbtree_stack_design.c)

add_library(rbtree_stack_tests rbtree_stack_tests.h rbtree_stack_tests.c)
target_link_libraries(rbtree_stack_tests PUBLIC
    rbtree_stack_design
    debug_break
)

add_library(rbtree_stack_printer rbtree_stack_printer.h rbtree_stack_printer.c)
target_link_libraries(rbtree_stack_printer PUBLIC
    print_utility
    rbtree_stack_design
)

#############################   RBTree Topdown Utilities    #######################################


add_library(rbtree_topdown_design rbtree_topdown_design.h rbtree_topdown_design.c)

add_library(rbtree_topdown_tests rbtree_topdown_tests.h rbtree_topdown_tests.c)
target_link_libraries(rbtree_topdown_tests PUBLIC
    rbtree_topdown_design
    debug_break
)

add_library(rbtree_topdown_printer rbtree_topdown_printer.h rbtree_topdown_printer.c)
target_link_libraries(rbtree_topdown_printer PUBLIC
    print_utility
    rbtree_topdown_design
)
