#include "allocator.h"
#include "matplot/core/figure_registry.h"
#include "matplot/freestanding/plot.h"
#include "osync.hh"
#include "print_utility.h"
#include "script.hh"
#include "segment.h"
// NOLINTNEXTLINE(*include-cleaner)
#include <matplot/matplot.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using breakpoint = size_t;

constexpr size_t heap_size = 1ULL << 32;
constexpr int numeric_base = 10;
constexpr int max_digits = 9;

struct user_breaks
{
    enum print_style style;
    std::vector<breakpoint> breakpoints{};
    bool quiet{ false };
};

struct break_limit
{
    size_t cur;
    size_t max;
};

int print_peaks( const std::string &script_name, user_breaks &user_reqs );
size_t print_allocator( script::requests &s, user_breaks &user_reqs );
void handle_user_breakpoints( user_breaks &user_reqs, break_limit lim );
bool validate_breakpoints( script::requests &s, user_breaks &user_reqs );

} // namespace

int peaks( std::span<const char *const> args )
{
    try {
        user_breaks breaks{};
        std::string file{};
        for ( const auto *arg : args ) {
            try {
                const breakpoint found = std::stoull( arg );
                breaks.breakpoints.push_back( found );
            } catch ( ... ) {
                auto strarg = std::string( arg );
                if ( std::string::npos != strarg.find( ".script" ) ) {
                    file = std::move( strarg );
                } else if ( strarg == "-v" ) {
                    breaks.style = VERBOSE;
                } else if ( strarg == "-q" ) {
                    breaks.quiet = true;
                } else {
                    auto err = std::string(
                                   "Valid options are: numbers for breakpoints or the -v for verbose flag.\nFound: "
                    )
                                   .append( strarg )
                                   .append( "\n" );
                    osync::cerr( err, osync::ansi_bred );
                    return 1;
                }
            }
        }
        std::sort( breaks.breakpoints.begin(), breaks.breakpoints.end() );
        return print_peaks( file, breaks );
    } catch ( std::exception &e ) {
        std::cerr << "Caught " << e.what() << "\n";
        return 1;
    }
}

int main( int argc, char **argv )
{
    auto args = std::span<const char *const>{ argv, static_cast<size_t>( argc ) }.subspan( 1 );
    if ( args.empty() ) {
        return 0;
    }
    return peaks( args );
}

namespace {

int print_peaks( const std::string &script_name, user_breaks &user_reqs )
{
    std::optional<script::requests> s = script::parse_script( script_name );
    if ( !s ) {
        return 1;
    }
    script::requests &script = s.value();
    if ( !validate_breakpoints( script, user_reqs ) ) {
        return 1;
    }
    init_heap_segment( heap_size );
    if ( !myinit( heap_segment_start(), heap_segment_size() ) ) {
        osync::cerr( "Could not initialize heap\n", osync::ansi_bred );
        return 1;
    }
    void *dummy = heap_segment_start();
    std::vector<size_t> free_nodes{};
    size_t peak_free_node_request = 0;
    size_t peak_free_node_count = 0;
    size_t curr_breakpoint = 0;
    for ( const auto &line : script.lines ) {
        if ( !script::exec_request( line, script, 0, dummy ) ) {
            return 1;
        }
        const size_t total_free_nodes = get_free_total();
        free_nodes.push_back( total_free_nodes );

        if ( curr_breakpoint < user_reqs.breakpoints.size()
             && user_reqs.breakpoints[curr_breakpoint] == line.line ) {
            const auto msg = std::string( "After executing line " )
                                 .append( osync::ansi_byel )
                                 .append( std::to_string( line.line ) )
                                 .append( osync::ansi_nil )
                                 .append( " we have " )
                                 .append( osync::ansi_byel )
                                 .append( std::to_string( total_free_nodes ) )
                                 .append( osync::ansi_nil )
                                 .append( " free nodes.\n" );
            std::cout << msg;
            print_free_nodes( user_reqs.style );
            std::cout << msg;
            handle_user_breakpoints( user_reqs, { curr_breakpoint, script.lines.size() - 1 } );
            ++curr_breakpoint;
        }
        if ( total_free_nodes > peak_free_node_count ) {
            peak_free_node_count = total_free_nodes;
            peak_free_node_request = line.line;
        }
    }

    init_heap_segment( heap_size );
    if ( !myinit( heap_segment_start(), heap_segment_size() ) ) {
        osync::cerr( "Could not initialize heap\n", osync::ansi_bred );
        return 1;
    }
    dummy = heap_segment_start();
    size_t req = 0;
    for ( const auto &line : script.lines ) {
        if ( !script::exec_request( line, script, 0, dummy ) ) {
            return 1;
        }
        if ( req == peak_free_node_request ) {
            const auto msg = std::string( "Peak free nodes occurred after line " )
                                 .append( osync::ansi_byel )
                                 .append( std::to_string( req + 1 ) )
                                 .append( osync::ansi_nil )
                                 .append( ". There were " )
                                 .append( osync::ansi_byel )
                                 .append( std::to_string( peak_free_node_count ) )
                                 .append( osync::ansi_nil )
                                 .append( " free nodes.\n" );
            std::cout << msg;
            print_free_nodes( user_reqs.style );
            std::cout << msg;
        }
        ++req;
    }
    if ( user_reqs.quiet ) {
        return 0;
    }
    auto p = matplot::gcf( true );
    auto axes = p->current_axes();
    axes->title( "Free Node Count over Heap Lifetime" );
    axes->grid( true );
    axes->font_size( 14.0 );
    auto plot = axes->plot( free_nodes, "r-" );
    plot->line_width( 3 );
    p->size( 1920, 1080 );
    matplot::show();
    return 0;
}

void handle_user_breakpoints( user_breaks &user_reqs, break_limit lim )
{
    const size_t min = user_reqs.breakpoints[lim.cur] + 1;
    for ( ;; ) {
        if ( user_reqs.breakpoints[lim.cur] == lim.max ) {
            auto msg = std::string();
            std::cout << "Script complete.\nEnter<ENTER> to exit: ";
            std::getline( std::cin, msg );
            return;
        }
        std::cout << "Enter the character <c> to continue to next breakpoint.\n"
                     "Enter a positive line number to add another breakpoint.\n"
                     "Enter <ENTER> to exit: ";
        std::string buf{};
        std::getline( std::cin, buf );
        try {
            const size_t breakpoint = std::stoull( buf );
            if ( breakpoint > lim.max || breakpoint < min ) {
                const auto msg = std::string( "Breakpoint is past the last which is " )
                                     .append( std::to_string( lim.max ) )
                                     .append( "\n" );
                std::cout << msg;
            } else if ( !std::binary_search(
                            user_reqs.breakpoints.begin(), user_reqs.breakpoints.end(), breakpoint
                        ) ) {
                user_reqs.breakpoints.insert(
                    std::lower_bound( user_reqs.breakpoints.begin(), user_reqs.breakpoints.end(), breakpoint ),
                    breakpoint
                );
            }
        } catch ( ... ) {
            if ( buf.empty() || buf == "c" ) {
                return;
            }
            const auto msg = std::string( "Invalid entry: " )
                                 .append( osync::ansi_bred )
                                 .append( buf )
                                 .append( osync::ansi_nil )
                                 .append( "\n" );
            std::cout << msg;
        }
    }
}

bool validate_breakpoints( script::requests &s, user_breaks &user_reqs )
{
    const size_t reqs = s.lines.size();
    return std::all_of( user_reqs.breakpoints.begin(), user_reqs.breakpoints.end(), [reqs]( breakpoint b ) {
        return b < reqs;
    } );
}

} // namespace
