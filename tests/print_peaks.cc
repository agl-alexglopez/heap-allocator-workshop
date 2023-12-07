
#include <cstddef>
#include <iostream>
#include <span>

namespace {} // namespace

int peaks( std::span<const char *const> args )
{
    try {
        for ( const auto *arg : args ) {
            std::cout << arg << "\n";
        }
        return 0;
    } catch ( std::exception &e ) {
        std::cerr << "Caught " << e.what() << "\n";
        return 1;
    }
}

int main( int argc, char **argv )
{
    auto args = std::span<const char *const>{ argv, static_cast<size_t>( argc ) };
    if ( args.empty() ) {
        return 0;
    }
    return peaks( args );
}
