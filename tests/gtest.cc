#include "allocator.h"
#include "segment.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <numeric>
#include <string_view>

namespace {

struct Malloc_expectation {
    size_t bytes;
    Status_error e;
};

} // namespace

static constexpr size_t small_heap_size = 256;
static constexpr size_t medium_heap_size = 1 << 15;
static constexpr size_t max_heap_size = 1 << 30;
static constexpr std::string_view red_err = "\033[38;5;9m";
static constexpr std::string_view green_ok = "\033[38;5;10m";
static constexpr std::string_view nil = "\033[0m";
/// This will be a semantic stand in for a free block of heap memory. We only
/// need the falseyness so nullptr might confuse people while reading a test.
/// Use to indicate we don't know/care what address the heap is using to track
/// this free block of memory.
static constexpr std::nullptr_t freed = nullptr;
/// Use this when you are done with your array of mallocs. It indicates that you
/// expect the rest of the heap that is available to be at your indicated index
/// in the array.
static constexpr size_t heap = 0;

//////////////////////////    Wrappers for Heap Allocator Calls with Checks

static void
assert_init(size_t size, enum Status_error e) // NOLINT(*cognitive-complexity)
{
    void *segment = init_heap_segment(size);
    switch (e) {
        case OK: {
            ASSERT_NE(nullptr, segment);
            ASSERT_EQ(true, winit(segment, size));
            ASSERT_EQ(true, wvalidate_heap());
            break;
        }
        case ER: {
            ASSERT_NE(nullptr, segment);
            ASSERT_EQ(false, winit(segment, size));
            break;
        }
        default: {
            std::cerr
                << "init can only expect err or ok error status, not bounds "
                   "error.\n";
            std::abort();
            break;
        }
    }
}

static void *
expect_realloc(void *old_ptr, size_t new_size, enum Status_error e) {
    void *newptr = wrealloc(old_ptr, new_size);
    if (new_size == 0) {
        EXPECT_EQ(nullptr, newptr);
        return nullptr;
    }
    switch (e) {
        case OK:
            EXPECT_NE(nullptr, newptr);
            break;
        case ER:
            EXPECT_EQ(nullptr, newptr);
            break;
        default: {
            std::cerr << "realloc can only expect err or ok error status, not "
                         "bounds error.\n";
            std::abort();
            break;
        }
    }
    EXPECT_EQ(true, wvalidate_heap());
    return newptr;
}

static void
expect_free(void *addr) {
    wfree(addr);
    EXPECT_EQ(true, wvalidate_heap());
}

static void
expect_state(std::vector<Heap_block> const &expected) {
    std::vector<Heap_block> actual(expected.size());
    wheap_diff(expected.data(), actual.data(), expected.size());
    EXPECT_EQ(expected, actual);
}

/// Give the heap addresses that one desires to free and the expected heap
/// state and layout after those frees occur. Best used in conjunction with
/// expect malloc when setting up a heap scenario. For example:
///
///    auto alloc = expect_mallocs<void>({
///        {bytes, OK},
///        {bytes, OK},
///        {bytes, OK},
///        {heap, OK},
///    });
///    const size_t remaining_mem = wheap_capacity();
///    expect_frees(
///        {
///            alloc[1],
///        },
///        {
///            {alloc[0], aligned1, OK},
///            {freed, aligned2, OK},
///            {alloc[2], aligned3, OK},
///            {freed, remaining_mem, OK},
///        });
///
/// In the above example the address at alloc[1] is freed and the result of
/// that free is illustrated in the array below. See the tests for more.
static void
expect_frees(std::vector<void *> const &frees,
             std::vector<Heap_block> const &expected) {
    ASSERT_FALSE(frees.empty());
    size_t const old_capacity = wheap_capacity();
    for (auto const &f : frees) {
        expect_free(f);
    }
    expect_state(expected);
    EXPECT_GT(wheap_capacity(), old_capacity);
}

