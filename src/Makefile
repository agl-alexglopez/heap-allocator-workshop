# The SRCDIR is implicit because I leave the Makefile in the same directory as the source files.
SRCDIR = ../src
OBJDIR = ../obj
BINDIR = ../bin
SRC := $(wildcard $(SRCDIR)/*.c)
OBJ := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o,$(SRC))

# While debugging and testing allocators use -Og flag, then optimize once correct.
$(OBJDIR)/list_bestfit.o: CFLAGS += -O3
$(OBJDIR)/list_addressorder.o: CFLAGS += -O3
$(OBJDIR)/list_segregated.o: CFLAGS += -O3
$(OBJDIR)/rbtree_clrs.o: CFLAGS += -O3
$(OBJDIR)/rbtree_unified.o: CFLAGS += -O3
$(OBJDIR)/rbtree_linked.o: CFLAGS += -O3
$(OBJDIR)/rbtree_stack.o: CFLAGS += -O3
$(OBJDIR)/rbtree_topdown.o: CFLAGS += -O3

# Files that set up basic heap segment and script parsing. Needed by all programs that use allocator.
DEPENDENCIES = segment.c script.c

ALLOCATORS = list_bestfit list_addressorder list_segregated rbtree_clrs rbtree_unified rbtree_stack rbtree_linked rbtree_topdown
TEST_PROGRAMS = $(addprefix $(BINDIR)/test_,$(ALLOCATORS))
TIMER_PROGRAMS = $(addprefix $(BINDIR)/time_,$(ALLOCATORS))
PRINT_PROGRAMS = $(addprefix $(BINDIR)/print_peaks_,$(ALLOCATORS))
MY_PROGRAMS = $(addprefix $(BINDIR)/my_optional_program_,$(ALLOCATORS))

all:: $(TEST_PROGRAMS) $(TIMER_PROGRAMS) $(PRINT_PROGRAMS) $(MY_PROGRAMS)

# I had to use gcc-11 on mac to be able to build this project.
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

$(OBJ): $(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TEST_PROGRAMS): $(BINDIR)/test_%:$(OBJDIR)/%.o $(DEPENDENCIES) test_harness.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TIMER_PROGRAMS): $(BINDIR)/time_%:$(OBJDIR)/%.o $(DEPENDENCIES) time_harness.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(PRINT_PROGRAMS): $(BINDIR)/print_peaks_%:$(OBJDIR)/%.o $(DEPENDENCIES) print_peaks.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MY_PROGRAMS): $(BINDIR)/my_optional_program_%:my_optional_program.c $(OBJDIR)/%.o $(DEPENDENCIES)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean::
	rm -f $(TEST_PROGRAMS) $(TIMER_PROGRAMS) $(PRINT_PROGRAMS) $(MY_PROGRAMS) $(OBJ) callgrind.out.*

.PHONY: clean all
