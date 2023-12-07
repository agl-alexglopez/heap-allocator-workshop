#include "script.hh"
#include "allocator.h"
#include "osync.hh"

#include <cstdint>
#include <ctime>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>

namespace script {
namespace {
constexpr std::string_view ansi_bred = "\033[38;5;9m";
constexpr std::string_view ansi_nil = "\033[0m";
} // namespace

std::optional<requests> parse_script( const std::string &filepath )
{
    std::ifstream sfile( filepath );
    if ( sfile.fail() ) {
        return {};
    }
    requests s{ .lines{}, .blocks{}, .peak = 0 };
    size_t line = 0;
    size_t max_id = 0;
    for ( std::string buf{}; std::getline( sfile, buf ); ) {
        std::istringstream readline( buf );
        const std::vector<std::string> tokens_split_on_whitespace{
            std::istream_iterator<std::string>( readline ), std::istream_iterator<std::string>()
        };
        if ( tokens_split_on_whitespace.empty() || tokens_split_on_whitespace.front().starts_with( "#" ) ) {
            continue;
        }
        const std::optional<struct line> parsed = tokens_pass( tokens_split_on_whitespace, line );
        if ( !parsed ) {
            return {};
        }
        s.lines.push_back( parsed.value() );
        max_id = std::max( max_id, parsed.value().block_index );
        ++line;
    }
    s.blocks.resize( max_id + 1 );
    return s;
}

std::optional<line> tokens_pass( std::span<const std::string> toks, size_t lineno )
{
    try {
        if ( toks.size() > 3 || toks.size() < 2 || !( toks[0] == "a" || toks[0] == "f" || toks[0] == "r" ) ) {
            osync::cerr( "Request has an unknown format.\n", ansi_bred );
            return {};
        }
        line ret{ .line = lineno };
        ret.block_index = std::stoull( toks[1] );
        if ( toks[0] == "a" || toks[0] == "r" ) {
            ret.req = toks[0] == "a" ? op::alloc : op::reallocd;
            ret.size = std::stoull( toks[2] );
        } else if ( toks[0] == "f" ) {
            ret.req = op::freed;
        } else {
            osync::cerr( "Request has an unknown format.\n", ansi_bred );
            return {};
        }
        return ret;
    } catch ( ... ) {
        const auto err = std::string( "Could not convert size or id on line: " )
                             .append( std::to_string( lineno ) )
                             .append( "\n" );
        osync::cerr( err, ansi_bred );
        return {};
    }
}

bool exec_malloc( const script::line &line, script::requests &s, void *&heap_end )
{
    void *p = mymalloc( line.size );
    if ( nullptr == p && line.size != 0 ) {
        osync::cerr( "mymalloc() exhasted the heap\n", ansi_bred );
        return false;
    }
    const std::span<uint8_t> cur_block = std::span<uint8_t>{ static_cast<uint8_t *>( p ), line.size };
    // Specs say subspan is undefined if Offset > .size(). So this is safe: new.data() == this->data() + Offset.
    // This is basically pointer arithmetic but the kind that makes clang-tidy happy ¯\_(ツ)_/¯
    const std::span<uint8_t> end = cur_block.subspan( line.size );
    if ( end.data() > static_cast<uint8_t *>( heap_end ) ) {
        heap_end = end.data();
    }
    s.blocks.at( line.block_index ) = { p, line.size };
    return true;
}

bool exec_realloc( const script::line &line, script::requests &s, void *&heap_end )
{
    void *old_ptr = s.blocks.at( line.block_index ).first;
    const size_t old_size = s.blocks.at( line.block_index ).second;
    void *new_ptr = myrealloc( old_ptr, line.size );
    if ( nullptr == new_ptr && line.size != 0 ) {
        osync::cerr( "Realloc exhausted the heap.\n", ansi_bred );
        return false;
    }
    s.blocks[line.block_index].second = 0;
    const auto cur_block = std::span<uint8_t>{ static_cast<uint8_t *>( new_ptr ), line.size };
    const std::span<uint8_t> end = cur_block.subspan( line.size );
    if ( end.data() > static_cast<uint8_t *>( heap_end ) ) {
        heap_end = end.data();
    }
    s.blocks.at( line.block_index ) = { new_ptr, line.size };
    return true;
}

std::optional<size_t> exec_request( const line &line, requests &script, size_t heap_size, void *&heap_end )
{
    switch ( line.req ) {
    case script::op::alloc: {
        heap_size += line.size;
        if ( !exec_malloc( line, script, heap_end ) ) {
            const auto err = std::string( "Malloc request failure line " )
                                 .append( std::to_string( line.line ) )
                                 .append( "\n" );
            osync::cerr( err, osync::ansi_bred );
            return {};
        }
    } break;
    case script::op::reallocd: {
        heap_size += ( line.size - script.blocks.at( line.block_index ).second );
        if ( !exec_realloc( line, script, heap_end ) ) {
            const auto err = std::string( "Realloc request failure line " )
                                 .append( std::to_string( line.line ) )
                                 .append( "\n" );
            osync::cerr( err, osync::ansi_bred );
            return {};
        }
    } break;
    case script::op::freed: {
        std::pair<void *, size_t> &old_block = script.blocks.at( line.block_index );
        myfree( old_block.first );
        heap_size -= old_block.second;
        old_block = { nullptr, 0 };
    } break;
    default: {
        osync::cerr( "Unknown request slipped through script validation", osync::ansi_bred );
        return {};
    }
    }
    return heap_size;
}

std::optional<double> time_malloc( const script::line &line, script::requests &s, void *&heap_end )
{
    void *p = heap_end;
    auto start_time = std::clock();
    volatile void *start_report = p;
    p = mymalloc( line.size );
    volatile void *end_report = p;
    auto end_time = std::clock();

    if ( nullptr == p && line.size != 0 ) {
        auto printablestart = reinterpret_cast<uintptr_t>( start_report ); // NOLINT
        auto printableend = reinterpret_cast<uintptr_t>( end_report );     // NOLINT
        auto err = std::string( "mymalloc() exhaustion (ignore the following)..." )
                       .append( std::to_string( printablestart ) )
                       .append( std::to_string( printableend ) )
                       .append( "\n" );
        osync::cerr( err, ansi_bred );
        return {};
    }
    const std::span<uint8_t> cur_block = std::span<uint8_t>{ static_cast<uint8_t *>( p ), line.size };
    // Specs say subspan is undefined if Offset > .size(). So this is safe: new.data() == this->data() + Offset.
    // This is basically pointer arithmetic but the kind that makes clang-tidy happy ¯\_(ツ)_/¯
    const std::span<uint8_t> end = cur_block.subspan( line.size );
    if ( end.data() > static_cast<uint8_t *>( heap_end ) ) {
        heap_end = end.data();
    }
    s.blocks.at( line.block_index ) = { p, line.size };
    return 1000.0 * ( static_cast<double>( end_time - start_time ) / CLOCKS_PER_SEC );
}

std::optional<double> time_realloc( const script::line &line, script::requests &s, void *&heap_end )
{
    void *old_ptr = s.blocks.at( line.block_index ).first;
    const size_t old_size = s.blocks.at( line.block_index ).second;
    void *new_ptr = nullptr;
    auto start_time = std::clock();
    volatile void *start_report = new_ptr;
    new_ptr = myrealloc( old_ptr, line.size );
    volatile void *end_report = new_ptr;
    auto end_time = std::clock();

    if ( nullptr == new_ptr && line.size != 0 ) {
        auto printablestart = reinterpret_cast<uintptr_t>( start_report ); // NOLINT
        auto printableend = reinterpret_cast<uintptr_t>( end_report );     // NOLINT
        auto err = std::string( "myrealloc() exhaustion (ignore the following)..." )
                       .append( std::to_string( printablestart ) )
                       .append( std::to_string( printableend ) )
                       .append( "\n" );
        osync::cerr( err, ansi_bred );
        return {};
    }
    s.blocks[line.block_index].second = 0;
    const auto cur_block = std::span<uint8_t>{ static_cast<uint8_t *>( new_ptr ), line.size };
    const std::span<uint8_t> end = cur_block.subspan( line.size );
    if ( end.data() > static_cast<uint8_t *>( heap_end ) ) {
        heap_end = end.data();
    }
    s.blocks.at( line.block_index ) = { new_ptr, line.size };
    return 1000.0 * ( static_cast<double>( end_time - start_time ) / CLOCKS_PER_SEC );
}

double time_free( const line &line, requests &script )
{
    std::pair<void *, size_t> old_block = script.blocks.at( line.block_index );
    auto start_time = std::clock();
    volatile void *start_addr = old_block.first;
    myfree( old_block.first );
    volatile void *end_addr = start_addr;
    auto end_time = std::clock();
    return 1000.0 * ( static_cast<double>( end_time - start_time ) / CLOCKS_PER_SEC );
}

std::optional<heap_delta> time_request( const line &line, requests &script, size_t heap_size, void *&heap_end )
{
    double cpu_time{};
    switch ( line.req ) {
    case script::op::alloc: {
        heap_size += line.size;
        std::optional<double> t = time_malloc( line, script, heap_end );
        if ( !t ) {
            return {};
        }
        return std::optional<heap_delta>{ { heap_size, t.value() } };
    }
    case script::op::reallocd: {
        heap_size += ( line.size - script.blocks.at( line.block_index ).second );
        std::optional<double> t = time_realloc( line, script, heap_end );
        if ( !t ) {
            return {};
        }
        return std::optional<heap_delta>{ { heap_size, t.value() } };
    }
    case script::op::freed: {
        std::pair<void *, size_t> &old_block = script.blocks.at( line.block_index );
        heap_size -= old_block.second;
        double t = time_free( line, script );
        old_block = { nullptr, 0 };
        return std::optional<heap_delta>{ { heap_size, t } };
    }
    default: {
        osync::cerr( "Unknown request slipped through script validation", osync::ansi_bred );
        return {};
    }
    }
}

} // namespace script
