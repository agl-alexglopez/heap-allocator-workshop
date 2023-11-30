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

struct malloc_expectation
{
    size_t bytes;
    status_error e;
};

constexpr size_t small_heap_size = 256;
constexpr size_t medium_heap_size = 1 << 15;
constexpr size_t max_heap_size = 1 << 30;
constexpr std::string_view red_err = "\033[38;5;9m";
constexpr std::string_view green_ok = "\033[38;5;10m";
constexpr std::string_view nil = "\033[0m";
/// This will be a semantic stand in for a free block of heap memory. We only need the falseyness
/// so nullptr might confuse people while reading a test. Use to indicate we don't know/care
/// what address the heap is using to track this free block of memory.
constexpr std::nullptr_t freed = nullptr;
/// Use this when you are done with your array of mallocs. It indicates that you expect the rest
/// of the heap that is available to be at your indicated index in the array.
constexpr size_t heap = 0;

//////////////////////////    Wrappers for Heap Allocator Calls with Checks    ///////////////////////////

void assert_init( size_t size, enum status_error e ) // NOLINT(*cognitive-complexity)
{
    void *segment = init_heap_segment( size );
    switch ( e ) {
    case OK: {
        ASSERT_NE( nullptr, segment );
        ASSERT_EQ( true, myinit( segment, size ) );
        ASSERT_EQ( true, validate_heap() );
        break;
    }
    case ER: {
        ASSERT_NE( nullptr, segment );
        ASSERT_EQ( false, myinit( segment, size ) );
        break;
    }
    default: {
        std::cerr << "malloc can only expect valid or invalid error status, not bounds error.\n";
        std::abort();
        break;
    }
    }
}

void *expect_malloc( size_t size, status_error e )
{
    void *m = mymalloc( size );
    switch ( e ) {
    case OK:
        EXPECT_NE( nullptr, m );
        break;
    case ER:
        EXPECT_EQ( nullptr, m );
        break;
    default: {
        std::cerr << "malloc can only expect valid or invalid error status, not bounds error.\n";
        std::abort();
        break;
    }
    }
    EXPECT_EQ( true, validate_heap() );
    return m;
}

void *expect_realloc( void *old_ptr, size_t new_size, enum status_error e )
{
    void *newptr = myrealloc( old_ptr, new_size );
    if ( new_size == 0 ) {
        EXPECT_EQ( nullptr, newptr );
        return nullptr;
    }
    switch ( e ) {
    case OK:
        EXPECT_NE( nullptr, newptr );
        break;
    case ER:
        EXPECT_EQ( nullptr, newptr );
        break;
    default: {
        std::cerr << "malloc can only expect valid or invalid error status, not bounds error.\n";
        std::abort();
        break;
    }
    }
    EXPECT_EQ( true, validate_heap() );
    return newptr;
}

void expect_free( void *addr )
{
    myfree( addr );
    EXPECT_EQ( true, validate_heap() );
}

void expect_state( const std::vector<heap_block> &expected )
{
    std::vector<heap_block> actual( expected.size() );
    myheap_diff( expected.data(), actual.data(), expected.size() );
    EXPECT_EQ( expected, actual );
}

void expect_frees( const std::vector<void *> &frees, const std::vector<heap_block> &expected )
{
    ASSERT_FALSE( frees.empty() );
    const size_t old_capacity = myheap_capacity();
    for ( const auto &f : frees ) {
        expect_free( f );
    }
    expect_state( expected );
    EXPECT_GT( myheap_capacity(), old_capacity );
}

/// Malloc returns void *ptr. So if you want a vector of returned pointers from calls to malloc of
/// the same type provide that type in the template and this function will cast the resulting malloc
/// to the specified type.
/// Note: Don't pass in <T*>, but rather <T> to the template. Example:
/// std::vector<char *> my_malloc_strings = expect_mallocs<char>( { { 88, OK }, { 32, OK }, { heap, OK } } );
/// but you should prefer auto to avoid confusion.
/// auto my_malloc_strings = expect_mallocs<char>( { { 88, OK }, { 32, OK }, { heap, OK } } );
template <typename T> std::vector<T *> expect_mallocs( const std::vector<malloc_expectation> &expected )
{
    EXPECT_EQ( expected.empty(), false );
    const size_t starting_capacity = myheap_capacity();
    std::vector<T *> addrs{};
    addrs.reserve( expected.size() );
    for ( const auto &e : expected ) {
        if ( e.bytes != heap ) {
            addrs.push_back( static_cast<T *>( expect_malloc( e.bytes, e.e ) ) );
        }
    }
    EXPECT_GT( starting_capacity, myheap_capacity() );
    return addrs;
}

} // namespace

