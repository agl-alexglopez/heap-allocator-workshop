SRCDIR = ../src
OBJDIR = ../obj
SRC := $(wildcard $(SRCDIR)/*.c)
OBJ := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o,$(SRC))

# They had us try different optimization levels here (-O0, -O2, etc).
$(OBJDIR)/list_bestfit.o: CFLAGS += -O3
$(OBJDIR)/list_addressorder.o: CFLAGS += -O3
$(OBJDIR)/rbtree_clrs.o: CFLAGS += -O3
$(OBJDIR)/rbtree_unified.o: CFLAGS += -O3
$(OBJDIR)/rbtree_linked.o: CFLAGS += -O3
$(OBJDIR)/rbtree_stack.o: CFLAGS += -O3
$(OBJDIR)/rbtree_topdown.o: CFLAGS += -O3

ALLOCATORS = list_bestfit list_addressorder rbtree_clrs rbtree_unified rbtree_stack rbtree_linked rbtree_topdown
PROGRAMS = $(addprefix $(OBJDIR)/test_,$(ALLOCATORS))
TIMERS = $(addprefix $(OBJDIR)/time_,$(ALLOCATORS))
MY_PROGRAMS = $(addprefix $(OBJDIR)/my_optional_program_,$(ALLOCATORS))

all:: $(PROGRAMS) $(TIMERS) $(MY_PROGRAMS)

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

$(PROGRAMS): $(OBJDIR)/test_%: $(OBJDIR)/%.o segment.c test_harness.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TIMERS): $(OBJDIR)/time_%: $(OBJDIR)/%.o segment.c time_harness.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MY_PROGRAMS): $(OBJDIR)/my_optional_program_%: my_optional_program.c $(OBJDIR)/%.o segment.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean::
	rm -f $(PROGRAMS) $(TIMERS) $(MY_PROGRAMS) $(OBJ) callgrind.out.*
