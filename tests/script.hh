/// File: script.hh
/// ---------------
/// This file contains a helper library that can parse and execute scripts.
/// Given a script file it will return the completed script::requests object
/// with all lines ready for running through a program like a correctness tester
/// or timing program.
#pragma once
#ifndef SCRIPT_HH
#    define SCRIPT_HH

#    include <cstddef>
#    include <cstdint>
#    include <optional>
#    include <string>
#    include <utility>
#    include <vector>

namespace script {

enum op : uint8_t {
    empty = 0,
    alloc,
    reallocd,
    freed,
};

struct line {
    op req;
    size_t block_index;
    size_t size;
    size_t line;
};

struct requests {
    std::vector<line> lines;
    std::vector<std::pair<void *, size_t>> blocks;
    size_t peak;
};

struct heap_delta {
    size_t heap_size;
    double delta_time;
};

/// @brief parse_script  if successful parsing of the .script file at the
/// filepath location
///                      occurs a requests object is returned otherwise nothing
///                      is returned.
/// @param filepath      the path the .script file we wish parsed.
/// @return              an empty optional on failure, a requests object on
/// success.
std::optional<requests> parse_script(std::string const &filepath);

/// @brief exec_request  executes a heap request on the current line object
/// passed in and adjusts the
///                      end of the heap pointer if a change has occured.
///                      Executing a request returns the new size of the heap if
///                      the request was successful. However upon failure of any
///                      request output is given to cerr and we return empty.
/// @param line          the current line of hte script of requests we are
/// executing.
/// @param script        the requests objec that holds the blocks of pointers we
/// may adjust.
/// @param heap_size     the current heap size as input.
/// @param heap_end      the pointer to the heap end we take by reference for
/// adjustment if needed.
/// @return              the new size of the heap upon a successful request,
/// empty on any failure.
std::optional<size_t> exec_request(line const &line, requests &script,
                                   size_t heap_size, void *&heap_end);

/// @brief time_request  executes a request with timing information regarding
/// how long the request took.
/// @param line          the current line of hte script of requests we are
/// executing.
/// @param script        the requests objec that holds the blocks of pointers we
/// may adjust.
/// @param heap_size     the current heap size as input.
/// @param heap_end      the pointer to the heap end we take by reference for
/// adjustment if needed.
/// @return              the new size of the heap and request time or nothing on
/// failure.
std::optional<heap_delta> time_request(line const &line, requests &script,
                                       size_t heap_size, void *&heap_end);

} // namespace script
#endif
