# Tests

This file contains the internal tests for the heap allocators. Rather than write unit tests for heap allocators, I have a `test_harness.c` program that is designed to test the allocators. We use these tests to verify internals of the heap that would be hard to access through a unit testing framework. This makes development of new allocators much faster.

