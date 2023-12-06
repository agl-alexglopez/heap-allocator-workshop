add_library(debug_break INTERFACE debug_break.h)

add_library(print_utility print_utility.h print_utility.c)

#########################  Preparing Controlled Memory Segment  ###################################

add_library(segment segment.h segment.c)

############################# Libc Default Allocator ########################################

add_library(libc_default allocator.h libc_default.c)

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

############################# Splay Tree Stack Allocator ########################################

add_library(splaytree_stack allocator.h splaytree_stack.c)

############################# Splay Tree Topdown ########################################

add_library(splaytree_topdown allocator.h splaytree_topdown.c)

