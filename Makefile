.PHONY: default gcc-release gcc-debug build format tidy clean debug-tests release-tests

MAKE := $(MAKE)
# Adjust parallel build jobs based on your available cores.
JOBS ?= $(shell (command -v nproc > /dev/null 2>&1 && echo "-j$$(nproc)") || echo "")
BUILD_DIR := build/
TEST_ARGS := ./scripts/example* ./scripts/pattern* ./scripts/robust* ./scripts/trace*

default: build

gcc-release:
	cmake --preset=gcc-release
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

gcc-debug:
	cmake --preset=gcc-debug
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

clang-release:
	cmake --preset=clang-release
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

clang-debug:
	cmake --preset=clang-debug
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

build:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)

format:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) format

tidy:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) tidy

debug-tests:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)
	$(BUILD_DIR)debug/test_list_addressorder $(TEST_ARGS)
	$(BUILD_DIR)debug/test_list_bestfit $(TEST_ARGS)
	$(BUILD_DIR)debug/test_list_segregated $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_clrs $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_unified $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_linked $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_stack $(TEST_ARGS)
	$(BUILD_DIR)debug/test_rbtree_topdown $(TEST_ARGS)

release-tests:
	$(MAKE) --no-print-directory -C $(BUILD_DIR) $(JOBS)
	$(BUILD_DIR)release/test_list_addressorder $(TEST_ARGS)
	$(BUILD_DIR)release/test_list_bestfit $(TEST_ARGS)
	$(BUILD_DIR)release/test_list_segregated $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_clrs $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_unified $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_linked $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_stack $(TEST_ARGS)
	$(BUILD_DIR)release/test_rbtree_topdown $(TEST_ARGS)

clean:
	rm -rf build/
