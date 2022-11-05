
# These are initial flags that came with the project so I am keeping them. The most important is
# -g3 that gives us the maximum possible debug information for gdb debugger. Set the level of
# optimization for each allocator while debugging (-O0 or -Og) and then for efficiency testing
# (-O2 or -O3). Finally, I do not want to configure a Debug and Release package for this project.
# Instead just go the the target_compile_options() for each allocator and set the level of
# optimization required. Use -Og while debugging then -O2 or -O3 when it is working.
add_library(printer_compiler_flags INTERFACE)
target_compile_features(printer_compiler_flags INTERFACE c_std_99)
set(gcc_like_c "$<COMPILE_LANG_AND_ID:C,ARMClang,AppleClang,Clang,GNU>")
target_compile_options(allocator_compiler_flags INTERFACE
    "$<${gcc_like_c}:$<BUILD_INTERFACE:-O3;-g3;-Wall;-Wextra;-Wfloat-equal;-Wtype-limits;-Wpointer-arith;-Wshadow;-Winit-self;-fno-diagnostics-show-option;-fcf-protection=none;-fno-pic;-fpie>>"
)
add_library(print_utility print_utility.h print_utility.c)
target_link_libraries(print_utility PUBLIC
    printer_compiler_flags
)

