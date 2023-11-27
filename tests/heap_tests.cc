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
    auto *request = static_cast<char *>( mymalloc( 32 ) );
    std::copy( chars.begin(), chars.end(), request );
    EXPECT_EQ( std::string( chars.data() ), std::string( request ) );
    // Now that we have copied our string into the bytes they gave us lets check the heap is not overwritten.
    std::vector<heap_block> expected{ { a, align( bytes ), OK }, { f, capacity(), OK } };
    std::vector<heap_block> actual( expected.size() );
    validate_heap_state( expected.data(), actual.data(), expected.size() );
    EXPECT_EQ( expected, actual );
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
}
