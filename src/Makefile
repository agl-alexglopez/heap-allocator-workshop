# A simple makefile for building a program composed of C source files.
# Refer to CS107 guide to Make for background info on makefiles

# EDIT HERE to apply different gcc optimization flags (-Ox and -fxxx)
# Initially, the flags are configured for no optimization (to enable better
# debugging) but you can experiment with different compiler settings
# (e.g. different levels and enabling/disabling specific optimizations)
rbtree_clrs.o: CFLAGS += -O0
rbtree_unified.o: CFLAGS += -O0
rbtree_linked.o: CFLAGS += -O0
rbtree_stack.o: CFLAGS += -O0
rbtree_topdown.o: CFLAGS += -O0

ALLOCATORS = rbtree_clrs rbtree_unified rbtree_stack rbtree_linked rbtree_topdown
PROGRAMS = $(ALLOCATORS:%=test_%)
MY_PROGRAMS = $(ALLOCATORS:%=my_optional_program_%)

all:: $(PROGRAMS) $(MY_PROGRAMS)

# Mac needs gcc-11 and some path trickery to compile with a proper gcc compiler and not clang.
# Mac uses a homebrew install of gcc and puts it before clang in the $PATH. Linux is fine with gcc.
UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
CC = gcc-11
else
CC = gcc
endif

CFLAGS = -g3 -std=gnu99 -Wall $$warnflags -fcf-protection=none -fno-pic -no-pie
export warnflags = -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option
LDFLAGS =
LDLIBS =

$(PROGRAMS): test_%:%.o segment.c test_harness.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MY_PROGRAMS): my_optional_program_%:my_optional_program.c %.o segment.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean::
	rm -f $(PROGRAMS) $(MY_PROGRAMS) *.o callgrind.out.*

.PHONY: clean all

.INTERMEDIATE: $(ALLOCATORS:%=%.o)
