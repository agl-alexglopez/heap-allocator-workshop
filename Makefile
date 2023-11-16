.PHONY: default release debug build format tidy clean

MAKE := $(MAKE)
# Adjust parallel build jobs based on your available cores.
JOBS ?= $(shell (command -v nproc > /dev/null 2>&1 && nproc) || echo 1)

default: build

release:
	@cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release
	@$(MAKE) --no-print-directory -C build/ -j$(JOBS)

debug:
	@cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Debug
	@$(MAKE) --no-print-directory -C build/ -j$(JOBS)

build:
	@$(MAKE) --no-print-directory -C build/ -j$(JOBS)

format:
	@$(MAKE) --no-print-directory -C build/ format

tidy:
	@$(MAKE) --no-print-directory -C build/ tidy

clean:
	rm -rf build/
