.PHONY: default gcc-rel gcc-deb build format tidy clean test-deb test-rel gtest-deb gtest-rel test-deb-all test-rel-all

MAKE := $(MAKE)
# Adjust parallel build jobs based on your available cores.
JOBS ?= $(shell (command -v nproc > /dev/null 2>&1 && echo "-j$$(nproc)") || echo "")
BUILD_DIR := build/
TEST_ARGS := ./scripts/example* ./scripts/pattern* ./scripts/robust* ./scripts/trace*

default: build

gcc-rel:
	cmake --preset=gcc-release
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

gcc-deb:
	cmake --preset=gcc-debug
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

clang-rel:
	cmake --preset=clang-release
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

clang-deb:
	cmake --preset=clang-debug
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

build:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

format:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) format

tidy:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) tidy $(JOBS)

test-deb: build
	$(BUILD_DIR)debug/test_list_segregated $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_clrs $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_unified $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_linked $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_stack $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_topdown $(TEST_ARGS)
	$(BUILD_DIR)debug/test_splaytree_stack $(TEST_ARGS)
	$(BUILD_DIR)debug/test_splaytree_topdown $(TEST_ARGS)
	@echo "Ran DEBUG Script Tests"

test-rel: build
	$(BUILD_DIR)release/test_list_segregated $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_clrs $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_unified $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_linked $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_stack $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_topdown $(TEST_ARGS)
	$(BUILD_DIR)release/test_splaytree_stack $(TEST_ARGS)
	$(BUILD_DIR)release/test_splaytree_topdown $(TEST_ARGS)
	@echo "Ran RELEASE Script Tests"

gtest-deb: build
	$(BUILD_DIR)debug/gtest_generic_list_segregated
	$(BUILD_DIR)debug/gtest_generic_rbtree_clrs
	$(BUILD_DIR)debug/gtest_generic_rbtree_unified
	$(BUILD_DIR)debug/gtest_generic_rbtree_linked
	$(BUILD_DIR)debug/gtest_generic_rbtree_stack
	$(BUILD_DIR)debug/gtest_generic_rbtree_topdown
	$(BUILD_DIR)debug/gtest_generic_splaytree_topdown
	$(BUILD_DIR)debug/gtest_generic_splaytree_stack
	@echo "Ran DEBUG GTests"

gtest-rel: build
	$(BUILD_DIR)release/gtest_generic_list_segregated
	$(BUILD_DIR)release/gtest_generic_rbtree_clrs
	$(BUILD_DIR)release/gtest_generic_rbtree_unified
	$(BUILD_DIR)release/gtest_generic_rbtree_linked
	$(BUILD_DIR)release/gtest_generic_rbtree_stack
	$(BUILD_DIR)release/gtest_generic_rbtree_topdown
	$(BUILD_DIR)release/gtest_generic_splaytree_topdown
	$(BUILD_DIR)release/gtest_generic_splaytree_stack
	@echo "Ran RELEASE GTests"

test-deb-all: test-deb gtest-deb

test-rel-all: test-rel gtest-rel

clean:
	rm -rf build/