//////////////////////////    GTest Needs Operators for EXPECT/ASSERT    //////////////////////////////

bool operator==( const heap_block &lhs, const heap_block &rhs )
{
    return lhs.address == rhs.address && lhs.payload_bytes == rhs.payload_bytes && lhs.err == rhs.err;
}

std::ostream &operator<<( std::ostream &os, const heap_block &b )
{
    switch ( b.err ) {
    case OK:
        os << "{ " << green_ok << b.address << ", "
           << ( b.payload_bytes == NA ? "NA" : std::to_string( b.payload_bytes ) ) << ", " << err_string[OK] << nil;
        break;
    case ER:
        os << "{ " << red_err << b.address << ", " << b.payload_bytes << ", " << err_string[ER] << nil;
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

/////////////////////////////////    Initialization Tests    ///////////////////////////////////////

TEST( InitTests, SmallInitialization )
{
    assert_init( small_heap_size, OK );
}

TEST( InitTests, MaxInitialization )
{
    assert_init( max_heap_size, OK );
}

TEST( InitTests, FailInitializationTooSmall )
{
    assert_init( 8, ER );
}

//////////////////////////////////    Malloc Tests    ///////////////////////////////////////////

TEST( MallocTests, SingleMalloc )
{
    assert_init( small_heap_size, OK );
    const size_t bytes = 32;
    static_cast<void>( expect_mallocs<void>( {
        { bytes, OK },
        { heap, OK },
    } ) );
}

TEST( MallocTests, SingleMallocGivesAdvertisedSpace )
{
    assert_init( small_heap_size, OK );
    const size_t bytes = 32;
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '@' );
    chars.back() = '\0';
    auto *request = static_cast<char *>( expect_malloc( bytes, OK ) );
    std::copy( chars.begin(), chars.end(), request );
    EXPECT_EQ( std::string_view( chars.data(), bytes ), std::string_view( request, bytes ) );
    // Now that we have copied our string into the bytes they gave us lets check the heap is not overwritten.
    expect_state( {
        { request, myheap_align( bytes ), OK },
        { freed, myheap_capacity(), OK },
    } );
}

// This test can get a little dicy because different internal schemes will have different sizes available.
// Try to pick an easy malloc amount that is obviously going to fail.
TEST( MallocTests, MallocExhaustsHeap )
{
    assert_init( 100, OK );
    const size_t bytes = 100;
    void *request3 = expect_malloc( bytes, ER );
}

TEST( MallocFreeTests, SingleMallocSingleFree )
{
    assert_init( small_heap_size, OK );
    const size_t bytes = 32;
    std::array<char, 32> chars{};
    std::iota( chars.begin(), chars.end(), '@' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    auto *request = static_cast<char *>( expect_malloc( 32, OK ) );
    std::copy( chars.begin(), chars.end(), request );
    EXPECT_EQ( std::string_view( chars.data(), bytes ), std::string_view( request, bytes ) );
    // Now that we have copied our string into the bytes they gave us lets check the heap is not overwritten.
    expect_state( {
        { request, myheap_align( bytes ), OK },
        { freed, myheap_capacity(), OK },
    } );
    expect_free( request );
    EXPECT_EQ( original_capacity, myheap_capacity() );
}

TEST( MallocFreeTests, ThreeMallocMiddleFree )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned1 = myheap_align( bytes );
    const size_t aligned2 = aligned1;
    const size_t aligned3 = aligned1;
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    const size_t remaining_mem = myheap_capacity();
    expect_frees(
        {
            alloc[1],
        },
        {
            { alloc[0], aligned1, OK },
            { freed, aligned2, OK },
            { alloc[2], aligned3, OK },
            { freed, remaining_mem, OK },
        }
    );
}

TEST( MallocFreeTests, ThreeMallocLeftEndFree )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned1 = myheap_align( bytes );
    const size_t aligned2 = aligned1;
    const size_t aligned3 = aligned1;
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    const size_t remaining_bytes = myheap_capacity();
    expect_frees(
        {
            alloc[0],
        },
        {
            { freed, aligned1, OK },
            { alloc[1], aligned2, OK },
            { alloc[2], aligned3, OK },
            { freed, remaining_bytes, OK },
        }
    );
}

///////////////////////////////////    Coalesce Tests    ////////////////////////////////////////////

TEST( CoalesceTests, CoalesceRightWithPool )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned1 = myheap_align( bytes );
    const size_t aligned2 = aligned1;
    const size_t aligned3 = aligned1;
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    expect_frees(
        {
            alloc[2],
        },
        {
            { alloc[0], aligned1, OK },
            { alloc[1], aligned2, OK },
            { freed, NA, OK },
        }
    );
}

