cmake_minimum_required(VERSION 3.14)
project(red-black-tree-allocators VERSION 1.0 LANGUAGES C CXX)

include(etc/scanners.cmake)

include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/lib")
include_directories("${PROJECT_SOURCE_DIR}/tests")

add_subdirectory("${PROJECT_SOURCE_DIR}/src")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib")
add_subdirectory("${PROJECT_SOURCE_DIR}/tests")
