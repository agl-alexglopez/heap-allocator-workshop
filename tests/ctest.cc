/// Files: ctest.c
/// ---------------------
/// This is the Correctness Tester aka ctest.c. This program parses and runs scripts from the
/// provided location by the user. script::requests files are helpful for performing the necessary heap
/// operations to exercise code paths but not actually introducing program logic to get in the
/// way of checking such operations. This programs provides a decent amount of correctness checks
/// based on a limited api it can gain as a user of each heap allocator. However, it relies heavily
/// on thorough implementations of the validation functions written by each allocator. This framework
/// will allow the user to validate a variety of scripts with those internal debugging functions.
/// This program is written with synchronized error cout and cerr. This is so that if it is run
/// in a multithreaded or multiprocess environment error messages will not be garbled.
/// This is based upon the test_harness program written in C by jzelenski and Nick Troccoli (Winter 18-19)
/// but has been updated to C++ and modified for ease of use with the rest of my testing framework.
/// I added more information for overlapping heap boundary errors and encapsulated alloc, realloc,
/// and free logic to their own functions.
#include "allocator.h"
#include "osync.hh"
#include "script.hh"
#include "segment.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t heap_size = 1L << 32;
constexpr size_t max_threads = 20;
constexpr size_t default_workers = 4;
constexpr size_t scale_to_whole_num = 100;
constexpr size_t lowest_byte = 0xFFUL;
constexpr std::string_view ansi_bred = "\033[38;5;9m";
constexpr std::string_view ansi_bgrn = "\033[38;5;10m";
constexpr std::string_view ansi_byel = "\033[38;5;11m";
constexpr std::string_view ansi_nil = "\033[0m";
constexpr std::string_view save_cursor = "\033[s";
constexpr std::string_view restore_cursor = "\033[u";
constexpr std::string_view background_loading_bar = "▒";
constexpr std::string_view progress_bar = "█";
constexpr std::string_view pass = "●";

std::optional<size_t> eval_correctness( script::requests &s );
bool verify_block( void *ptr, size_t size, const script::requests &s );
bool verify_payload( size_t block_id, void *ptr, size_t size );
bool eval_malloc( const script::line &line, script::requests &s, void *&heap_end );
bool eval_realloc( const script::line &line, script::requests &s, void *&heap_end );
bool eval_free( const script::line &line, script::requests &s );

} // namespace

int test( std::span<const char *const> args )
{
    try {
        size_t utilization = 0;
        osync::cout( save_cursor );
        for ( size_t i = 0; i < args.size(); ++i ) {
            osync::cout( background_loading_bar, ansi_bred );
        }
        osync::cout( restore_cursor );
        for ( const auto *arg : args ) {
            std::optional<script::requests> script = script::parse_script( arg );
            if ( !script ) {
                const auto err = std::string( "Failed to parse script " ).append( arg ).append( "\n" );
                osync::cerr( err, ansi_bred );
                return 1;
            }
            const std::optional<size_t> used_segment = eval_correctness( script.value() );
            if ( !used_segment ) {
                const auto err = std::string( "Failed script " ).append( arg ).append( "\n" );
                osync::cerr( err, ansi_bred );
                return 1;
            }
            const size_t val = used_segment.value();
            if ( 0 < val ) {
                utilization += ( scale_to_whole_num * script.value().peak ) / val;
            }
            osync::cout( progress_bar, ansi_bgrn );
        }
        const auto util
            = std::string( "Utilization=" ).append( std::to_string( utilization / args.size() ) ).append( "%\n" );
        osync::cout( util, ansi_bgrn );
        return 0;
    } catch ( std::exception &e ) {
        const auto ex = std::string( "Script tester exception thrown: " ).append( e.what() ).append( "\n" );
        osync::cout( ex, ansi_bred );
        return 1;
    }
}

int main( int argc, char *argv[] )
{
    // argv[0] is always the program name so this is well defined.
    const auto args = std::span<const char *const>{ argv, static_cast<size_t>( argc ) }.subspan( 1 );
    if ( args.empty() ) {
        return 0;
    }
    return test( args );
}

