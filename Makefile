.PHONY: default gcc-rel gcc-deb build format tidy clean ctest-deb ctest-rel gtest-deb gtest-rel test-deb-all test-rel-all

MAKE := $(MAKE)
# Adjust parallel build jobs based on your available cores.
JOBS ?= $(shell (command -v nproc > /dev/null 2>&1 && echo "-j$$(nproc)") || echo "")
BUILD_DIR := build/
TEST_ARGS := ./scripts/example* ./scripts/pattern* ./scripts/robust* ./scripts/trace*

default: build

gcc-rel:
	cmake --preset=gcc-rel
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

gcc-deb:
	cmake --preset=gcc-deb
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

clang-rel:
	cmake --preset=clang-rel
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

clang-deb:
	cmake --preset=clang-deb
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

build:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

format:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) format

tidy:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) tidy $(JOBS)

ctest-deb: build
	$(BUILD_DIR)deb/ctest_list_segregated $(TEST_ARGS)
	$(BUILD_DIR)deb/ctest_rbtree_clrs $(TEST_ARGS)
	$(BUILD_DIR)deb/ctest_rbtree_unified $(TEST_ARGS)
	$(BUILD_DIR)deb/ctest_rbtree_linked $(TEST_ARGS)
	$(BUILD_DIR)deb/ctest_rbtree_stack $(TEST_ARGS)
	$(BUILD_DIR)deb/ctest_rbtree_topdown $(TEST_ARGS)
	$(BUILD_DIR)deb/ctest_splaytree_stack $(TEST_ARGS)
	$(BUILD_DIR)deb/ctest_splaytree_topdown $(TEST_ARGS)
	@echo "Ran DEBUG Script Correctness Tests"

ctest-rel: build
	$(BUILD_DIR)rel/ctest_list_segregated $(TEST_ARGS)
	$(BUILD_DIR)rel/ctest_rbtree_clrs $(TEST_ARGS)
	$(BUILD_DIR)rel/ctest_rbtree_unified $(TEST_ARGS)
	$(BUILD_DIR)rel/ctest_rbtree_linked $(TEST_ARGS)
	$(BUILD_DIR)rel/ctest_rbtree_stack $(TEST_ARGS)
	$(BUILD_DIR)rel/ctest_rbtree_topdown $(TEST_ARGS)
	$(BUILD_DIR)rel/ctest_splaytree_stack $(TEST_ARGS)
	$(BUILD_DIR)rel/ctest_splaytree_topdown $(TEST_ARGS)
	@echo "Ran RELEASE Script Correctness Tests"

gtest-deb: build
	$(BUILD_DIR)deb/gtest_generic_list_segregated
	$(BUILD_DIR)deb/gtest_generic_rbtree_clrs
	$(BUILD_DIR)deb/gtest_generic_rbtree_unified
	$(BUILD_DIR)deb/gtest_generic_rbtree_linked
	$(BUILD_DIR)deb/gtest_generic_rbtree_stack
	$(BUILD_DIR)deb/gtest_generic_rbtree_topdown
	$(BUILD_DIR)deb/gtest_generic_splaytree_topdown
	$(BUILD_DIR)deb/gtest_generic_splaytree_stack
	@echo "Ran DEBUG GTests"

gtest-rel: build
	$(BUILD_DIR)rel/gtest_generic_list_segregated
	$(BUILD_DIR)rel/gtest_generic_rbtree_clrs
	$(BUILD_DIR)rel/gtest_generic_rbtree_unified
	$(BUILD_DIR)rel/gtest_generic_rbtree_linked
	$(BUILD_DIR)rel/gtest_generic_rbtree_stack
	$(BUILD_DIR)rel/gtest_generic_rbtree_topdown
	$(BUILD_DIR)rel/gtest_generic_splaytree_topdown
	$(BUILD_DIR)rel/gtest_generic_splaytree_stack
	@echo "Ran RELEASE GTests"

test-deb-all: test-deb gtest-deb

test-rel-all: test-rel gtest-rel

clean:
	rm -rf build/
