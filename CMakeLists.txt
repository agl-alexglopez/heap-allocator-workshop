cmake_minimum_required(VERSION 3.14)
project(red-black-tree-allocators VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
# This helps something like LSP in my neovim config find include paths and gtest libraries. Now, I
# won't get countless errors and autocomplete works on google test language and my class.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
add_subdirectory(lib)
