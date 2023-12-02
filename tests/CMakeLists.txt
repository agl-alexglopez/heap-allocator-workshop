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

###############    Performance Tester    ##################

FetchContent_Declare(matplotplusplus
  GIT_REPOSITORY https://github.com/alandefreitas/matplotplusplus
  GIT_TAG origin/master
)
FetchContent_GetProperties(matplotplusplus)
if(NOT matplotplusplus_POPULATED)
    FetchContent_Populate(matplotplusplus)
    add_subdirectory(${matplotplusplus_SOURCE_DIR} ${matplotplusplus_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
add_executable(perf perf.cc)
target_link_libraries(perf PUBLIC matplot)

###############   Generic Tests for all Allocators    ############

###############   Segregated Lists   ######################

add_executable(gtest_generic_list_segregated gtest_generic.cc)
target_link_libraries(gtest_generic_list_segregated GTest::gtest_main segment list_segregated)
gtest_discover_tests(gtest_generic_list_segregated)

###############   Splay Trees   ######################

add_executable(gtest_generic_splaytree_topdown gtest_generic.cc)
target_link_libraries(gtest_generic_splaytree_topdown GTest::gtest_main segment splaytree_topdown)
gtest_discover_tests(gtest_generic_splaytree_topdown)

add_executable(gtest_generic_splaytree_stack gtest_generic.cc)
target_link_libraries(gtest_generic_splaytree_stack GTest::gtest_main segment splaytree_stack)
gtest_discover_tests(gtest_generic_splaytree_stack)

###############   Red Black Trees   ######################

add_executable(gtest_generic_rbtree_clrs gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_clrs GTest::gtest_main segment rbtree_clrs)
gtest_discover_tests(gtest_generic_rbtree_clrs)

add_executable(gtest_generic_rbtree_unified gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_unified GTest::gtest_main segment rbtree_unified)
gtest_discover_tests(gtest_generic_rbtree_unified)

add_executable(gtest_generic_rbtree_linked gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_linked GTest::gtest_main segment rbtree_linked)
gtest_discover_tests(gtest_generic_rbtree_linked)

add_executable(gtest_generic_rbtree_stack gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_stack GTest::gtest_main segment rbtree_stack)
gtest_discover_tests(gtest_generic_rbtree_stack)

add_executable(gtest_generic_rbtree_topdown gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_topdown GTest::gtest_main segment rbtree_topdown)
gtest_discover_tests(gtest_generic_rbtree_topdown)

