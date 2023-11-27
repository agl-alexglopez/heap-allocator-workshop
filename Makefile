.PHONY: default gcc-rel gcc-deb build format tidy clean deb-test rel-test deb-gtest rel-gtest

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

deb-test:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)
	$(BUILD_DIR)debug/test_list_segregated $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_clrs $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_unified $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_linked $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_stack $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_topdown $(TEST_ARGS)
	$(BUILD_DIR)debug/test_splaytree_stack $(TEST_ARGS)
	$(BUILD_DIR)debug/test_splaytree_topdown $(TEST_ARGS)
	@echo "Ran DEBUG Script Tests"

rel-test:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)
	$(BUILD_DIR)release/test_list_segregated $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_clrs $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_unified $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_linked $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_stack $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_topdown $(TEST_ARGS)
	$(BUILD_DIR)release/test_splaytree_stack $(TEST_ARGS)
	$(BUILD_DIR)release/test_splaytree_topdown $(TEST_ARGS)
	@echo "Ran RELEASE Script Tests"

deb-gtest:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)
	$(BUILD_DIR)debug/gtest*
	@echo "Ran DEBUG GTests"

rel-gtest:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)
	$(BUILD_DIR)debug/gtest*
	@echo "Ran RELEASE GTests"

clean:
	rm -rf build/
