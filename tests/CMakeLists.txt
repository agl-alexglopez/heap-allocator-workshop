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

add_executable(gtest_generic_list_segregated gtest_generic.cc)
target_link_libraries(gtest_generic_list_segregated GTest::gtest_main segment list_segregated)
gtest_discover_tests(gtest_generic_list_segregated)

add_executable(gtest_generic_splaytree_topdown gtest_generic.cc)
target_link_libraries(gtest_generic_splaytree_topdown GTest::gtest_main segment splaytree_topdown)
gtest_discover_tests(gtest_generic_splaytree_topdown)

add_executable(gtest_generic_rbtree_linked gtest_generic.cc)
target_link_libraries(gtest_generic_rbtree_linked GTest::gtest_main segment rbtree_linked)
gtest_discover_tests(gtest_generic_rbtree_linked)