namespace {

std::optional<size_t> eval_correctness( script::requests &s )
{
    init_heap_segment( heap_size );
    if ( !myinit( heap_segment_start(), heap_segment_size() ) ) {
        osync::cerr( "myinit() failed\n", ansi_bred );
        return {};
    }
    if ( !validate_heap() ) {
        osync::cerr( "validate_heap() failed\n", ansi_bred );
        return {};
    }
    void *heap_end_addr = heap_segment_start();
    size_t heap_size = 0;
    for ( const script::line &line : s.lines ) {
        switch ( line.req ) {

        case script::op::alloc: {
            heap_size += line.size;
            if ( !eval_malloc( line, s, heap_end_addr ) ) {
                const auto err = std::string( "Malloc request failure line " )
                                     .append( std::to_string( line.line ) )
                                     .append( "\n" );
                osync::cerr( err, ansi_bred );
                return {};
            }
        } break;

        case script::op::reallocd: {
            heap_size += ( line.size - s.blocks.at( line.block_index ).second );
            if ( !eval_realloc( line, s, heap_end_addr ) ) {
                const auto err = std::string( "Realloc request failure line " )
                                     .append( std::to_string( line.line ) )
                                     .append( "\n" );
                osync::cerr( err, ansi_bred );
                return {};
            }
        } break;

        case script::op::freed: {
            if ( !eval_free( line, s ) ) {
                const auto err = std::string( "Free request failure line " )
                                     .append( std::to_string( line.line ) )
                                     .append( "\n" );
                osync::cerr( err, ansi_bred );
                return {};
            }
            std::pair<void *, size_t> &old_block = s.blocks.at( line.block_index );
            heap_size -= old_block.second;
            old_block = { nullptr, 0 };
        } break;

        default: {
            osync::cerr( "Unknown request slipped through script validation\n", ansi_bred );
            return {};
        }
        }
        if ( !validate_heap() ) {
            const auto err = std::string( "validate_heap() failed request " )
                                 .append( std::to_string( line.line ) )
                                 .append( "\n" );
            osync::cerr( err, ansi_bred );
            return {};
        }
        s.peak = std::max( s.peak, heap_size );
    }
    for ( size_t block_id = 0; block_id < s.blocks.size(); ++block_id ) {
        const std::pair<void *, size_t> &block = s.blocks.at( block_id );
        if ( !verify_payload( block_id, block.first, block.second ) ) {
            osync::cerr( "Final blocks validation failed.\n", ansi_bred );
            return {};
        }
    }
    return static_cast<uint8_t *>( heap_end_addr ) - static_cast<uint8_t *>( heap_segment_start() );
}

bool eval_malloc( const script::line &line, script::requests &s, void *&heap_end )
{
    void *p = mymalloc( line.size );
    if ( nullptr == p && line.size != 0 ) {
        osync::cerr( "mymalloc() exhasted the heap\n", ansi_bred );
        return false;
    }
    if ( !verify_block( p, line.size, s ) ) {
        osync::cerr( "Block is overlapping another block causing heap corruption.\n", ansi_bred );
        return false;
    }
    const std::span<uint8_t> cur_block = std::span<uint8_t>{ static_cast<uint8_t *>( p ), line.size };
    // Specs say subspan is undefined if Offset > .size(). So this is safe: new.data() == this->data() + Offset.
    // This is basically pointer arithmetic but the kind that makes clang-tidy happy ¯\_(ツ)_/¯
    const std::span<uint8_t> end = cur_block.subspan( line.size );
    if ( end.data() > static_cast<uint8_t *>( heap_end ) ) {
        heap_end = end.data();
    }
    std::fill( cur_block.begin(), cur_block.end(), line.block_index & lowest_byte );
    s.blocks.at( line.block_index ) = { p, line.size };
    return true;
}

bool eval_realloc( const script::line &line, script::requests &s, void *&heap_end )
{
    void *old_ptr = s.blocks.at( line.block_index ).first;
    const size_t old_size = s.blocks.at( line.block_index ).second;
    if ( !verify_payload( line.block_index, old_ptr, old_size ) ) {
        return false;
    }
    void *new_ptr = myrealloc( old_ptr, line.size );
    if ( nullptr == new_ptr && line.size != 0 ) {
        osync::cerr( "Realloc exhausted the heap.\n", ansi_bred );
        return false;
    }
    s.blocks[line.block_index].second = 0;
    if ( !verify_block( new_ptr, ( old_size < line.size ? old_size : line.size ), s ) ) {
        return false;
    }
    const auto cur_block = std::span<uint8_t>{ static_cast<uint8_t *>( new_ptr ), line.size };
    const std::span<uint8_t> end = cur_block.subspan( line.size );
    if ( end.data() > static_cast<uint8_t *>( heap_end ) ) {
        heap_end = end.data();
    }
    std::fill( cur_block.begin(), cur_block.end(), line.block_index & lowest_byte );
    s.blocks.at( line.block_index ) = { new_ptr, line.size };
    return true;
}

bool eval_free( const script::line &line, script::requests &s )
{
    const std::pair<void *, size_t> old_block = s.blocks.at( line.block_index );
    if ( !verify_payload( line.block_index, old_block.first, old_block.second ) ) {
        osync::cerr( "Block corrupted before free\n", ansi_bred );
        return false;
    }
    myfree( old_block.first );
    return true;
}

bool verify_block( void *ptr, size_t size, const script::requests &s )
{
    if ( reinterpret_cast<uintptr_t>( ptr ) % ALIGNMENT != 0 ) { // NOLINT
        osync::cerr( "block is out of alignment.\n", ansi_bred );
    }
    if ( nullptr == ptr && 0 == size ) {
        return true;
    }
    auto block_end = std::span<const uint8_t>( static_cast<uint8_t *>( ptr ), size ).subspan( size );
    auto heap_end = std::span<const uint8_t>( static_cast<uint8_t *>( heap_segment_start() ), heap_segment_size() )
                        .subspan( heap_segment_size() );
    if ( ptr < heap_segment_start() ) {
        std::stringstream err{};
        err << "New block ( " << ptr << ":" << block_end.data() << ") not within heap segment ( "
            << heap_segment_start() << ":" << heap_end.data()
            << ")\n"
               "|----block-------|\n"
               "        |------heap-------...|\n";
        const std::string errstr{ err.str() };
        osync::cerr( errstr, ansi_bred );
        return false;
    }
    if ( block_end.data() > heap_end.data() ) {
        std::stringstream err{};
        err << "New block ( " << ptr << ":" << block_end.data() << ") not within heap segment ( "
            << heap_segment_start() << ":" << heap_end.data()
            << ")\n"
               "           |----block-------|\n"
               "|...------heap------|\n";
        const std::string errstr{ err.str() };
        osync::cerr( errstr, ansi_bred );
        return false;
    }
    for ( const auto &[addr, size] : s.blocks ) {
        if ( nullptr == addr || size == 0 ) {
            continue;
        }
        auto other_end = std::span<const uint8_t>( static_cast<uint8_t *>( addr ), size ).subspan( size );
        if ( ptr >= addr && ptr < other_end.data() ) {
            std::stringstream err{};
            err << "New block ( " << ptr << ":" << block_end.data() << ") overlaps existing block ( " << addr << ":"
                << other_end.data() << ")\n"
                << "     |------current---------|\n"
                   "|------other-------|\n"
                   "or\n"
                   "  |--current----|\n"
                   "|------other-------|\n";
            const std::string errstr{ err.str() };
            osync::cerr( errstr, ansi_bred );
            return false;
        }
        if ( block_end.data() > addr && block_end.data() < other_end.data() ) {
            std::stringstream err{};
            err << "New block ( " << ptr << ":" << block_end.data() << ") overlaps existing block ( " << addr << ":"
                << other_end.data() << ")\n"
                << "|---current---|\n"
                   "       |------other-------|\n"
                   "or\n"
                   "  |--current----|\n"
                   "|------other-------|\n";
            const std::string errstr{ err.str() };
            osync::cerr( errstr, ansi_bred );
            return false;
        }
        if ( ptr < addr && block_end.data() >= other_end.data() ) {
            std::stringstream err{};
            err << "New block ( " << ptr << ":" << block_end.data() << ") overlaps existing block ( " << addr << ":"
                << other_end.data() << ")\n"
                << "      |---current---|\n"
                   "|------------other------------|\n";
            const std::string errstr{ err.str() };
            osync::cerr( errstr, ansi_bred );
            return false;
        }
    }
    return true;
}

bool verify_payload( size_t block_id, void *ptr, size_t size )
{
    const uint8_t signature = block_id & lowest_byte;
    return std::ranges::all_of(
        std::span<const uint8_t>( static_cast<uint8_t *>( ptr ), size ),
        [signature]( const uint8_t byte ) { return byte == signature; }
    );
}

} // namespace
