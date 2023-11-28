enable_testing()
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

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

