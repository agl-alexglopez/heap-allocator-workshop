#pragma once
#ifndef OSYNC_HH
#define OSYNC_HH

#include <string_view>

/// I can't decide how many of these testers should be multithreaded/multiprocessed so I'm making
/// io syncd and thread safe for now for future changes.
namespace osync {

constexpr std::string_view ansi_bred = "\033[38;5;9m";
constexpr std::string_view ansi_bgrn = "\033[38;5;10m";
constexpr std::string_view ansi_byel = "\033[38;5;11m";
constexpr std::string_view ansi_nil = "\033[0m";

void cerr( std::string_view s, std::string_view color );
void cerr( std::string_view s );
void cout( std::string_view s, std::string_view color );
void cout( std::string_view s );

} // namespace osync

#endif // OSYNC_HH