/// Malloc returns void *ptr. So if you want your own type from calls to malloc
/// use this instead of casting. Provide the type in the template and this
/// function will cast the resulting malloc to the specified type. Note: Don't
/// pass in <T*>, but rather <T> to the template. Example:
/// auto *my_char_array_or_string = expect_malloc<char>( 500, OK );
template <typename T>
static T *
expect_malloc(size_t size, Status_error e) {
    auto m = static_cast<T *>(wmalloc(size));
    switch (e) {
        case OK:
            EXPECT_NE(nullptr, m);
            break;
        case ER:
            EXPECT_EQ(nullptr, m);
            break;
        default: {
            std::cerr << "malloc can only expect err or ok error status, not "
                         "bounds error.\n";
            std::abort();
            break;
        }
    }
    EXPECT_EQ(true, wvalidate_heap());
    return m;
}

/// Again, same principle as normal expect_malloc but this time we are giving
/// back an array of typed pointers. Examples:
///
/// std::vector<char *> my_malloc_strings = expect_mallocs<char>({
///     {88, OK}, {32, OK}, {heap, OK}
/// });
///
/// But you should prefer auto to avoid type mismatches.
///
/// auto my_malloc_strings = expect_mallocs<char>({
///     {88, OK}, {32, OK}, {heap, OK}
/// });
template <typename T>
static std::vector<T *>
expect_mallocs(std::vector<Malloc_expectation> const &expected) {
    EXPECT_EQ(expected.empty(), false);
    size_t const starting_capacity = wheap_capacity();
    std::vector<T *> addrs{};
    addrs.reserve(expected.size());
    for (auto const &e : expected) {
        if (e.bytes != heap) {
            addrs.push_back(expect_malloc<T>(e.bytes, e.e));
        }
    }
    EXPECT_GT(starting_capacity, wheap_capacity());
    // The user claimed the rest of the heap is at the end so they do not intend
    // for the heap to exhaust.
    if (expected.back().bytes == heap) {
        EXPECT_NE(0, wheap_capacity());
    }
    return addrs;
}

//////////////////////////    GTest Needs Operators for EXPECT/ASSERT

// NOLINTBEGIN(misc-use-internal-linkage)

bool
operator==(Heap_block const &lhs, Heap_block const &rhs) {
    return lhs.address == rhs.address && lhs.payload_bytes == rhs.payload_bytes
           && lhs.err == rhs.err;
}

std::ostream &
operator<<(std::ostream &os, Heap_block const &b) {
    switch (b.err) {
        case OK:
            os << "{ " << green_ok << b.address << ", "
               << (b.payload_bytes == NA ? "NA"
                                         : std::to_string(b.payload_bytes))
               << ", " << err_string[OK] << nil;
            break;
        case ER:
            os << "{ " << red_err << b.address << ", " << b.payload_bytes
               << ", " << err_string[ER] << nil;
            break;
        case OUT_OF_BOUNDS:
            os << "{ " << red_err << err_string[OUT_OF_BOUNDS] << nil;
            break;
        case HEAP_CONTINUES:
            os << "{ " << red_err << err_string[HEAP_CONTINUES] << "..." << nil;
            break;
    }
    return os << " }";
}

// NOLINTEND(misc-use-internal-linkage)

/////////////////////////////////    Initialization Tests

TEST(InitTests, SmallInitialization) {
    assert_init(small_heap_size, OK);
}

TEST(InitTests, MaxInitialization) {
    assert_init(max_heap_size, OK);
}

TEST(InitTests, FailInitializationTooSmall) {
    assert_init(8, ER);
}

//////////////////////////////////    Malloc Tests

TEST(MallocTests, SingleMalloc) {
    assert_init(small_heap_size, OK);
    size_t const bytes = 32;
    static_cast<void>(expect_mallocs<void>({
        {bytes, OK},
        {heap, OK},
    }));
}

TEST(MallocTests, SingleMallocGivesAdvertisedSpace) {
    assert_init(small_heap_size, OK);
    size_t const bytes = 32;
    std::array<char, bytes> chars{};
    std::iota(chars.begin(), chars.end(), '@');
    chars.back() = '\0';
    auto *request = expect_malloc<char>(bytes, OK);
    std::copy(chars.begin(), chars.end(), request);
    EXPECT_EQ(std::string_view(chars.data(), bytes),
              std::string_view(request, bytes));
    // Now that we have copied our string into the bytes they gave us lets check
    // the heap is not overwritten.
    expect_state({
        {request, wheap_align(bytes), OK},
        {freed, wheap_capacity(), OK},
    });
}