TEST( CoalesceTests, CoalesceRightWhileSurrounded )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    const size_t remaining_bytes = myheap_capacity();
    expect_frees(
        {
            alloc[1],
        },
        {
            { alloc[0], aligned, OK },
            { freed, aligned, OK },
            { alloc[2], aligned, OK },
            { alloc[3], aligned, OK },
            { freed, remaining_bytes, OK },
        }
    );
    expect_frees(
        {
            alloc[2],
        },
        {
            { alloc[0], aligned, OK },
            { freed, NA, OK },
            { alloc[3], aligned, OK },
            { freed, remaining_bytes, OK },
        }
    );
}

TEST( CoalesceTests, CoalesceLeftHeapStart )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    const size_t remaining_bytes = myheap_capacity();
    expect_frees(
        {
            alloc[0],
        },
        {
            { freed, aligned, OK },
            { alloc[1], aligned, OK },
            { alloc[2], aligned, OK },
            { freed, remaining_bytes, OK },
        }
    );
    expect_frees(
        {
            alloc[1],
        },
        {
            { freed, NA, OK },
            { alloc[2], aligned, OK },
            { freed, NA, OK },
        }
    );
}

TEST( CoalesceTests, CoalesceLeftWhileSurrounded )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    const size_t remaining_bytes = myheap_capacity();
    expect_frees(
        {
            alloc[1],
        },
        {
            { alloc[0], aligned, OK },
            { freed, aligned, OK },
            { alloc[2], aligned, OK },
            { alloc[3], aligned, OK },
            { freed, remaining_bytes, OK },
        }
    );
    expect_frees(
        {
            alloc[2],
        },
        {
            { alloc[0], aligned, OK },
            { freed, NA, OK },
            { alloc[3], aligned, OK },
            { freed, remaining_bytes, OK },
        }
    );
}

TEST( CoalesceTests, CoalesceEntireHeap )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    const size_t remaining_bytes = myheap_capacity();
    expect_frees(
        {
            alloc[0],
        },
        {
            { freed, aligned, OK },
            { alloc[1], aligned, OK },
            { freed, remaining_bytes, OK },
        }
    );
    expect_frees(
        {
            alloc[1],
        },
        {
            { freed, NA, OK },
        }
    );
}

TEST( CoalesceTests, CoalesceLeftRightWhileSurrounded )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    const size_t remaining_bytes = myheap_capacity();
    expect_frees(
        {
            alloc[1],
            alloc[3],
        },
        {
            { alloc[0], aligned, OK },
            { freed, aligned, OK },
            { alloc[2], aligned, OK },
            { freed, aligned, OK },
            { alloc[4], aligned, OK },
            { freed, remaining_bytes, OK },
        }
    );
    expect_frees(
        {
            alloc[2],
        },
        {
            { alloc[0], aligned, OK },
            { freed, NA, OK },
            { alloc[4], aligned, OK },
            { freed, remaining_bytes, OK },
        }
    );
}

////////////////////////////////    Realloc Tests    ////////////////////////////////////////////

