#pragma once
#ifndef SCRIPT_HH
#define SCRIPT_HH

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace script {

enum op
{
    empty = 0,
    alloc,
    reallocd,
    freed,
};

struct line
{
    op req;
    size_t block_index;
    size_t size;
    size_t line;
};

struct requests
{
    std::vector<line> lines;
    std::vector<std::pair<void *, size_t>> blocks;
    size_t peak;
};

struct heap_delta
{
    size_t heap_size;
    double delta_time;
};

std::optional<requests> parse_script( const std::string &filepath );
std::optional<line> tokens_pass( std::span<const std::string> toks, size_t line );
std::optional<size_t> exec_request( const line &line, requests &script, size_t heap_size, void *&heap_end );
std::optional<heap_delta> time_request( const line &line, requests &script, size_t heap_size, void *&heap_end );

} // namespace script
#endif
