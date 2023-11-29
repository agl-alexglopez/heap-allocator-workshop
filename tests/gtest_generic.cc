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

enum expect
{
    pass,
    fail
};

constexpr size_t small_heap_size = 256;
constexpr size_t medium_heap_size = 1 << 15;
constexpr size_t max_heap_size = 1 << 30;
constexpr bool a = true;
constexpr bool f = false;
constexpr std::string_view red_err = "\033[38;5;9m";
constexpr std::string_view green_ok = "\033[38;5;10m";
constexpr std::string_view nil = "\033[0m";

//////////////////////////    Wrappers for Heap Allocator Calls with Checks    ///////////////////////////

void assert_init( size_t size, expect e ) // NOLINT(*cognitive-complexity)
{
    void *segment = init_heap_segment( size );
    switch ( e ) {
    case expect::pass: {
        ASSERT_NE( nullptr, segment );
        ASSERT_EQ( true, myinit( segment, size ) );
        ASSERT_EQ( true, validate_heap() );
        break;
    }
    case expect::fail: {
        ASSERT_NE( nullptr, segment );
        ASSERT_EQ( false, myinit( segment, size ) );
        break;
    }
    }
}

void *expect_malloc( size_t size, enum expect e )
{
    void *m = mymalloc( size );
    switch ( e ) {
    case expect::pass:
        EXPECT_NE( nullptr, m );
        break;
    case expect::fail:
        EXPECT_EQ( nullptr, m );
        break;
    }
    EXPECT_EQ( true, validate_heap() );
    return m;
}

void *expect_realloc( void *old_ptr, size_t new_size, enum expect e )
{
    void *newptr = myrealloc( old_ptr, new_size );
    if ( new_size == 0 ) {
        EXPECT_EQ( nullptr, newptr );
        return nullptr;
    }
    switch ( e ) {
    case expect::pass:
        EXPECT_NE( nullptr, newptr );
        break;
    case expect::fail:
        EXPECT_EQ( nullptr, newptr );
        break;
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
    assert_init( small_heap_size, expect::pass );
}

TEST( InitTests, MaxInitialization )
{
    assert_init( max_heap_size, expect::pass );
}

TEST( InitTests, FailInitializationTooSmall )
{
    assert_init( 8, expect::fail );
}

//////////////////////////////////    Malloc Tests    ///////////////////////////////////////////

TEST( MallocTests, SingleMalloc )
{
    assert_init( small_heap_size, expect::pass );
    const size_t bytes = 32;
    void *request = expect_malloc( bytes, expect::pass );
    expect_state( {
        { request, myheap_align( bytes ), OK },
        { nullptr, myheap_capacity(), OK },
    } );
}

TEST( MallocTests, SingleMallocGivesAdvertisedSpace )
{
    assert_init( small_heap_size, expect::pass );
    const size_t bytes = 32;
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '@' );
    chars.back() = '\0';
    auto *request = static_cast<char *>( expect_malloc( bytes, expect::pass ) );
    std::copy( chars.begin(), chars.end(), request );
    EXPECT_EQ( std::string_view( chars.data(), bytes ), std::string_view( request, bytes ) );
    // Now that we have copied our string into the bytes they gave us lets check the heap is not overwritten.
    expect_state( {
        { request, myheap_align( bytes ), OK },
        { nullptr, myheap_capacity(), OK },
    } );
}

// This test can get a little dicy because different internal schemes will have different sizes.
// Try to pick the smallest size such that any reasonable allocator will exhaust after 1 malloc.
TEST( MallocTests, MallocExhaustsHeap )
{
    assert_init( 100, expect::pass );
    const size_t bytes = 100;
    void *request3 = expect_malloc( bytes, expect::fail );
}

