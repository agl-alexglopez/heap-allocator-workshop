#include "allocator.h"
#include "segment.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <syncstream>
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
constexpr std::string_view circle = "●";

enum heap_request
{
    empty = 0,
    alloc,
    realloc,
    free,
};

struct script_line
{
    heap_request req;
    size_t block_index;
    size_t size;
    size_t line;
};

struct script
{
    std::string title;
    std::vector<script_line> lines;
    std::vector<std::pair<void *, size_t>> blocks;
    size_t peak;
};

std::optional<script> parse_script( const std::string &filepath );
std::optional<script_line> tokens_pass( std::span<std::string> toks );
std::optional<size_t> eval_correctness( script &s );
bool verify_block( void *ptr, size_t size, const script &s );
bool verify_payload( size_t block_id, void *ptr, size_t size );
bool eval_malloc( const script_line &line, script &s, void *&heap_end );
bool eval_realloc( const script_line &line, script &s, void *&heap_end );
bool eval_free( const script_line &line, script &s );
void cerr_sync( std::string_view s, std::string_view color );
void cerr_sync( std::string_view s );
void cout_sync( std::string_view s, std::string_view color );
void cout_sync( std::string_view s );

} // namespace

int main( int argc, char *argv[] )
{
    // Safe to take subspan here because argv always starts with program name and is size 1.
    size_t success_count = 0;
    size_t fail_count = 0;
    size_t utilization = 0;
    for ( const auto *arg : std::span<const char *const>( argv, argc ).subspan( 1 ) ) {
        std::optional<script> script = parse_script( arg );
        if ( !script ) {
            std::string err = std::string( "Failed to parse script " ).append( arg ).append( "\n" );
            cerr_sync( err, ansi_bred );
            ++fail_count;
            continue;
        }
        std::optional<size_t> used_segment = eval_correctness( script.value() );
        if ( !used_segment ) {
            std::string err = std::string( "Failed script " ).append( arg ).append( "\n" );
            cerr_sync( err, ansi_bred );
            ++fail_count;
            continue;
        }
        size_t val = used_segment.value();
        if ( val > 0 ) {
            utilization += ( scale_to_whole_num * script.value().peak ) / val;
        }
        cout_sync( circle, ansi_bgrn );
        ++success_count;
    }
    cout_sync( "\n" );
    return static_cast<int>( fail_count );
}

