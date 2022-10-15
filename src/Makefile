SRCDIR = ../src
OBJDIR = ../obj
SRC := $(wildcard $(SRCDIR)/*.c)
OBJ := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o,$(SRC))

# They had us try different optimization levels here (-O0, -O2, etc).
$(OBJDIR)/list_bestfit.o: CFLAGS += -O3
$(OBJDIR)/list_addressorder.o: CFLAGS += -O3
$(OBJDIR)/list_segregated.o: CFLAGS += -Og
$(OBJDIR)/rbtree_clrs.o: CFLAGS += -O3
$(OBJDIR)/rbtree_unified.o: CFLAGS += -O3
$(OBJDIR)/rbtree_linked.o: CFLAGS += -O3
$(OBJDIR)/rbtree_stack.o: CFLAGS += -O3
$(OBJDIR)/rbtree_topdown.o: CFLAGS += -O3

# Files that set up basic heap segment and script parsing. Needed by all programs that use allocator.
DEPENDENCIES = segment.c script.c

ALLOCATORS = list_bestfit list_addressorder list_segregated rbtree_clrs rbtree_unified rbtree_stack rbtree_linked rbtree_topdown
TEST_PROGRAMS = $(addprefix $(OBJDIR)/test_,$(ALLOCATORS))
TIMER_PROGRAMS = $(addprefix $(OBJDIR)/time_,$(ALLOCATORS))
PRINT_PROGRAMS = $(addprefix $(OBJDIR)/print_peaks_,$(ALLOCATORS))
MY_PROGRAMS = $(addprefix $(OBJDIR)/my_optional_program_,$(ALLOCATORS))

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

$(TEST_PROGRAMS): $(OBJDIR)/test_%: $(OBJDIR)/%.o $(DEPENDENCIES) test_harness.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TIMER_PROGRAMS): $(OBJDIR)/time_%: $(OBJDIR)/%.o $(DEPENDENCIES) time_harness.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(PRINT_PROGRAMS): $(OBJDIR)/print_peaks_%: $(OBJDIR)/%.o $(DEPENDENCIES) print_peaks.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MY_PROGRAMS): $(OBJDIR)/my_optional_program_%: my_optional_program.c $(OBJDIR)/%.o $(DEPENDENCIES)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean::
	rm -f $(TEST_PROGRAMS) $(TIMER_PROGRAMS) $(PRINT_PROGRAMS) $(MY_PROGRAMS) $(OBJ) callgrind.out.*