TEST( MallocFreeTests, SingleMallocSingleFree )
{
    assert_init( small_heap_size, expect::pass );
    const size_t bytes = 32;
    std::array<char, 32> chars{};
    std::iota( chars.begin(), chars.end(), '@' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    auto *request = static_cast<char *>( expect_malloc( 32, expect::pass ) );
    std::copy( chars.begin(), chars.end(), request );
    EXPECT_EQ( std::string_view( chars.data(), bytes ), std::string_view( request, bytes ) );
    // Now that we have copied our string into the bytes they gave us lets check the heap is not overwritten.
    expect_state( {
        { request, myheap_align( bytes ), OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( request );
    EXPECT_EQ( original_capacity, myheap_capacity() );
}

TEST( MallocFreeTests, ThreeMallocMiddleFree )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned1 = myheap_align( bytes );
    const size_t aligned2 = aligned1;
    const size_t aligned3 = aligned1;
    std::array<void *, 3> mymallocs{
        expect_malloc( aligned1, expect::pass ),
        expect_malloc( aligned2, expect::pass ),
        expect_malloc( aligned3, expect::pass ),
    };
    expect_state( {
        { mymallocs[0], aligned1, OK },
        { mymallocs[1], aligned2, OK },
        { mymallocs[2], aligned3, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[1] );
    expect_state( {
        { mymallocs[0], aligned1, OK },
        { nullptr, aligned2, OK },
        { mymallocs[2], aligned3, OK },
        { nullptr, myheap_capacity() - aligned2, OK },
    } );
}

TEST( MallocFreeTests, ThreeMallocLeftEndFree )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned1 = myheap_align( bytes );
    const size_t aligned2 = aligned1;
    const size_t aligned3 = aligned1;
    std::array<void *, 3> mymallocs{
        expect_malloc( aligned1, expect::pass ),
        expect_malloc( aligned2, expect::pass ),
        expect_malloc( aligned3, expect::pass ),
    };
    expect_state( {
        { mymallocs[0], aligned1, OK },
        { mymallocs[1], aligned2, OK },
        { mymallocs[2], aligned3, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[0] );
    expect_state( {
        { nullptr, aligned1, OK },
        { mymallocs[1], aligned2, OK },
        { mymallocs[2], aligned3, OK },
        { nullptr, myheap_capacity() - aligned1, OK },
    } );
}

///////////////////////////////////    Coalesce Tests    ////////////////////////////////////////////

TEST( CoalesceTests, CoalesceRightWithPool )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned1 = myheap_align( bytes );
    const size_t aligned2 = aligned1;
    const size_t aligned3 = aligned1;
    std::array<void *, 3> mymallocs{
        expect_malloc( aligned1, expect::pass ),
        expect_malloc( aligned2, expect::pass ),
        expect_malloc( aligned3, expect::pass ),
    };
    expect_state( {
        { mymallocs[0], aligned1, OK },
        { mymallocs[1], aligned2, OK },
        { mymallocs[2], aligned3, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[2] );
    expect_state( {
        { mymallocs[0], aligned1, OK },
        { mymallocs[1], aligned2, OK },
        { nullptr, myheap_capacity(), OK },
    } );
}

TEST( CoalesceTests, CoalesceRightWhileSurrounded )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<void *, 4> mymallocs{
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
    };
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[1] );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned, OK },
    } );
    expect_free( mymallocs[2] );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { nullptr, NA, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, NA, OK },
    } );
}

TEST( CoalesceTests, CoalesceLeftHeapStart )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<void *, 3> mymallocs{
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
    };
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[0] );
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { nullptr, myheap_capacity() - aligned, OK },
    } );
    expect_free( mymallocs[1] );
    expect_state( {
        { nullptr, NA, OK },
        { mymallocs[2], aligned },
        { nullptr, NA, OK },
    } );
}

TEST( CoalesceTests, CoalesceLeftWhileSurrounded )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<void *, 4> mymallocs{
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
    };
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[1] );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned, OK },
    } );
    expect_free( mymallocs[2] );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { nullptr, NA, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, NA, OK },
    } );
}

TEST( CoalesceTests, CoalesceEntireHeap )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<void *, 2> mymallocs{
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
    };
    std::array<heap_block, 3> expected{ {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } };
    expect_free( mymallocs[0] );
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { nullptr, myheap_capacity() - aligned, OK },
    } );
    expect_free( mymallocs[1] );
    expect_state( {
        { nullptr, myheap_capacity(), OK },
    } );
}

TEST( CoalesceTests, CoalesceLeftRightWhileSurrounded )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<void *, 5> mymallocs{ {
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
    } };
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { mymallocs[4], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[1] );
    expect_free( mymallocs[3] );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[2], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[4], aligned, OK },
        { nullptr, myheap_capacity() - aligned - aligned, OK },
    } );
    expect_free( mymallocs[2] );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { nullptr, NA, OK },
        { mymallocs[4], aligned, OK },
        { nullptr, NA, OK },
    } );
}

////////////////////////////////    Realloc Tests    ////////////////////////////////////////////

