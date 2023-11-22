add_library(debug_break INTERFACE debug_break.h)

add_library(print_utility print_utility.h print_utility.c)

##########################  Script Parsing for Heap Execution  ####################################

add_library(script script.h script.c)


#########################  Preparing Controlled Memory Segment  ###################################

add_library(segment segment.h segment.c)

############################# List Addressorder Allocator ########################################


add_library(list_addressorder allocator.h list_addressorder.c)


############################# List Bestfit Allocator ########################################


add_library(list_bestfit allocator.h list_bestfit.c)

############################# List Segregated Allocator ########################################


add_library(list_segregated allocator.h list_segregated.c)

############################# RBTree CLRS Allocator ########################################


add_library(rbtree_clrs allocator.h rbtree_clrs.c)

############################# RBTree Unified Allocator ########################################


add_library(rbtree_unified allocator.h rbtree_unified.c)

############################# RBTree Linked Allocator ########################################


add_library(rbtree_linked allocator.h rbtree_linked.c)

############################# RBTree Stack Allocator ########################################


add_library(rbtree_stack allocator.h rbtree_stack.c)


############################# RBTree Topdown Allocator ########################################

add_library(rbtree_topdown allocator.h rbtree_topdown.c)

############################# Splay Tree Allocator ########################################

add_library(splaytree allocator.h splaytree.c)

############################# Splay Tree Topdown ########################################

add_library(splaytree_topdown allocator.h splaytree_topdown.c)

