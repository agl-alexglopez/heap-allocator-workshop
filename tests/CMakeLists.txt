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


# The allocator name is the name of the .c file in the lib/ folder.

###############    Correctness Tester    ##################

macro(add_ctest ALLOCATOR_NAME)
  add_executable(ctest_${ALLOCATOR_NAME} ctest.cc)
  target_link_libraries(ctest_${ALLOCATOR_NAME} PUBLIC
    segment
    osync
    script
    ${ALLOCATOR_NAME}
  )
endmacro()

add_ctest(list_segregated)
add_ctest(rbtree_clrs)
add_ctest(rbtree_unified)
add_ctest(rbtree_linked)
add_ctest(rbtree_stack)
add_ctest(rbtree_topdown)
add_ctest(splaytree_stack)
add_ctest(splaytree_topdown)


###############   Generic Tests for all Allocators    ############

macro(add_gtest ALLOCATOR_NAME)
  add_executable(gtest_${ALLOCATOR_NAME} gtest.cc)
  target_link_libraries(gtest_${ALLOCATOR_NAME}
    GTest::gtest_main
    segment
    ${ALLOCATOR_NAME}
  )
  gtest_discover_tests(gtest_${ALLOCATOR_NAME})
endmacro()

add_gtest(list_segregated)
add_gtest(rbtree_clrs)
add_gtest(rbtree_unified)
add_gtest(rbtree_linked)
add_gtest(rbtree_stack)
add_gtest(rbtree_topdown)
add_gtest(splaytree_stack)
add_gtest(splaytree_topdown)

####################   Print and Plot Peaks   ########################

macro(add_peaks ALLOCATOR_NAME)
  add_executable(peaks_${ALLOCATOR_NAME} peaks.cc)
  target_link_libraries(peaks_${ALLOCATOR_NAME}
    segment
    matplot
    osync
    script
    ${ALLOCATOR_NAME}
  )
endmacro()

add_peaks(list_segregated)
add_peaks(rbtree_clrs)
add_peaks(rbtree_unified)
add_peaks(rbtree_linked)
add_peaks(rbtree_stack)
add_peaks(rbtree_topdown)
add_peaks(splaytree_stack)
add_peaks(splaytree_topdown)

################################# QUICK STAT PROGRAM ############################################

macro(add_stats ALLOCATOR_NAME)
  add_executable(stats_${ALLOCATOR_NAME} stats.cc)
  target_link_libraries(stats_${ALLOCATOR_NAME} PUBLIC
    segment
    osync
    script
    ${ALLOCATOR_NAME}
  )
endmacro()

add_stats(list_segregated)
add_stats(rbtree_clrs)
add_stats(rbtree_unified)
add_stats(rbtree_linked)
add_stats(rbtree_stack)
add_stats(rbtree_topdown)
add_stats(splaytree_stack)
add_stats(splaytree_topdown)
add_stats(libc_default)

