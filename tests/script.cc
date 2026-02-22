/// File: script.cc
/// ---------------
/// This file contains my implementation in C++ of a script parsing
/// and execution helper library. It is based on the work of jzelenski and Nick
/// Troccoli in that it parses scripts, but it has been updated to C++ and
/// changed significantly. I also had to include more basic script execution and
/// timing functions so that I could incorporate scripts and their execution in
/// contexts where correctness does not matter and we just need to progress
/// calls to a heap allocator library. So, this file includes function to
/// exectute scripts and to time calls to scripts based on lines given. This
/// allows much flexibility and fits in well with the rest of my testing
/// framework.
#include "script.hh"
#include "allocator.h"
#include "osync.hh"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace script {

static std::optional<Line>
tokens_pass(std::span<std::string const> toks, size_t lineno) {
    try {
        if (toks.size() > 3 || toks.size() < 2
            || !(toks[0] == "a" || toks[0] == "f" || toks[0] == "r")) {
            osync::syncerr("Request has an unknown format.\n",
                           osync::ansi_bred);
            return {};
        }
        Line ret{
            .req = Op::EMPTY,
            .block_index = 0,
            .size = 0,
            .line = lineno,
        };
        ret.block_index = std::stoull(toks[1]);
        if (toks[0] == "a" || toks[0] == "r") {
            ret.req = toks[0] == "a" ? Op::ALLOC : Op::REALLOCD;
            ret.size = std::stoull(toks[2]);
        } else if (toks[0] == "f") {
            ret.req = Op::FREED;
        } else {
            osync::syncerr("Request has an unknown format.\n",
                           osync::ansi_bred);
            return {};
        }
        return ret;
    } catch (...) {
        auto const err = std::string("Could not convert size or id on line: ")
                             .append(std::to_string(lineno))
                             .append("\n");
        osync::syncerr(err, osync::ansi_bred);
        return {};
    }
}

std::optional<Requests>
parse_script(std::string const &filepath) {
    std::ifstream sfile(filepath);
    if (sfile.fail()) {
        auto const msg
            = std::string("Could not open file ").append(filepath).append("\n");
        osync::syncerr(msg, osync::ansi_bred);
        return {};
    }
    size_t const lineo = std::count(std::istreambuf_iterator<char>(sfile),
                                    std::istreambuf_iterator<char>(), '\n');
    sfile.clear();
    sfile.seekg(0);
    Requests s{.lines{}, .blocks{}, .peak = 0};
    s.lines.reserve(lineo + 1);
    size_t line = 0;
    size_t max_id = 0;
    for (std::string buf{}; std::getline(sfile, buf);) {
        std::istringstream readline(buf);
        std::vector<std::string> const tokens_split_on_whitespace{
            std::istream_iterator<std::string>(readline),
            std::istream_iterator<std::string>()};
        if (tokens_split_on_whitespace.empty()
            || tokens_split_on_whitespace.front().starts_with("#")) {
            continue;
        }
        std::optional<struct Line> const parsed
            = tokens_pass(tokens_split_on_whitespace, line);
        if (!parsed) {
            return {};
        }
        s.lines.push_back(parsed.value());
        max_id = std::max(max_id, parsed.value().block_index);
        ++line;
    }
    s.blocks.resize(max_id + 1);
    return s;
}

static bool
exec_malloc(script::Line const &line, script::Requests &s, void *&heap_end) {
    void *p = wmalloc(line.size);
    if (nullptr == p && line.size != 0) {
        osync::syncerr("wmalloc() exhasted the heap\n", osync::ansi_bred);
        return false;
    }
    // Specs say subspan is undefined if Offset > .size(). So this is safe:
    // new.data() == this->data() + Offset. This is basically pointer arithmetic
    // but the kind that makes clang-tidy happy ¯\_(ツ)_/¯
    auto const end
        = std::span<uint8_t>{static_cast<uint8_t *>(p), line.size}.subspan(
            line.size);
    if (end.data() > static_cast<uint8_t *>(heap_end)) {
        heap_end = end.data();
    }
    s.blocks.at(line.block_index) = {p, line.size};
    return true;
}

static bool
exec_realloc(script::Line const &line, script::Requests &s, void *&heap_end) {
    void *old_ptr = s.blocks.at(line.block_index).first;
    void *new_ptr = wrealloc(old_ptr, line.size);
    if (nullptr == new_ptr && line.size != 0) {
        osync::syncerr("Realloc exhausted the heap.\n", osync::ansi_bred);
        return false;
    }
    s.blocks[line.block_index].second = 0;
    auto const end
        = std::span<uint8_t>{static_cast<uint8_t *>(new_ptr), line.size}
              .subspan(line.size);
    if (end.data() > static_cast<uint8_t *>(heap_end)) {
        heap_end = end.data();
    }
    s.blocks.at(line.block_index) = {new_ptr, line.size};
    return true;
}