TEST( ReallocTests, ReallocCanMalloc )
{
    assert_init( small_heap_size, expect::pass );
    const size_t aligned = myheap_align( 64 );
    void *req = nullptr;
    req = expect_realloc( req, aligned, expect::pass );
    expect_state( {
        { req, aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocCanFree )
{
    assert_init( small_heap_size, expect::pass );
    const size_t aligned = myheap_align( 64 );
    void *req = nullptr;
    req = expect_realloc( req, aligned, expect::pass );
    expect_state( {
        { req, aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    EXPECT_EQ( expect_realloc( req, 0, expect::pass ), nullptr );
    expect_state( {
        { nullptr, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocDoesNotMoveWhenShrinking )
{
    assert_init( small_heap_size, expect::pass );
    const size_t aligned = myheap_align( 64 );
    void *req = expect_malloc( aligned, expect::pass );
    expect_state( {
        { req, aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    req = expect_realloc( req, 32, expect::pass );
    expect_state( {
        { req, myheap_align( 32 ), OK },
        { nullptr, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocDoesNotMoveWhenGrowing )
{
    assert_init( small_heap_size, expect::pass );
    const size_t aligned = myheap_align( 64 );
    void *req = expect_malloc( aligned, expect::pass );
    expect_state( {
        { req, aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    req = expect_realloc( req, 128, expect::pass );
    expect_state( {
        { req, myheap_align( 128 ), OK },
        { nullptr, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocPrefersShortMoveEvenIfMemmoveRequired )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<void *, 3> mymallocs{ {
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
    } };
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[0] );
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { nullptr, myheap_capacity() - aligned, OK },
    } );
    void *new_addr = expect_realloc( mymallocs[1], aligned + aligned, expect::pass );
    // Our new address is the old address of malloc[0] because we coalesced left and took their space.
    EXPECT_EQ( new_addr, mymallocs[0] );
    expect_state( {
        { new_addr, NA, OK },
        { mymallocs[2], NA, OK },
        { nullptr, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocCoalescesLeftAndRight )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<void *, 5> mymallocs{ {
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
    } };
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[0] );
    expect_free( mymallocs[2] );
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned - aligned, OK },
    } );
    void *new_addr = expect_realloc( mymallocs[1], aligned + aligned + aligned, expect::pass );
    EXPECT_EQ( new_addr, mymallocs[0] );
    expect_state( {
        { new_addr, NA, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocFindsSpaceElsewhere )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<void *, 4> mymallocs{ {
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
    } };
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[0] );
    expect_free( mymallocs[2] );
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned - aligned, OK },
    } );
    // We will try to coalesce but that is still not enough space so we must search elsewhere.
    const size_t new_req = aligned * 4;
    void *new_addr = expect_realloc( mymallocs[1], new_req, expect::pass );
    expect_state( {
        { nullptr, NA, OK },
        { mymallocs[3], aligned, OK },
        { new_addr, NA, OK },
        { nullptr, NA, OK },
    } );
}

TEST( ReallocTests, ReallocExhaustiveSearchFailureInPlace )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<void *, 4> mymallocs{ {
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
    } };
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    // Upon failure NULL is returned and original memory is left intact though coalescing may have occured.
    const size_t overload_req = medium_heap_size << 1;
    void *new_addr = expect_realloc( mymallocs[1], overload_req, expect::fail );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
}

TEST( ReallocTests, ReallocFailsIdempotently )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<void *, 4> mymallocs{ {
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
        expect_malloc( aligned, expect::pass ),
    } };
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[0] );
    expect_free( mymallocs[2] );
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned - aligned, OK },
    } );
    const size_t overload_req = medium_heap_size << 1;
    void *new_addr = expect_realloc( mymallocs[1], overload_req, expect::fail );
    // We should not alter anything if we fail a reallocation. The user should still have their pointer.
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned - aligned, OK },
    } );
}

TEST( ReallocTests, ReallocFailsIdempotentlyPreservingData )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '!' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    std::array<char *, 4> mymallocs{ {
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
    } };
    // Fill surroundings with terminator because we want the string views to keep looking until a null is found
    // This may help us spot errors in how we move bytes around while reallocing.
    std::fill( mymallocs[0], mymallocs[0] + bytes, '\0' );
    std::copy( chars.begin(), chars.end(), mymallocs[1] );
    std::fill( mymallocs[2], mymallocs[2] + bytes, '\0' );
    std::fill( mymallocs[3], mymallocs[3] + bytes, '\0' );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( mymallocs[1] ) );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[0] );
    expect_free( mymallocs[2] );
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned - aligned, OK },
    } );
    const size_t overload_req = medium_heap_size << 1;
    auto *new_addr = static_cast<char *>( expect_realloc( mymallocs[1], overload_req, expect::fail ) );
    // We should not alter anything if we fail a reallocation. The user should still have their data
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned - aligned, OK },
    } );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( mymallocs[1] ) );
}