// This test can get a little dicy because different internal schemes will have
// different sizes available. Try to pick an easy malloc amount that is
// obviously going to fail.
TEST(MallocTests, MallocExhaustsHeap) {
    assert_init(100, OK);
    size_t const bytes = 100;
    static_cast<void>(expect_malloc<void>(bytes, ER));
}

TEST(MallocFreeTests, SingleMallocSingleFree) {
    assert_init(small_heap_size, OK);
    size_t const bytes = 32;
    std::array<char, 32> chars{};
    std::iota(chars.begin(), chars.end(), '@');
    chars.back() = '\0';
    size_t const original_capacity = wheap_capacity();
    auto *request = expect_malloc<char>(32, OK);
    std::copy(chars.begin(), chars.end(), request);
    EXPECT_EQ(std::string_view(chars.data(), bytes),
              std::string_view(request, bytes));
    // Now that we have copied our string into the bytes they gave us lets check
    // the heap is not overwritten.
    expect_state({
        {request, wheap_align(bytes), OK},
        {freed, wheap_capacity(), OK},
    });
    expect_free(request);
    EXPECT_EQ(original_capacity, wheap_capacity());
}

TEST(MallocFreeTests, ThreeMallocMiddleFree) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned1 = wheap_align(bytes);
    size_t const aligned2 = aligned1;
    size_t const aligned3 = aligned1;
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    size_t const remaining_mem = wheap_capacity();
    expect_frees(
        {
            alloc[1],
        },
        {
            {alloc[0], aligned1, OK},
            {freed, aligned2, OK},
            {alloc[2], aligned3, OK},
            {freed, remaining_mem, OK},
        });
}

TEST(MallocFreeTests, ThreeMallocLeftEndFree) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned1 = wheap_align(bytes);
    size_t const aligned2 = aligned1;
    size_t const aligned3 = aligned1;
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    size_t const remaining_bytes = wheap_capacity();
    expect_frees(
        {
            alloc[0],
        },
        {
            {freed, aligned1, OK},
            {alloc[1], aligned2, OK},
            {alloc[2], aligned3, OK},
            {freed, remaining_bytes, OK},
        });
}

///////////////////////////////////    Coalesce Tests

TEST(CoalesceTests, CoalesceRightWithPool) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned1 = wheap_align(bytes);
    size_t const aligned2 = aligned1;
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    expect_frees(
        {
            alloc[2],
        },
        {
            {alloc[0], aligned1, OK},
            {alloc[1], aligned2, OK},
            {freed, NA, OK},
        });
}

TEST(CoalesceTests, CoalesceRightWhileSurrounded) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    size_t const remaining_bytes = wheap_capacity();
    expect_frees(
        {
            alloc[1],
        },
        {
            {alloc[0], aligned, OK},
            {freed, aligned, OK},
            {alloc[2], aligned, OK},
            {alloc[3], aligned, OK},
            {freed, remaining_bytes, OK},
        });
    expect_frees(
        {
            alloc[2],
        },
        {
            {alloc[0], aligned, OK},
            {freed, NA, OK},
            {alloc[3], aligned, OK},
            {freed, remaining_bytes, OK},
        });
}

TEST(CoalesceTests, CoalesceLeftHeapStart) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    size_t const remaining_bytes = wheap_capacity();
    expect_frees(
        {
            alloc[0],
        },
        {
            {freed, aligned, OK},
            {alloc[1], aligned, OK},
            {alloc[2], aligned, OK},
            {freed, remaining_bytes, OK},
        });
    expect_frees(
        {
            alloc[1],
        },
        {
            {freed, NA, OK},
            {alloc[2], aligned, OK},
            {freed, NA, OK},
        });
}

TEST(CoalesceTests, CoalesceLeftWhileSurrounded) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    size_t const remaining_bytes = wheap_capacity();
    expect_frees(
        {
            alloc[1],
        },
        {
            {alloc[0], aligned, OK},
            {freed, aligned, OK},
            {alloc[2], aligned, OK},
            {alloc[3], aligned, OK},
            {freed, remaining_bytes, OK},
        });
    expect_frees(
        {
            alloc[2],
        },
        {
            {alloc[0], aligned, OK},
            {freed, NA, OK},
            {alloc[3], aligned, OK},
            {freed, remaining_bytes, OK},
        });
}

