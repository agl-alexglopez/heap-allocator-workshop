###############     Thread Safe IO      ###################

add_library(osync osync.hh osync.cc)

###############     Script Parsing      ###################

add_library(script script.hh script.cc)

#####################   Google Tests   ###################

include(FetchContent)

enable_testing()
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

###############    Performance Plotter Tester    ##################

FetchContent_Declare(matplotplusplus
  GIT_REPOSITORY https://github.com/alandefreitas/matplotplusplus
  GIT_TAG origin/master
)
FetchContent_GetProperties(matplotplusplus)
if(NOT matplotplusplus_POPULATED)
    FetchContent_Populate(matplotplusplus)
    add_subdirectory(${matplotplusplus_SOURCE_DIR} ${matplotplusplus_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
add_executable(plot plot.cc)
target_link_libraries(plot PUBLIC matplot osync)



###############    Correctness Tester    ##################


########## List Segregated #############


add_executable(ctest_list_segregated ctest.cc)
target_link_libraries(ctest_list_segregated PUBLIC
  segment
  osync
  script
  list_segregated
)

########## RBTree CLRS #############


add_executable(ctest_rbtree_clrs ctest.cc)
target_link_libraries(ctest_rbtree_clrs PUBLIC
  segment
  osync
  script
  rbtree_clrs
)

########## RBTree Unified #############


add_executable(ctest_rbtree_unified ctest.cc)
target_link_libraries(ctest_rbtree_unified PUBLIC
  segment
  osync
  script
  rbtree_unified
)

########## RBTree Linked #############


add_executable(ctest_rbtree_linked ctest.cc)
target_link_libraries(ctest_rbtree_linked PUBLIC
  segment
  osync
  script
  rbtree_linked
)

########## RBTree Stack #############


add_executable(ctest_rbtree_stack ctest.cc)
target_link_libraries(ctest_rbtree_stack PUBLIC
  segment
  osync
  script
  rbtree_stack
)

########## RBTree Topdown #############


add_executable(ctest_rbtree_topdown ctest.cc)
target_link_libraries(ctest_rbtree_topdown PUBLIC
  segment
  osync
  script
  rbtree_topdown
)

########## Splay Tree Stack #############


add_executable(ctest_splaytree_stack ctest.cc)
target_link_libraries(ctest_splaytree_stack PUBLIC
  segment
  osync
  script
  splaytree_stack
)

########## Splay Tree Topdown #############


add_executable(ctest_splaytree_topdown ctest.cc)
target_link_libraries(ctest_splaytree_topdown PUBLIC
  segment
  osync
  script
  splaytree_topdown
)

###############   Generic Tests for all Allocators    ############

###############   Segregated Lists   ######################

add_executable(gtest_generic_list_segregated gtest_generic.cc)
target_link_libraries(gtest_generic_list_segregated 
  GTest::gtest_main 
  segment 
  list_segregated
)
gtest_discover_tests(gtest_generic_list_segregated)

###############   Splay Trees   ######################

add_executable(gtest_generic_splaytree_topdown gtest_generic.cc)
target_link_libraries(gtest_generic_splaytree_topdown
  GTest::gtest_main 
  segment 
  splaytree_topdown
)
gtest_discover_tests(gtest_generic_splaytree_topdown)

add_executable(gtest_generic_splaytree_stack gtest_generic.cc)
target_link_libraries(gtest_generic_splaytree_stack
  GTest::gtest_main 
  segment 
  splaytree_stack
)
gtest_discover_tests(gtest_generic_splaytree_stack)

###############   Red Black Trees   ######################

add_executable(gtest_generic_rbtree_clrs gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_clrs
  GTest::gtest_main 
  segment 
  rbtree_clrs
)
gtest_discover_tests(gtest_generic_rbtree_clrs)

add_executable(gtest_generic_rbtree_unified gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_unified
  GTest::gtest_main 
  segment 
  rbtree_unified
)
gtest_discover_tests(gtest_generic_rbtree_unified)

add_executable(gtest_generic_rbtree_linked gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_linked
  GTest::gtest_main 
  segment 
  rbtree_linked
)
gtest_discover_tests(gtest_generic_rbtree_linked)

add_executable(gtest_generic_rbtree_stack gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_stack
  GTest::gtest_main 
  segment 
  rbtree_stack
)
gtest_discover_tests(gtest_generic_rbtree_stack)

add_executable(gtest_generic_rbtree_topdown gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_topdown
  GTest::gtest_main 
  segment 
  rbtree_topdown
)
gtest_discover_tests(gtest_generic_rbtree_topdown)


####################   Print and Plot Peaks   ########################


###############   Segregated Lists   ######################

add_executable(print_peaks_list_segregated print_peaks.cc)
target_link_libraries(print_peaks_list_segregated
  segment 
  script
  list_segregated)

###############   Splay Trees   ######################

add_executable(print_peaks_splaytree_topdown print_peaks.cc)
target_link_libraries(print_peaks_splaytree_topdown 
  segment 
  script
  splaytree_topdown)

add_executable(print_peaks_splaytree_stack print_peaks.cc)
target_link_libraries(print_peaks_splaytree_stack 
  segment 
  script
  splaytree_stack)

###############   Red Black Trees   ######################

add_executable(print_peaks_rbtree_clrs print_peaks.cc)
target_link_libraries(print_peaks_rbtree_clrs 
  segment 
  script
  rbtree_clrs)

add_executable(print_peaks_rbtree_unified print_peaks.cc)
target_link_libraries(print_peaks_rbtree_unified 
  segment 
  script
  rbtree_unified)

add_executable(print_peaks_rbtree_linked print_peaks.cc)
target_link_libraries(print_peaks_rbtree_linked 
  segment 
  script
  rbtree_linked)

add_executable(print_peaks_rbtree_stack print_peaks.cc)
target_link_libraries(print_peaks_rbtree_stack 
  segment 
  script
  rbtree_stack)

add_executable(print_peaks_rbtree_topdown print_peaks.cc)
target_link_libraries(print_peaks_rbtree_topdown 
  segment 
  script
  rbtree_topdown)

################################# QUICK STAT PROGRAM ############################################

########## Libc Default #############


add_executable(stats_libc_default stats.cc)
target_link_libraries(stats_libc_default PUBLIC
                      segment
                      osync
                      script
                      libc_default
)

########## List Segregated #############


add_executable(stats_list_segregated stats.cc)
target_link_libraries(stats_list_segregated PUBLIC
                      segment
                      osync
                      script
                      list_segregated
)

############ RBTree ClRS ###############


add_executable(stats_rbtree_clrs stats.cc)
target_link_libraries(stats_rbtree_clrs PUBLIC
                      segment
                      osync
                      script
                      rbtree_clrs
)

############ RBTree Unified ###############


add_executable(stats_rbtree_unified stats.cc)
target_link_libraries(stats_rbtree_unified PUBLIC
                      segment
                      osync
                      script
                      rbtree_unified
)

############ RBTree Linked ###############


add_executable(stats_rbtree_linked stats.cc)
target_link_libraries(stats_rbtree_linked PUBLIC
                      segment
                      osync
                      script
                      rbtree_linked
)

############ RBTree Stack ###############


add_executable(stats_rbtree_stack stats.cc)
target_link_libraries(stats_rbtree_stack PUBLIC
                      segment
                      osync
                      script
                      rbtree_stack
)

############ RBTree Topdown ###############


add_executable(stats_rbtree_topdown stats.cc)
target_link_libraries(stats_rbtree_topdown PUBLIC
                      segment
                      osync
                      script
                      rbtree_topdown
)

############ Splay Tree Stack ###############


add_executable(stats_splaytree_stack stats.cc)
target_link_libraries(stats_splaytree_stack PUBLIC
                      segment
                      osync
                      script
                      splaytree_stack
)

############ Splay Tree Topdown ###############


add_executable(stats_splaytree_topdown stats.cc)
target_link_libraries(stats_splaytree_topdown PUBLIC
                      segment
                      osync
                      script
                      splaytree_topdown
)

