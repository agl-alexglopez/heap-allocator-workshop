#include "allocator.h"
#include "segment.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <string>

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

void expect_init( size_t size, expect e ) // NOLINT(*cognitive-complexity)
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

} // namespace

bool operator==( const heap_block &lhs, const heap_block &rhs )
{
    return lhs.allocated == rhs.allocated && lhs.payload_bytes == rhs.payload_bytes && lhs.err == rhs.err;
}

std::ostream &operator<<( std::ostream &os, const heap_block &b )
{
    switch ( b.err ) {
    case OK:
        os << "{ " << ( b.allocated ? "a" : "f" ) << ", " << b.payload_bytes << ", " << err_string[OK];
        break;
    case MISMATCH:
        os << "{ " << ( b.allocated ? "a" : "f" ) << ", " << b.payload_bytes << ", " << err_string[MISMATCH];
        break;
    case OUT_OF_BOUNDS:
        os << "{ " << err_string[OUT_OF_BOUNDS];
        break;
    case HEAP_CONTINUES:
        os << "{ " << err_string[HEAP_CONTINUES] << "...";
        break;
    }
    return os << " }";
}

TEST( InitTests, SmallInitialization ) { expect_init( small_heap_size, expect::pass ); }

TEST( InitTests, MaxInitialization ) { expect_init( max_heap_size, expect::pass ); }

TEST( InitTests, FailInitializationTooSmall ) { expect_init( 8, expect::fail ); }

TEST( MallocTests, SingleMalloc )
{
    expect_init( small_heap_size, expect::pass );
    const size_t bytes = 32;
    void *request = expect_malloc( bytes, expect::pass );
    std::array<heap_block, 2> expected{ { { a, myheap_align( bytes ), OK }, { f, myheap_capacity(), OK } } };
    std::array<heap_block, 2> actual{};
    myheap_state( expected.data(), actual.data(), expected.size() );
    EXPECT_EQ( expected, actual );
}

TEST( MallocTests, SingleMallocGivesAdvertisedSpace )
{
    expect_init( small_heap_size, expect::pass );
    const size_t bytes = 32;
    std::array<char, bytes> chars{};
    std::iota( chars.begin(), chars.end(), '@' );
    chars.back() = '\0';
    auto *request = static_cast<char *>( expect_malloc( bytes, expect::pass ) );
    std::copy( chars.begin(), chars.end(), request );
    EXPECT_EQ( std::string( chars.data() ), std::string( request ) );
    // Now that we have copied our string into the bytes they gave us lets check the heap is not overwritten.
    std::array<heap_block, 2> expected{ { { a, myheap_align( bytes ), OK }, { f, myheap_capacity(), OK } } };
    std::array<heap_block, 2> actual{};
    myheap_state( expected.data(), actual.data(), expected.size() );
    EXPECT_EQ( expected, actual );
}

TEST( MallocTests, MallocExhaustsHeap )
{
    expect_init( 128, expect::pass );
    const size_t bytes = 32;
    void *request1 = expect_malloc( bytes, expect::pass );
    void *request2 = expect_malloc( bytes, expect::pass );
    void *request3 = expect_malloc( bytes, expect::fail );
}

TEST( MallocFreeTests, SingleMallocSingleFree )
{
    expect_init( small_heap_size, expect::pass );
    const size_t bytes = 32;
    std::array<char, 32> chars{};
    std::iota( chars.begin(), chars.end(), '@' );
    chars.back() = '\0';
    const size_t original_capacity = myheap_capacity();
    auto *request = static_cast<char *>( expect_malloc( 32, expect::pass ) );
    std::copy( chars.begin(), chars.end(), request );
    EXPECT_EQ( std::string( chars.data() ), std::string( request ) );
    // Now that we have copied our string into the bytes they gave us lets check the heap is not overwritten.
    std::array<heap_block, 2> expected{ { { a, myheap_align( bytes ), OK }, { f, myheap_capacity(), OK } } };
    std::array<heap_block, 2> actual{};
    myheap_state( expected.data(), actual.data(), expected.size() );
    EXPECT_EQ( expected, actual );
    EXPECT_EQ( validate_heap(), true );
    myfree( request );
    EXPECT_EQ( validate_heap(), true );
    EXPECT_EQ( original_capacity, myheap_capacity() );
}

TEST( MallocFreeTests, ThreeMallocMiddleFree )
{
    expect_init( small_heap_size, expect::pass );
    const size_t bytes = 64;
    const size_t aligned1 = myheap_align( bytes );
    const size_t aligned2 = aligned1;
    const size_t aligned3 = aligned1;
    std::array<void *, 3> mymallocs{ expect_malloc( aligned1, expect::pass ),
                                     expect_malloc( aligned2, expect::pass ),
                                     expect_malloc( aligned3, expect::pass ) };
    std::array<heap_block, 4> expected{
        { { a, aligned1, OK }, { a, aligned2, OK }, { a, aligned3, OK }, { f, myheap_capacity(), OK } } };
    std::array<heap_block, 4> actual{};
    myheap_state( expected.data(), actual.data(), expected.size() );
    EXPECT_EQ( expected, actual );
    myfree( mymallocs[1] );
    EXPECT_EQ( true, validate_heap() );
    expected = { { { a, aligned1, OK },
                   { f, aligned2, OK },
                   { a, aligned3, OK },
                   { f, myheap_capacity() - aligned2, OK } } };
    myheap_state( expected.data(), actual.data(), expected.size() );
    EXPECT_EQ( expected, actual );
}