TEST(CoalesceTests, CoalesceEntireHeap) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    size_t const remaining_bytes = wheap_capacity();
    expect_frees(
        {
            alloc[0],
        },
        {
            {freed, aligned, OK},
            {alloc[1], aligned, OK},
            {freed, remaining_bytes, OK},
        });
    expect_frees(
        {
            alloc[1],
        },
        {
            {freed, NA, OK},
        });
}

TEST(CoalesceTests, CoalesceLeftRightWhileSurrounded) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    size_t const remaining_bytes = wheap_capacity();
    expect_frees(
        {
            alloc[1],
            alloc[3],
        },
        {
            {alloc[0], aligned, OK},
            {freed, aligned, OK},
            {alloc[2], aligned, OK},
            {freed, aligned, OK},
            {alloc[4], aligned, OK},
            {freed, remaining_bytes, OK},
        });
    expect_frees(
        {
            alloc[2],
        },
        {
            {alloc[0], aligned, OK},
            {freed, NA, OK},
            {alloc[4], aligned, OK},
            {freed, remaining_bytes, OK},
        });
}

////////////////////////////////    Realloc Tests

TEST(ReallocTests, ReallocCanMalloc) {
    assert_init(small_heap_size, OK);
    size_t const aligned = wheap_align(64);
    void *req = nullptr;
    req = expect_realloc(req, aligned, OK);
    expect_state({
        {req, aligned, OK},
        {freed, wheap_capacity(), OK},
    });
}

TEST(ReallocTests, ReallocCanFree) {
    assert_init(small_heap_size, OK);
    size_t const aligned = wheap_align(64);
    void *req = nullptr;
    req = expect_realloc(req, aligned, OK);
    expect_state({
        {req, aligned, OK},
        {freed, wheap_capacity(), OK},
    });
    EXPECT_EQ(expect_realloc(req, 0, OK), nullptr);
    expect_state({
        {freed, wheap_capacity(), OK},
    });
}

TEST(ReallocTests, ReallocDoesNotMoveWhenShrinking) {
    assert_init(small_heap_size, OK);
    size_t const aligned = wheap_align(64);
    auto alloc = expect_mallocs<void>({
        {aligned, OK},
        {heap, OK},
    });
    void *req = expect_realloc(alloc[0], 32, OK);
    expect_state({
        {alloc[0], wheap_align(32), OK},
        {freed, wheap_capacity(), OK},
    });
    EXPECT_EQ(req, alloc[0]);
}

TEST(ReallocTests, ReallocDoesNotMoveWhenGrowing) {
    assert_init(small_heap_size, OK);
    size_t const aligned = wheap_align(64);
    auto alloc = expect_mallocs<void>({
        {aligned, OK},
        {heap, OK},
    });
    void *req = expect_realloc(alloc[0], 128, OK);
    expect_state({
        {alloc[0], wheap_align(128), OK},
        {freed, wheap_capacity(), OK},
    });
    EXPECT_EQ(req, alloc[0]);
}

TEST(ReallocTests, ReallocPrefersShortMoveEvenIfMemmoveRequired) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    expect_frees(
        {
            alloc[0],
        },
        {
            {freed, aligned, OK},
            {alloc[1], aligned, OK},
            {alloc[2], aligned, OK},
            {freed, NA, OK},
        });
    void *new_addr = expect_realloc(alloc[1], aligned + aligned, OK);
    // Our new address is the old address of malloc[0] because we coalesced left
    // and took their space.
    EXPECT_EQ(new_addr, alloc[0]);
    expect_state({
        {new_addr, NA, OK},
        {alloc[2], NA, OK},
        {freed, wheap_capacity(), OK},
    });
}

TEST(ReallocTests, ReallocCoalescesLeftAndRight) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    expect_frees(
        {
            alloc[0],
            alloc[2],
        },
        {
            {freed, aligned, OK},
            {alloc[1], aligned, OK},
            {freed, aligned, OK},
            {alloc[3], aligned, OK},
            {freed, NA, OK},
        });
    void *new_addr = expect_realloc(alloc[1], aligned + aligned + aligned, OK);
    EXPECT_EQ(new_addr, alloc[0]);
    expect_state({
        {new_addr, NA, OK},
        {alloc[3], aligned, OK},
        {freed, wheap_capacity(), OK},
    });
}