TEST( ReallocTests, ReallocPreservesDataWhenCoalescingRight )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '!' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    std::array<char *, 4> mymallocs{ {
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
    } };
    // Fill surroundings with terminator because we want the string views to keep looking until a null is found
    // This may help us spot errors in how we move bytes around while reallocing.
    std::fill( mymallocs[0], mymallocs[0] + bytes, '\0' );
    std::copy( chars.begin(), chars.end(), mymallocs[1] );
    std::fill( mymallocs[2], mymallocs[2] + bytes, '\0' );
    std::fill( mymallocs[3], mymallocs[3] + bytes, '\0' );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( mymallocs[1] ) );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[2] );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned, OK },
    } );
    auto *new_addr = static_cast<char *>( expect_realloc( mymallocs[1], aligned + aligned, expect::pass ) );
    // Realloc will take the space to the right but not move the data so data should be in original state.
    // Check old pointer rather than new_addr.
    EXPECT_EQ( new_addr, mymallocs[1] );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], NA, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, NA, OK },
    } );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( mymallocs[1] ) );
}

TEST( ReallocTests, ReallocPreservesDataWhenCoalescingLeft )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '!' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    std::array<char *, 4> mymallocs{ {
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
    } };
    // Fill surroundings with terminator because we want the string views to keep looking until a null is found
    // This may help us spot errors in how we move bytes around while reallocing.
    std::fill( mymallocs[0], mymallocs[0] + bytes, '\0' );
    std::copy( chars.begin(), chars.end(), mymallocs[1] );
    std::fill( mymallocs[2], mymallocs[2] + bytes, '\0' );
    std::fill( mymallocs[3], mymallocs[3] + bytes, '\0' );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( mymallocs[1] ) );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[0] );
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned, OK },
    } );
    auto *new_addr = static_cast<char *>( expect_realloc( mymallocs[1], aligned + aligned, expect::pass ) );
    // Realloc must move the data to the left so old pointer will not be valid. Probably memmoved.
    EXPECT_NE( new_addr, mymallocs[1] );
    expect_state( {
        { new_addr, NA, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, NA, OK },
    } );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( new_addr ) );
}

TEST( ReallocTests, ReallocPreservesDataWhenCoalescingElsewhere )
{
    assert_init( medium_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned = myheap_align( bytes );
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '!' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    std::array<char *, 4> mymallocs{ {
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
        static_cast<char *>( expect_malloc( aligned, expect::pass ) ),
    } };
    // Fill surroundings with terminator because we want the string views to keep looking until a null is found
    // This may help us spot errors in how we move bytes around while reallocing.
    std::fill( mymallocs[0], mymallocs[0] + bytes, '\0' );
    std::copy( chars.begin(), chars.end(), mymallocs[1] );
    std::fill( mymallocs[2], mymallocs[2] + bytes, '\0' );
    std::fill( mymallocs[3], mymallocs[3] + bytes, '\0' );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( mymallocs[1] ) );
    expect_state( {
        { mymallocs[0], aligned, OK },
        { mymallocs[1], aligned, OK },
        { mymallocs[2], aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity(), OK },
    } );
    expect_free( mymallocs[0] );
    expect_free( mymallocs[2] );
    expect_state( {
        { nullptr, aligned, OK },
        { mymallocs[1], aligned, OK },
        { nullptr, aligned, OK },
        { mymallocs[3], aligned, OK },
        { nullptr, myheap_capacity() - aligned - aligned, OK },
    } );
    size_t new_req = aligned * 4;
    auto *new_addr = static_cast<char *>( expect_realloc( mymallocs[1], new_req, expect::pass ) );
    // Realloc must move the data to the elsewhere so old pointer will not be valid. Probably memcopy.
    EXPECT_NE( new_addr, mymallocs[1] );
    expect_state( {
        // Left behind space should always be coalesced to reduce fragmentation.
        { nullptr, NA, OK },
        { mymallocs[3], aligned, OK },
        { new_addr, new_req, OK },
        { nullptr, NA, OK },
    } );
    EXPECT_EQ( std::string_view( chars.data() ), std::string_view( new_addr ) );
}
