#ifndef OSYNC_HH
#define OSYNC_HH

#include <string_view>

/// I can't decide how many of these testers should be
/// multithreaded/multiprocessed so I'm making io syncd and thread safe for now
/// for future changes.
namespace osync {

constexpr std::string_view ansi_bred = "\033[38;5;9m";
constexpr std::string_view ansi_bgrn = "\033[38;5;10m";
constexpr std::string_view ansi_byel = "\033[38;5;11m";
constexpr std::string_view ansi_nil = "\033[0m";

/// Syncd output so no interleaving occurs in multithreading environements.
void syncerr(std::string_view s, std::string_view color);
void syncerr(std::string_view s);
void syncout(std::string_view s, std::string_view color);
void syncout(std::string_view s);

/// Unsyncd but convenient wrapper for printing a message with color.
void cerr(std::string_view s, std::string_view color);
void cout(std::string_view s, std::string_view color);

} // namespace osync

#endif // OSYNC_HH