TEST(ReallocTests, ReallocFindsSpaceElsewhere) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    expect_frees(
        {
            alloc[0],
            alloc[2],
        },
        {
            {freed, aligned, OK},
            {alloc[1], aligned, OK},
            {freed, aligned, OK},
            {alloc[3], aligned, OK},
            {freed, NA, OK},
        });
    // We will try to coalesce but that is still not enough space so we must
    // search elsewhere.
    size_t const new_req = aligned * 4;
    void *new_addr = expect_realloc(alloc[1], new_req, OK);
    expect_state({
        // We always leave behinde coalesced space when possible.
        {freed, NA, OK},
        {alloc[3], aligned, OK},
        {new_addr, NA, OK},
        {freed, NA, OK},
    });
}

TEST(ReallocTests, ReallocExhaustiveSearchFailureInPlace) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    // Upon failure NULL is returned and original memory is left intact though
    // coalescing may have occured.
    size_t const overload_req = medium_heap_size << 1;
    void *new_addr = expect_realloc(alloc[1], overload_req, ER);
    EXPECT_EQ(new_addr, nullptr);
    expect_state({
        {alloc[0], aligned, OK},
        {alloc[1], aligned, OK},
        {alloc[2], aligned, OK},
        {alloc[3], aligned, OK},
        {freed, wheap_capacity(), OK},
    });
}

TEST(ReallocTests, ReallocFailsIdempotently) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    auto alloc = expect_mallocs<void>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    expect_frees(
        {
            alloc[0],
            alloc[2],
        },
        {
            {freed, aligned, OK},
            {alloc[1], aligned, OK},
            {freed, aligned, OK},
            {alloc[3], aligned, OK},
            {freed, NA, OK},
        });
    size_t const overload_req = medium_heap_size << 1;
    void *new_addr = expect_realloc(alloc[1], overload_req, ER);
    // We should not alter anything if we fail a reallocation. The user should
    // still have their old pointer.
    EXPECT_EQ(new_addr, nullptr);
    expect_state({
        {freed, aligned, OK},
        {alloc[1], aligned, OK},
        {freed, aligned, OK},
        {alloc[3], aligned, OK},
        {freed, wheap_capacity() - aligned - aligned, OK},
    });
}

TEST(ReallocTests, ReallocFailsIdempotentlyPreservingData) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    std::array<char, bytes> chars{};
    std::iota(chars.begin(), chars.end(), '!');
    chars.back() = '\0';
    // Fill surroundings with terminator because we want the string views to
    // keep looking until a null is found This may help us spot errors in how we
    // move bytes around while reallocing.
    auto alloc = expect_mallocs<char>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    std::fill(alloc[0], alloc[0] + bytes, '\0');
    std::copy(chars.begin(), chars.end(), alloc[1]);
    std::fill(alloc[2], alloc[2] + bytes, '\0');
    std::fill(alloc[3], alloc[3] + bytes, '\0');
    EXPECT_EQ(std::string_view(chars.data()), std::string_view(alloc[1]));
    expect_frees(
        {
            alloc[0],
            alloc[2],
        },
        {
            {freed, aligned, OK},
            {alloc[1], aligned, OK},
            {freed, aligned, OK},
            {alloc[3], aligned, OK},
            {freed, NA, OK},
        });
    size_t const overload_req = medium_heap_size << 1;
    auto *new_addr
        = static_cast<char *>(expect_realloc(alloc[1], overload_req, ER));
    // We should not alter anything if we fail a reallocation. The user should
    // still have their data
    EXPECT_EQ(new_addr, nullptr);
    expect_state({
        {freed, aligned, OK},
        {alloc[1], aligned, OK},
        {freed, aligned, OK},
        {alloc[3], aligned, OK},
        {freed, wheap_capacity() - aligned - aligned, OK},
    });
    EXPECT_EQ(std::string_view(chars.data()), std::string_view(alloc[1]));
}

