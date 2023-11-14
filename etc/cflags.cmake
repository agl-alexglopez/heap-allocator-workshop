set (CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set(SANITIZING_FLAGS -fno-sanitize-recover=all -fsanitize=undefined -fsanitize=address)

# ask for more warnings from the compiler
set (CMAKE_BASE_C_FLAGS "${CMAKE_C_FLAGS}")

# -Werror=effc++ has been removed ?

# AppleClang specific flags
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "AppleClang")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -fpie")
endif()

# GCC or Clang specific flags
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -fpie -D_GNU_SOURCE")
endif()