TEST( ReallocTests, ReallocCanMalloc )
{
    assert_init( small_heap_size, OK );
    const size_t aligned = myheap_align( 64 );
    void *req = nullptr;
    req = expect_realloc( req, aligned, OK );
    expect_state( {
        { req, aligned, OK },
        { freed, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocCanFree )
{
    assert_init( small_heap_size, OK );
    const size_t aligned = myheap_align( 64 );
    void *req = nullptr;
    req = expect_realloc( req, aligned, OK );
    expect_state( {
        { req, aligned, OK },
        { freed, myheap_capacity(), OK },
    } );
    EXPECT_EQ( expect_realloc( req, 0, OK ), nullptr );
    expect_state( {
        { freed, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocDoesNotMoveWhenShrinking )
{
    assert_init( small_heap_size, OK );
    const size_t aligned = myheap_align( 64 );
    auto alloc = expect_mallocs<void>( {
        { aligned, OK },
        { heap, OK },
    } );
    // Two different pointers are just copies pointing to same location.
    void *req = expect_realloc( alloc[0], 32, OK );
    expect_state( {
        { alloc[0], myheap_align( 32 ), OK },
        { freed, myheap_capacity(), OK },
    } );
    EXPECT_EQ( req, alloc[0] );
}

TEST( ReallocTests, ReallocDoesNotMoveWhenGrowing )
{
    assert_init( small_heap_size, OK );
    const size_t aligned = myheap_align( 64 );
    auto alloc = expect_mallocs<void>( {
        { aligned, OK },
        { heap, OK },
    } );
    void *req = expect_realloc( alloc[0], 128, OK );
    expect_state( {
        { alloc[0], myheap_align( 128 ), OK },
        { freed, myheap_capacity(), OK },
    } );
    EXPECT_EQ( req, alloc[0] );
}

TEST( ReallocTests, ReallocPrefersShortMoveEvenIfMemmoveRequired )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    expect_frees(
        {
            alloc[0],
        },
        {
            { freed, aligned, OK },
            { alloc[1], aligned, OK },
            { alloc[2], aligned, OK },
            { freed, NA, OK },
        }
    );
    void *new_addr = expect_realloc( alloc[1], aligned + aligned, OK );
    // Our new address is the old address of malloc[0] because we coalesced left and took their space.
    EXPECT_EQ( new_addr, alloc[0] );
    expect_state( {
        { new_addr, NA, OK },
        { alloc[2], NA, OK },
        { freed, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocCoalescesLeftAndRight )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    expect_frees(
        {
            alloc[0],
            alloc[2],
        },
        {
            { freed, aligned, OK },
            { alloc[1], aligned, OK },
            { freed, aligned, OK },
            { alloc[3], aligned, OK },
            { freed, NA, OK },
        }
    );
    void *new_addr = expect_realloc( alloc[1], aligned + aligned + aligned, OK );
    EXPECT_EQ( new_addr, alloc[0] );
    expect_state( {
        { new_addr, NA, OK },
        { alloc[3], aligned, OK },
        { freed, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocFindsSpaceElsewhere )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    expect_frees(
        {
            alloc[0],
            alloc[2],
        },
        {
            { freed, aligned, OK },
            { alloc[1], aligned, OK },
            { freed, aligned, OK },
            { alloc[3], aligned, OK },
            { freed, NA, OK },
        }
    );
    // We will try to coalesce but that is still not enough space so we must search elsewhere.
    const size_t new_req = aligned * 4;
    void *new_addr = expect_realloc( alloc[1], new_req, OK );
    expect_state( {
        // We always leave behinde coalesced space when possible.
        { freed, NA, OK },
        { alloc[3], aligned, OK },
        { new_addr, NA, OK },
        { freed, NA, OK },
    } );
}

TEST( ReallocTests, ReallocExhaustiveSearchFailureInPlace )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    // Upon failure NULL is returned and original memory is left intact though coalescing may have occured.
    const size_t overload_req = medium_heap_size << 1;
    void *new_addr = expect_realloc( alloc[1], overload_req, ER );
    expect_state( {
        { alloc[0], aligned, OK },
        { alloc[1], aligned, OK },
        { alloc[2], aligned, OK },
        { alloc[3], aligned, OK },
        { freed, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocFailsIdempotently )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    auto alloc = expect_mallocs<void>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    expect_frees(
        {
            alloc[0],
            alloc[2],
        },
        {
            { freed, aligned, OK },
            { alloc[1], aligned, OK },
            { freed, aligned, OK },
            { alloc[3], aligned, OK },
            { freed, NA, OK },
        }
    );
    const size_t overload_req = medium_heap_size << 1;
    void *new_addr = expect_realloc( alloc[1], overload_req, ER );
    // We should not alter anything if we fail a reallocation. The user should still have their pointer.
    expect_state( {
        { freed, aligned, OK },
        { alloc[1], aligned, OK },
        { freed, aligned, OK },
        { alloc[3], aligned, OK },
        { freed, myheap_capacity() - aligned - aligned, OK },
    } );
}

TEST( ReallocTests, ReallocFailsIdempotentlyPreservingData )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '!' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    // Fill surroundings with terminator because we want the string views to keep looking until a null is found
    // This may help us spot errors in how we move bytes around while reallocing.
    auto alloc = expect_mallocs<char>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    std::fill( alloc[0], alloc[0] + bytes, '\0' );
    std::copy( chars.begin(), chars.end(), alloc[1] );
    std::fill( alloc[2], alloc[2] + bytes, '\0' );
    std::fill( alloc[3], alloc[3] + bytes, '\0' );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( alloc[1] ) );
    expect_frees(
        {
            alloc[0],
            alloc[2],
        },
        {
            { freed, aligned, OK },
            { alloc[1], aligned, OK },
            { freed, aligned, OK },
            { alloc[3], aligned, OK },
            { freed, NA, OK },
        }
    );
    const size_t overload_req = medium_heap_size << 1;
    auto *new_addr = static_cast<char *>( expect_realloc( alloc[1], overload_req, ER ) );
    // We should not alter anything if we fail a reallocation. The user should still have their data
    expect_state( {
        { freed, aligned, OK },
        { alloc[1], aligned, OK },
        { freed, aligned, OK },
        { alloc[3], aligned, OK },
        { freed, myheap_capacity() - aligned - aligned, OK },
    } );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( alloc[1] ) );
}

TEST( ReallocTests, ReallocPreservesDataWhenCoalescingRight )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '!' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    auto alloc = expect_mallocs<char>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    // Fill surroundings with terminator because we want the string views to keep looking until a null is found
    // This may help us spot errors in how we move bytes around while reallocing.
    std::fill( alloc[0], alloc[0] + bytes, '\0' );
    std::copy( chars.begin(), chars.end(), alloc[1] );
    std::fill( alloc[2], alloc[2] + bytes, '\0' );
    std::fill( alloc[3], alloc[3] + bytes, '\0' );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( alloc[1] ) );
    expect_frees(
        {
            alloc[2],
        },
        {
            { alloc[0], aligned, OK },
            { alloc[1], aligned, OK },
            { freed, aligned, OK },
            { alloc[3], aligned, OK },
            { freed, NA, OK },
        }
    );
    auto *new_addr = static_cast<char *>( expect_realloc( alloc[1], aligned + aligned, OK ) );
    // Realloc will take the space to the right but not move the data so data should be in original state.
    // Check old pointer rather than new_addr.
    EXPECT_EQ( new_addr, alloc[1] );
    expect_state( {
        { alloc[0], aligned, OK },
        { alloc[1], NA, OK },
        { alloc[3], aligned, OK },
        { freed, NA, OK },
    } );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( alloc[1] ) );
}

TEST( ReallocTests, ReallocPreservesDataWhenCoalescingLeft )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '!' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    auto alloc = expect_mallocs<char>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    // Fill surroundings with terminator because we want the string views to keep looking until a null is found
    // This may help us spot errors in how we move bytes around while reallocing.
    std::fill( alloc[0], alloc[0] + bytes, '\0' );
    std::copy( chars.begin(), chars.end(), alloc[1] );
    std::fill( alloc[2], alloc[2] + bytes, '\0' );
    std::fill( alloc[3], alloc[3] + bytes, '\0' );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( alloc[1] ) );
    expect_frees(
        {
            alloc[0],
        },
        {
            { freed, aligned, OK },
            { alloc[1], aligned, OK },
            { alloc[2], aligned, OK },
            { alloc[3], aligned, OK },
            { freed, NA, OK },
        }
    );
    auto *new_addr = static_cast<char *>( expect_realloc( alloc[1], aligned + aligned, OK ) );
    // Realloc must move the data to the left so old pointer will not be valid. Probably memmoved.
    EXPECT_NE( new_addr, alloc[1] );
    expect_state( {
        { new_addr, NA, OK },
        { alloc[2], aligned, OK },
        { alloc[3], aligned, OK },
        { freed, NA, OK },
    } );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( new_addr ) );
}

TEST( ReallocTests, ReallocPreservesDataWhenCoalescingElsewhere )
{
    assert_init( medium_heap_size, OK );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '!' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    auto alloc = expect_mallocs<char>( {
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { bytes, OK },
        { heap, OK },
    } );
    // Fill surroundings with terminator because we want the string views to keep looking until a null is found
    // This may help us spot errors in how we move bytes around while reallocing.
    std::fill( alloc[0], alloc[0] + bytes, '\0' );
    std::copy( chars.begin(), chars.end(), alloc[1] );
    std::fill( alloc[2], alloc[2] + bytes, '\0' );
    std::fill( alloc[3], alloc[3] + bytes, '\0' );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( alloc[1] ) );
    expect_frees(
        {
            alloc[0],
            alloc[2],
        },
        {
            { freed, aligned, OK },
            { alloc[1], aligned, OK },
            { freed, aligned, OK },
            { alloc[3], aligned, OK },
            { freed, NA, OK },
        }
    );
    size_t new_req = aligned * 4;
    auto *new_addr = static_cast<char *>( expect_realloc( alloc[1], new_req, OK ) );
    // Realloc must move the data to elsewhere so old pointer will not be valid. Probably memcopy.
    EXPECT_NE( new_addr, alloc[1] );
    expect_state( {
        // Left behind space should always be coalesced to reduce fragmentation.
        { freed, NA, OK },
        { alloc[3], aligned, OK },
        { new_addr, new_req, OK },
        { freed, NA, OK },
    } );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( new_addr ) );
}
