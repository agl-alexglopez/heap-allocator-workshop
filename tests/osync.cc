#include "osync.hh"
#include <iostream>
#include <string_view>
#include <syncstream>

namespace osync {

void cerr( std::string_view s, std::string_view color = ansi_nil )
{
    std::osyncstream( std::cerr ) << color << s << ansi_nil;
}

void cerr( std::string_view s )
{
    std::osyncstream( std::cerr ) << s;
}

void cout( std::string_view s, std::string_view color = ansi_nil )
{
    std::osyncstream( std::cout ) << color << s << ansi_nil;
}

void cout( std::string_view s )
{
    std::osyncstream( std::cout ) << s;
}

} // namespace osync
