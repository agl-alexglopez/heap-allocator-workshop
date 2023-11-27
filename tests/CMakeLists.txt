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

add_executable(heap_tests_splaytree_topdown heap_tests.cc)
target_link_libraries(heap_tests_splaytree_topdown GTest::gtest_main segment splaytree_topdown)
gtest_discover_tests(heap_tests_splaytree_topdown)