namespace {

std::optional<size_t> eval_correctness( script &s )
{
    init_heap_segment( heap_size );
    if ( !myinit( heap_segment_start(), heap_segment_size() ) ) {
        cerr_sync( "myinit() failed", ansi_bred );
        return {};
    }
    if ( !validate_heap() ) {
        cerr_sync( "validate_heap() failed", ansi_bred );
        return {};
    }
    void *heap_end_addr = heap_segment_start();
    size_t heap_size = 0;
    for ( script_line &line : s.lines ) {
        const size_t id = line.block_index;
        const size_t requested_size = line.size;
        switch ( line.req ) {
        case heap_request::alloc: {
            heap_size += requested_size;
            if ( !eval_malloc( line, s, heap_end_addr ) ) {
                std::string err = std::string( "Malloc request failure line " )
                                      .append( std::to_string( line.line ) )
                                      .append( "\n" );
                cerr_sync( err, ansi_bred );
                return {};
            }
        } break;
        case heap_request::realloc: {
            heap_size += requested_size;
            if ( !eval_realloc( line, s, heap_end_addr ) ) {
                std::string err = std::string( "Realloc request failure line " )
                                      .append( std::to_string( line.line ) )
                                      .append( "\n" );
                cerr_sync( err, ansi_bred );
                return {};
            }
        } break;
        case heap_request::free: {
            if ( !eval_free( line, s ) ) {
                std::string err = std::string( "Free request failure line " )
                                      .append( std::to_string( line.line ) )
                                      .append( "\n" );
                cerr_sync( err, ansi_bred );
                return {};
            }
            std::pair<void *, size_t> &old_block = s.blocks.at( line.block_index );
            heap_size -= old_block.second;
            old_block = { nullptr, 0 };
        } break;
        default: {
            cerr_sync( "Unknown request slipped through script validation", ansi_bred );
            return {};
        }
        }
        if ( !validate_heap() ) {
            std::string err = std::string( "validate_heap() failed request " )
                                  .append( std::to_string( line.line ) )
                                  .append( "\n" );
            cerr_sync( err, ansi_bred );
            return {};
        }
        s.peak = std::max( s.peak, heap_size );
    }
    for ( size_t block_id = 0; block_id < s.blocks.size(); ++block_id ) {
        const std::pair<void *, size_t> &block = s.blocks.at( block_id );
        if ( !verify_payload( block_id, block.first, block.second ) ) {
            cerr_sync( "Final blocks validation failed.\n", ansi_bred );
            return {};
        }
    }
    return static_cast<uint8_t *>( heap_end_addr ) - static_cast<uint8_t *>( heap_segment_start() );
}

bool eval_malloc( const script_line &line, script &s, void *&heap_end )
{
    void *p = mymalloc( line.size );
    if ( nullptr == p && line.size != 0 ) {
        cerr_sync( "mymalloc() exhasted the heap\n", ansi_bred );
        return false;
    }
    if ( !verify_block( p, line.size, s ) ) {
        cerr_sync( "Block is overlapping another block causing heap corruption.\n", ansi_bred );
        return false;
    }
    std::span<uint8_t> cur_block = std::span<uint8_t>{ static_cast<uint8_t *>( p ), line.size };
    // As of current specs, subspan is undefined if Offset > .size(). So this is well defined and equivalent to
    // new.data() == this->data() + Offset. Aka, the same pointer arithmetic clang gets so fussy about ¯\_(ツ)_/¯
    std::span<uint8_t> end = cur_block.subspan( line.size );
    if ( end.data() > static_cast<uint8_t *>( heap_end ) ) {
        heap_end = end.data();
    }
    std::fill( cur_block.begin(), cur_block.end(), line.block_index & lowest_byte );
    s.blocks.at( line.block_index ) = { p, line.size };
    return true;
}

bool eval_realloc( const script_line &line, script &s, void *&heap_end )
{
    void *old_ptr = s.blocks.at( line.block_index ).first;
    size_t old_size = s.blocks.at( line.block_index ).second;
    if ( !verify_payload( line.block_index, old_ptr, old_size ) ) {
        return false;
    }
    void *new_ptr = myrealloc( old_ptr, line.size );
    if ( nullptr == new_ptr && line.size != 0 ) {
        cerr_sync( "Realloc exhausted the heap.\n", ansi_bred );
        return false;
    }
    s.blocks[line.block_index].second = 0;
    if ( !verify_block( new_ptr, line.size, s ) ) {
        return false;
    }
    if ( !verify_block( new_ptr, ( old_size < line.size ? old_size : line.size ), s ) ) {
        return false;
    }
    std::span<uint8_t> cur_block = std::span<uint8_t>{ static_cast<uint8_t *>( new_ptr ), line.size };
    // As of current specs, subspan is undefined if Offset > .size(). So this is well defined and equivalent to
    // new.data() == this->data() + Offset. Aka, the same pointer arithmetic clang gets so fussy about ¯\_(ツ)_/¯
    std::span<uint8_t> end = cur_block.subspan( line.size );
    if ( end.data() > static_cast<uint8_t *>( heap_end ) ) {
        heap_end = end.data();
    }
    std::fill( cur_block.begin(), cur_block.end(), line.block_index & lowest_byte );
    s.blocks.at( line.block_index ) = { new_ptr, line.size };
    return true;
}

bool eval_free( const script_line &line, script &s )
{
    std::pair<void *, size_t> old_block = s.blocks.at( line.block_index );
    if ( !verify_payload( line.block_index, old_block.first, old_block.second ) ) {
        cerr_sync( "Block corrupted before free", ansi_bred );
        return false;
    }
    myfree( old_block.first );
    return true;
}

bool verify_block( void *ptr, size_t size, const script &s )
{
    if ( reinterpret_cast<uintptr_t>( ptr ) % ALIGNMENT != 0 ) { // NOLINT
        cerr_sync( "block is out of alignment.", ansi_bred );
    }
    if ( nullptr == ptr && size == 0 ) {
        return true;
    }
    std::span<uint8_t> block_end = std::span<uint8_t>( static_cast<uint8_t *>( ptr ), size ).subspan( size );
    std::span<uint8_t> heap_end
        = std::span<uint8_t>( static_cast<uint8_t *>( heap_segment_start() ), heap_segment_size() )
              .subspan( heap_segment_size() );
    if ( ptr < heap_segment_start() ) {
        std::stringstream err{};
        err << "New block ( " << ptr << ":" << block_end.data() << ") not within heap segment ( "
            << heap_segment_start() << ":" << heap_end.data()
            << ")\n"
               "|----block-------|\n"
               "        |------heap-------...|\n";
        std::string errstr{ err.str() };
        cerr_sync( errstr, ansi_bred );
        return false;
    }
    if ( block_end.data() > heap_end.data() ) {
        std::stringstream err{};
        err << "New block ( " << ptr << ":" << block_end.data() << ") not within heap segment ( "
            << heap_segment_start() << ":" << heap_end.data()
            << ")\n"
               "           |----block-------|\n"
               "|...------heap------|\n";
        std::string errstr{ err.str() };
        cerr_sync( errstr, ansi_bred );
        return false;
    }
    for ( const auto &[addr, size] : s.blocks ) {
        if ( nullptr == addr || size == 0 ) {
            continue;
        }
        std::span<uint8_t> other_end = std::span<uint8_t>( static_cast<uint8_t *>( addr ), size ).subspan( size );
        if ( ptr >= addr && ptr < other_end.data() ) {
            std::stringstream err{};
            err << "New block ( " << ptr << ":" << block_end.data() << ") overlaps existing block ( " << addr << ":"
                << other_end.data() << ")\n"
                << "     |------current---------|\n"
                   "|------other-------|\n"
                   "or\n"
                   "  |--current----|\n"
                   "|------other-------|\n";
            std::string errstr{ err.str() };
            cerr_sync( errstr, ansi_bred );
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
            std::string errstr{ err.str() };
            cerr_sync( errstr, ansi_bred );
            return false;
        }
        if ( ptr < addr && block_end.data() >= other_end.data() ) {
            std::stringstream err{};
            err << "New block ( " << ptr << ":" << block_end.data() << ") overlaps existing block ( " << addr << ":"
                << other_end.data() << ")\n"
                << "      |---current---|\n"
                   "|------------other------------|\n";
            std::string errstr{ err.str() };
            cerr_sync( errstr, ansi_bred );
            return false;
        }
    }
    return true;
}

bool verify_payload( size_t block_id, void *ptr, size_t size )
{
    const uint8_t signature = block_id & lowest_byte;
    return std::ranges::all_of(
        std::span<uint8_t>( static_cast<uint8_t *>( ptr ), size ),
        [signature]( uint8_t byte ) { return byte == signature; }
    );
}

std::optional<script> parse_script( const std::string &filepath )
{
    std::ifstream sfile( filepath );
    if ( sfile.fail() ) {
        return {};
    }
    script s{};
    size_t line = 0;
    size_t max_id = 0;
    for ( std::string buf{}; std::getline( sfile, buf ); ) {
        std::istringstream readline( buf );
        std::vector<std::string> tokens_split_on_whitespace{
            std::istream_iterator<std::string>( readline ), std::istream_iterator<std::string>()
        };
        if ( tokens_split_on_whitespace.empty() || tokens_split_on_whitespace.front().starts_with( "#" ) ) {
            continue;
        }
        std::optional<script_line> parsed = tokens_pass( tokens_split_on_whitespace );
        if ( !parsed ) {
            std::string err
                = std::string( "Script error on line " ).append( std::to_string( line ) ).append( "\n" );
            cerr_sync( err, ansi_bred );
            return {};
        }
        parsed.value().line = line;
        s.lines.push_back( parsed.value() );
        max_id = std::max( max_id, parsed.value().block_index );
        ++line;
    }
    s.blocks.resize( max_id + 1 );
    return s;
}

std::optional<script_line> tokens_pass( std::span<std::string> toks )
{
    if ( toks.size() > 3 || toks.size() < 2 || !( toks[0] == "a" || toks[0] == "f" || toks[0] == "r" ) ) {
        cerr_sync( "Request has an unknown format.\n", ansi_bred );
        return {};
    }
    script_line ret{};
    try {
        ret.block_index = std::stoull( toks[1] );
    } catch ( [[maybe_unused]] std::invalid_argument &e ) {
        cerr_sync( "Could not convert request to block id.\n", ansi_bred );
        return {};
    }
    std::array<heap_request, 2> alloc_or_realloc{ heap_request::alloc, heap_request::realloc };
    if ( toks[0] == "a" || toks[0] == "r" ) {
        ret.req = alloc_or_realloc.at( static_cast<size_t>( toks[0] == "r" ) );
        try {
            ret.size = std::stoull( toks[2] );
        } catch ( [[maybe_unused]] std::invalid_argument &e ) {
            cerr_sync( "Could not convert alloc request to valid number.\n", ansi_bred );
            return {};
        }
    } else if ( toks[0] == "f" ) {
        ret.req = heap_request::free;
    } else {
        cerr_sync( "Request has an unknown format.\n", ansi_bred );
        return {};
    }
    return ret;
}

void cerr_sync( std::string_view s, std::string_view color = ansi_nil )
{
    std::osyncstream( std::cerr ) << color << s << ansi_nil;
}

void cerr_sync( std::string_view s )
{
    std::osyncstream( std::cerr ) << s;
}

void cout_sync( std::string_view s, std::string_view color = ansi_nil )
{
    std::osyncstream( std::cerr ) << color << s << ansi_nil;
}

void cout_sync( std::string_view s )
{
    std::osyncstream( std::cerr ) << s;
}

} // namespace
