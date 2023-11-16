set (CMAKE_C_STANDARD 11)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

# AppleClang specific flags
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "AppleClang")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Not sure if the address sanitizers and whatnot do anything, not getting any warnings.
        add_compile_options(-g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -fpie -D_GNU_SOURCE -fno-sanitize-recover=all -fsanitize=undefined -fsanitize=address)
        add_link_options(-fno-sanitize-recover=all -fsanitize=undefined -fsanitize=address)
    else()
        add_compile_options(-O3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -fpie -D_GNU_SOURCE -fno-sanitize-recover=all)
    endif()
endif()

# GCC or Clang specific flags
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Not sure if the address sanitizers and whatnot do anything, not getting any warnings.
        add_compile_options(-g3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -fpie -D_GNU_SOURCE -fno-sanitize-recover=all -fsanitize=undefined -fsanitize=address)
        add_link_options(-fno-sanitize-recover=all -fsanitize=undefined -fsanitize=address)
    else()
        add_compile_options(-O3 -Wall -Wextra -Wfloat-equal -Wtype-limits -Wpointer-arith -Wshadow -Winit-self -fno-diagnostics-show-option -fcf-protection=none -fno-pic -fpie -D_GNU_SOURCE)
    endif()
endif()
