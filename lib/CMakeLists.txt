
add_library(debug_break debug_break.h)
set_target_properties(debug_break PROPERTIES LINKER_LANGUAGE C)

add_library(segment segment.h segment.c)
add_library(script script.h script.c)

add_library(list_addressorder allocator.h list_addressorder.c)
target_link_libraries(list_addressorder PUBLIC
                      debug_break
)
set_target_properties(list_addressorder PROPERTIES
                      COMPILE_FLAGS
                      "-O3 -g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -no-pie -fPIE"
)

add_library(list_bestfit allocator.h list_bestfit.c)
target_link_libraries(list_bestfit PUBLIC
                      debug_break
)
set_target_properties(list_bestfit PROPERTIES
                      COMPILE_FLAGS
                      "-O3 -g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -no-pie -fPIE"
)

add_library(list_segregated allocator.h list_segregated.c)
target_link_libraries(list_segregated PUBLIC
                      debug_break
)
set_target_properties(list_segregated PROPERTIES
                      COMPILE_FLAGS
                      "-O3 -g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -no-pie -fPIE"
)

add_library(rbtree_clrs allocator.h rbtree_clrs.c)
target_link_libraries(rbtree_clrs PUBLIC
                      debug_break
)
set_target_properties(rbtree_clrs PROPERTIES
                      COMPILE_FLAGS
                      "-O3 -g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -no-pie -fPIE"
)

add_library(rbtree_unified allocator.h rbtree_unified.c)
target_link_libraries(rbtree_unified PUBLIC
                      debug_break
)
set_target_properties(rbtree_unified PROPERTIES
                      COMPILE_FLAGS
                      "-O3 -g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -no-pie -fPIE"
)

add_library(rbtree_linked allocator.h rbtree_linked.c)
target_link_libraries(rbtree_linked PUBLIC
                      debug_break
)
set_target_properties(rbtree_linked PROPERTIES
                      COMPILE_FLAGS
                      "-O3 -g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -no-pie -fPIE"
)

add_library(rbtree_stack allocator.h rbtree_stack.c)
target_link_libraries(rbtree_stack PUBLIC
                      debug_break
)
set_target_properties(rbtree_stack PROPERTIES
                      COMPILE_FLAGS
                      "-O3 -g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -no-pie -fPIE"
)

add_library(rbtree_topdown allocator.h rbtree_topdown.c)
target_link_libraries(rbtree_topdown PUBLIC
                      debug_break
)
set_target_properties(rbtree_topdown PROPERTIES
                      COMPILE_FLAGS
                      "-O3 -g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -no-pie -fPIE"
)