std::optional<size_t>
exec_request(Line const &line, Requests &script, size_t heap_size,
             void *&heap_end) {
    switch (line.req) {
        case script::Op::ALLOC: {
            heap_size += line.size;
            if (!exec_malloc(line, script, heap_end)) {
                auto const err = std::string("Malloc request failure line ")
                                     .append(std::to_string(line.line))
                                     .append("\n");
                osync::syncerr(err, osync::ansi_bred);
                return {};
            }
        } break;
        case script::Op::REALLOCD: {
            heap_size
                += (line.size - script.blocks.at(line.block_index).second);
            if (!exec_realloc(line, script, heap_end)) {
                auto const err = std::string("Realloc request failure line ")
                                     .append(std::to_string(line.line))
                                     .append("\n");
                osync::syncerr(err, osync::ansi_bred);
                return {};
            }
        } break;
        case script::Op::FREED: {
            std::pair<void *, size_t> &old_block
                = script.blocks.at(line.block_index);
            wfree(old_block.first);
            heap_size -= old_block.second;
            old_block = {nullptr, 0};
        } break;
        default: {
            osync::syncerr("Unknown request slipped through script validation",
                           osync::ansi_bred);
            return {};
        }
    }
    return heap_size;
}

static std::optional<double>
time_malloc(script::Line const &line, script::Requests &s, void *&heap_end) {
    void *p = heap_end;
    auto start_time = std::clock();
    void const volatile *const start_report = p;
    p = wmalloc(line.size);
    void const volatile *const end_report = p;
    auto end_time = std::clock();

    if (nullptr == p && line.size != 0) {
        auto printablestart
            = reinterpret_cast<uintptr_t>(start_report);             // NOLINT
        auto printableend = reinterpret_cast<uintptr_t>(end_report); // NOLINT
        auto err = std::string("wmalloc() exhaustion (ignore the following)...")
                       .append(std::to_string(printablestart))
                       .append(std::to_string(printableend))
                       .append("\n");
        osync::syncerr(err, osync::ansi_bred);
        return {};
    }
    // Specs say subspan is undefined if Offset > .size(). So this is safe:
    // new.data() == this->data() + Offset. This is basically pointer arithmetic
    // but the kind that makes clang-tidy happy ¯\_(ツ)_/¯
    auto const end
        = std::span<uint8_t>{static_cast<uint8_t *>(p), line.size}.subspan(
            line.size);
    if (end.data() > static_cast<uint8_t *>(heap_end)) {
        heap_end = end.data();
    }
    s.blocks.at(line.block_index) = {p, line.size};
    return 1000.0
           * (static_cast<double>(end_time - start_time) / CLOCKS_PER_SEC);
}

static std::optional<double>
time_realloc(script::Line const &line, script::Requests &s, void *&heap_end) {
    void *old_ptr = s.blocks.at(line.block_index).first;
    void *new_ptr = nullptr;
    auto start_time = std::clock();
    void const volatile *const start_report = new_ptr;
    new_ptr = wrealloc(old_ptr, line.size);
    void const volatile *const end_report = new_ptr;
    auto end_time = std::clock();

    if (nullptr == new_ptr && line.size != 0) {
        auto printablestart
            = reinterpret_cast<uintptr_t>(start_report);             // NOLINT
        auto printableend = reinterpret_cast<uintptr_t>(end_report); // NOLINT
        auto err
            = std::string("wrealloc() exhaustion (ignore the following)...")
                  .append(std::to_string(printablestart))
                  .append(std::to_string(printableend))
                  .append("\n");
        osync::syncerr(err, osync::ansi_bred);
        return {};
    }
    s.blocks[line.block_index].second = 0;
    auto const end
        = std::span<uint8_t>{static_cast<uint8_t *>(new_ptr), line.size}
              .subspan(line.size);
    if (end.data() > static_cast<uint8_t *>(heap_end)) {
        heap_end = end.data();
    }
    s.blocks.at(line.block_index) = {new_ptr, line.size};
    return 1000.0
           * (static_cast<double>(end_time - start_time) / CLOCKS_PER_SEC);
}

static double
time_free(Line const &line, Requests &script) {
    std::pair<void *, size_t> const old_block
        = script.blocks.at(line.block_index);
    auto start_time = std::clock();
    void const volatile *const start_addr = old_block.first; // NOLINT
    wfree(old_block.first);
    void const volatile *const end_addr = start_addr; // NOLINT
    auto end_time = std::clock();
    double const result
        = 1000.0
          * (static_cast<double>(end_time - start_time) / CLOCKS_PER_SEC);
    // This silly check is to silence compiler warning about unused volatile.
    if (result < 0) {
        std::stringstream s;
        s << static_cast<void const *>(&end_addr);
        auto const msg = std::string("Error timing free. Last known address")
                             .append(s.str());
        osync::syncerr(msg, osync::ansi_bred);
    }
    return result;
}

std::optional<Heap_delta>
time_request(Line const &line, Requests &script, size_t heap_size,
             void *&heap_end) {
    switch (line.req) {
        case script::Op::ALLOC: {
            heap_size += line.size;
            std::optional<double> t = time_malloc(line, script, heap_end);
            if (!t) {
                return {};
            }
            return std::optional<Heap_delta>{{heap_size, t.value()}};
        }
        case script::Op::REALLOCD: {
            heap_size
                += (line.size - script.blocks.at(line.block_index).second);
            std::optional<double> t = time_realloc(line, script, heap_end);
            if (!t) {
                return {};
            }
            return std::optional<Heap_delta>{{heap_size, t.value()}};
        }
        case script::Op::FREED: {
            std::pair<void *, size_t> &old_block
                = script.blocks.at(line.block_index);
            heap_size -= old_block.second;
            double const t = time_free(line, script);
            old_block = {nullptr, 0};
            return std::optional<Heap_delta>{{heap_size, t}};
        }
        default: {
            osync::syncerr("Unknown request slipped through script validation",
                           osync::ansi_bred);
            return {};
        }
    }
}

} // namespace script
