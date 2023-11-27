#include "allocator.h"
#include "segment.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <string>
#include <vector>

namespace {

constexpr size_t small_heap_size = 256;
constexpr size_t max_heap_size = 1 << 30;
constexpr bool a = true;
constexpr bool f = false;

} // namespace

bool operator==( const heap_block &lhs, const heap_block &rhs )
{
    return lhs.allocated == rhs.allocated && lhs.payload_bytes == rhs.payload_bytes && lhs.err == rhs.err;
}

std::ostream &operator<<( std::ostream &os, const heap_block &b )
{
    os << "{ " << ( b.allocated ? "alloc" : "freed" ) << ", " << b.payload_bytes << ", ";
    switch ( b.err ) {
    case OK:
        os << "OK";
        break;
    case MISMATCH:
        os << "MISMATCH";
        break;
    case HEAP_HAS_FEWER_BLOCKS:
        os << "HEAP_HAS_FEWER_BLOCKS";
        break;
    case HEAP_HAS_MORE_BLOCKS:
        os << "HEAP_HAS_MORE_BLOCKS";
        break;
    }
    return os << " }";
}

TEST( InitTests, SmallInitialization )
{
    void *segment = init_heap_segment( small_heap_size );
    ASSERT_EQ( true, myinit( segment, small_heap_size ) );
}

TEST( InitTests, MaxInitialization )
{
    void *segment = init_heap_segment( max_heap_size );
    ASSERT_EQ( true, myinit( segment, max_heap_size ) );
}

TEST( InitTests, FailInitializationTooSmall )
{
    void *segment = init_heap_segment( 8 );
    ASSERT_EQ( false, myinit( segment, 8 ) );
}

TEST( MallocTests, SingleMalloc )
{
    const size_t bytes = 32;
    void *segment = init_heap_segment( small_heap_size );
    ASSERT_EQ( true, myinit( segment, small_heap_size ) );
    void *request = mymalloc( bytes );
    std::vector<heap_block> expected{ { a, align( bytes ), OK }, { f, capacity(), OK } };
    std::vector<heap_block> actual( expected.size() );
    validate_heap_state( expected.data(), actual.data(), expected.size() );
    EXPECT_EQ( expected, actual );
    EXPECT_EQ( validate_heap(), true );
}

TEST( MallocTests, SingleMallocGivesAdvertisedSpace )
{
    const size_t bytes = 32;
    std::vector<char> chars( bytes );
    std::iota( chars.begin(), chars.end(), '@' );
    chars.back() = '\0';
    void *segment = init_heap_segment( small_heap_size );
    ASSERT_EQ( true, myinit( segment, small_heap_size ) );
    auto *request = static_cast<char *>( mymalloc( bytes ) );
    std::copy( chars.begin(), chars.end(), request );
    EXPECT_EQ( std::string( chars.data() ), std::string( request ) );
    // Now that we have copied our string into the bytes they gave us lets check the heap is not overwritten.
    std::vector<heap_block> expected{ { a, align( bytes ), OK }, { f, capacity(), OK } };
    std::vector<heap_block> actual( expected.size() );
    validate_heap_state( expected.data(), actual.data(), expected.size() );
    EXPECT_EQ( expected, actual );
    EXPECT_EQ( validate_heap(), true );
}

TEST( MallocTests, MallocExhaustsHeap )
{
    const size_t bytes = 32;
    const size_t mini_heap = 128;
    void *segment = init_heap_segment( mini_heap );
    ASSERT_EQ( true, myinit( segment, mini_heap ) );
    void *request1 = mymalloc( bytes );
    EXPECT_EQ( validate_heap(), true );
    EXPECT_NE( request1, nullptr );
    void *request2 = mymalloc( bytes );
    EXPECT_NE( request2, nullptr );
    EXPECT_EQ( validate_heap(), true );
    void *request3 = mymalloc( bytes );
    EXPECT_EQ( request3, nullptr );
    EXPECT_EQ( validate_heap(), true );
}

TEST( MallocFreeTests, SingleMallocSingleFree )
{
    const size_t bytes = 32;
    std::vector<char> chars( bytes );
    std::iota( chars.begin(), chars.end(), '@' );
    chars.back() = '\0';
    void *segment = init_heap_segment( small_heap_size );
    ASSERT_EQ( true, myinit( segment, small_heap_size ) );
    const size_t original_capacity = capacity();
    auto *request = static_cast<char *>( mymalloc( 32 ) );
    std::copy( chars.begin(), chars.end(), request );
    EXPECT_EQ( std::string( chars.data() ), std::string( request ) );
    // Now that we have copied our string into the bytes they gave us lets check the heap is not overwritten.
    std::vector<heap_block> expected{ { a, align( bytes ), OK }, { f, capacity(), OK } };
    std::vector<heap_block> actual( expected.size() );
    validate_heap_state( expected.data(), actual.data(), expected.size() );
    EXPECT_EQ( expected, actual );
    EXPECT_EQ( validate_heap(), true );
    myfree( request );
    EXPECT_EQ( validate_heap(), true );
    EXPECT_EQ( original_capacity, capacity() );
}
