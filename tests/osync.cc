#include "osync.hh"
#include <iostream>
#include <string_view>
#include <syncstream>

namespace osync {

void
syncerr(std::string_view s, std::string_view color = ansi_nil) {
    std::osyncstream(std::cerr) << color << s << ansi_nil;
}

void
syncerr(std::string_view s) {
    std::osyncstream(std::cerr) << s;
}

void
syncout(std::string_view s, std::string_view color = ansi_nil) {
    std::osyncstream(std::cout) << color << s << ansi_nil;
}

void
syncout(std::string_view s) {
    std::osyncstream(std::cout) << s;
}

void
cerr(std::string_view s, std::string_view color) {
    std::cerr << color << s << ansi_nil;
}

void
cout(std::string_view s, std::string_view color) {
    std::cout << color << s << ansi_nil;
}

} // namespace osync