TEST(ReallocTests, ReallocPreservesDataWhenCoalescingRight) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    std::array<char, bytes> chars{};
    std::iota(chars.begin(), chars.end(), '!');
    chars.back() = '\0';
    auto alloc = expect_mallocs<char>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    std::fill(alloc[0], alloc[0] + bytes, '\0');
    std::copy(chars.begin(), chars.end(), alloc[1]);
    std::fill(alloc[2], alloc[2] + bytes, '\0');
    std::fill(alloc[3], alloc[3] + bytes, '\0');
    EXPECT_EQ(std::string_view(chars.data()), std::string_view(alloc[1]));
    expect_frees(
        {
            alloc[2],
        },
        {
            {alloc[0], aligned, OK},
            {alloc[1], aligned, OK},
            {freed, aligned, OK},
            {alloc[3], aligned, OK},
            {freed, NA, OK},
        });
    auto *new_addr
        = static_cast<char *>(expect_realloc(alloc[1], aligned + aligned, OK));
    // Realloc will take the space to the right but not move the data so data
    // should be in original state. Check old pointer rather than new_addr.
    EXPECT_EQ(new_addr, alloc[1]);
    expect_state({
        {alloc[0], aligned, OK},
        {alloc[1], NA, OK},
        {alloc[3], aligned, OK},
        {freed, NA, OK},
    });
    EXPECT_EQ(std::string_view(chars.data()), std::string_view(alloc[1]));
}

TEST(ReallocTests, ReallocPreservesDataWhenCoalescingLeft) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    std::array<char, bytes> chars{};
    std::iota(chars.begin(), chars.end(), '!');
    chars.back() = '\0';
    auto alloc = expect_mallocs<char>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    std::fill(alloc[0], alloc[0] + bytes, '\0');
    std::copy(chars.begin(), chars.end(), alloc[1]);
    std::fill(alloc[2], alloc[2] + bytes, '\0');
    std::fill(alloc[3], alloc[3] + bytes, '\0');
    EXPECT_EQ(std::string_view(chars.data()), std::string_view(alloc[1]));
    expect_frees(
        {
            alloc[0],
        },
        {
            {freed, aligned, OK},
            {alloc[1], aligned, OK},
            {alloc[2], aligned, OK},
            {alloc[3], aligned, OK},
            {freed, NA, OK},
        });
    auto *new_addr
        = static_cast<char *>(expect_realloc(alloc[1], aligned + aligned, OK));
    // Realloc must move the data to the left so old pointer will not be valid.
    // Probably memmoved.
    EXPECT_NE(new_addr, alloc[1]);
    expect_state({
        {new_addr, NA, OK},
        {alloc[2], aligned, OK},
        {alloc[3], aligned, OK},
        {freed, NA, OK},
    });
    EXPECT_EQ(std::string_view(chars.data()), std::string_view(new_addr));
}

TEST(ReallocTests, ReallocPreservesDataWhenCoalescingElsewhere) {
    assert_init(medium_heap_size, OK);
    size_t const bytes = 64;
    size_t const aligned = wheap_align(bytes);
    std::array<char, bytes> chars{};
    std::iota(chars.begin(), chars.end(), '!');
    chars.back() = '\0';
    auto alloc = expect_mallocs<char>({
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {bytes, OK},
        {heap, OK},
    });
    std::fill(alloc[0], alloc[0] + bytes, '\0');
    std::copy(chars.begin(), chars.end(), alloc[1]);
    std::fill(alloc[2], alloc[2] + bytes, '\0');
    std::fill(alloc[3], alloc[3] + bytes, '\0');
    EXPECT_EQ(std::string_view(chars.data()), std::string_view(alloc[1]));
    expect_frees(
        {
            alloc[0],
            alloc[2],
        },
        {
            {freed, aligned, OK},
            {alloc[1], aligned, OK},
            {freed, aligned, OK},
            {alloc[3], aligned, OK},
            {freed, NA, OK},
        });
    size_t new_req = aligned * 4;
    auto *new_addr = static_cast<char *>(expect_realloc(alloc[1], new_req, OK));
    // Realloc must move the data to elsewhere so old pointer will not be valid.
    // Probably memcopy.
    EXPECT_NE(new_addr, alloc[1]);
    expect_state({
        // Left behind space should always be coalesced to reduce fragmentation.
        {freed, NA, OK},
        {alloc[3], aligned, OK},
        {new_addr, new_req, OK},
        {freed, NA, OK},
    });
    EXPECT_EQ(std::string_view(chars.data()), std::string_view(new_addr));
}
