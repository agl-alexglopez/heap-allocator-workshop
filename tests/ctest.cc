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
void *eval_malloc( script_line &line, void *&heap_end );
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
            cerr_sync( circle, ansi_bred );
            ++fail_count;
        }
    }
    return 0;
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
        size_t id = line.block_index;
        size_t requested_size = line.size;
        switch ( line.req ) {
        case heap_request::alloc: {
            heap_size += requested_size;
            void *new_req = eval_malloc( line, heap_end_addr );
            if ( nullptr == new_req ) {
                return {};
            }
            s.blocks.emplace_back( new_req, line.size );
        }
        case heap_request::realloc: {
            heap_size += requested_size;
            ///
            /// TODO(agl-alexglopez)  Implement realloc checks.
            ///
        }
        case heap_request::free:
        default: {
            cerr_sync( "Unknown request slipped through", ansi_bred );
            return {};
        }
        }
        if ( !validate_heap() ) {
            std::string err
                = std::string( "validate_heap() failed request " ).append( std::to_string( line.line ) );
            cerr_sync( err, ansi_bred );
            return {};
        }
    }
    return static_cast<uint8_t *>( heap_end_addr ) - static_cast<uint8_t *>( heap_segment_start() );
}

void *eval_malloc( script_line &line, void *&heap_end )
{
    void *p = mymalloc( line.size );
    if ( nullptr == p && line.size != 0 ) {
        cerr_sync( "mymalloc() exhasted the heap", ansi_bred );
        return nullptr;
    }

    //
    // TODO(agl-alexglopez) Implement verify block logic.
    //

    if ( static_cast<uint8_t *>( p ) + line.size > static_cast<uint8_t *>( heap_end ) ) { // NOLINT
        heap_end = static_cast<uint8_t *>( p ) + line.size;                               // NOLINT
    }

    // Try this to see if it works!
    // std::span<uint8_t> cur_block{ static_cast<uint8_t *>( p ), line.size };
    // if ( &cur_block.back() > static_cast<uint8_t *>( heap_end ) ) {
    //     heap_end = &cur_block.back();
    // }
    std::span<uint8_t> cur_block{ static_cast<uint8_t *>( p ), line.size };
    std::fill( cur_block.begin(), cur_block.end(), line.block_index & lowest_byte );
    return p;
}

std::optional<script> parse_script( const std::string &filepath )
{
    std::ifstream sfile( filepath );
    if ( sfile.fail() ) {
        return {};
    }
    script s{};
    size_t line = 0;
    for ( std::string buf{}; std::getline( sfile, buf ); ) {
        std::istringstream readline( buf );
        std::vector<std::string> tokens_split_on_whitespace{
            std::istream_iterator<std::string>( readline ), std::istream_iterator<std::string>()
        };
        std::optional<script_line> parsed = tokens_pass( tokens_split_on_whitespace );
        if ( !parsed ) {
            std::string err = std::string( "Script error on line " ).append( std::to_string( line ) );
            cerr_sync( err, ansi_bred );
            return {};
        }
        ++line;
    }
    return s;
}

std::optional<script_line> tokens_pass( std::span<std::string> toks )
{
    if ( toks.size() > 3 || toks.size() < 2 || !( toks[0] == "a" || toks[0] == "f" || toks[0] == "r" ) ) {
        cerr_sync( "Request has an unknown format.", ansi_bred );
        return {};
    }
    script_line ret{};
    try {
        ret.block_index = std::stoull( toks[1] );
    } catch ( [[maybe_unused]] std::invalid_argument &e ) {
        cerr_sync( "Could not convert request to block id.", ansi_bred );
        return {};
    }
    std::array<heap_request, 2> alloc_or_realloc{ heap_request::alloc, heap_request::realloc };
    if ( toks[0] == "a" || toks[0] == "r" ) {
        ret.req = alloc_or_realloc.at( static_cast<size_t>( toks[0] == "r" ) );
        try {
            ret.size = std::stoull( toks[2] );
        } catch ( [[maybe_unused]] std::invalid_argument &e ) {
            cerr_sync( "Could not convert alloc request to valid number.", ansi_bred );
            return {};
        }
    } else if ( toks[0] == "f" ) {
        ret.req = heap_request::free;
    } else {
        cerr_sync( "Request has an unknown format.", ansi_bred );
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
