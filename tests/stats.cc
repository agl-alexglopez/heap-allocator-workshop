#include "allocator.h"
#include "osync.hh"
#include "script.hh"
#include "segment.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

// Requests to the heap are zero indexed, but we can allow users to enter line no. then subtract 1.
struct interval
{
    int start_req;
    int end_req;
};

struct interval_reqs
{
    std::vector<interval> intervals{};
    std::vector<double> interval_averages{};
    bool quiet{ false };
};

constexpr size_t heap_size = 1L << 32;
constexpr int base_10 = 10;

/// FUNCTION PROTOTYPE

int time_script( std::string_view script_name, interval_reqs &user_requests );
std::optional<size_t> time_allocator( script::requests &s, interval_reqs &user_requests );
bool validate_intervals( script::requests &s, interval_reqs &user_requests );

} // namespace

int stats( std::span<const char *const> args )
{
    try {
        interval_reqs user_reqs{};
        std::string_view filename{};
        for ( size_t i = 0; i < args.size(); ++i ) {
            auto arg_copy = std::string( args[i] );
            interval intv{ 0, 0 };
            if ( arg_copy == "-q" ) {
                user_reqs.quiet = true;
            } else if ( arg_copy == "-r" ) {
                if ( i + 1 >= args.size() ) {
                    osync::cerr( "Starting breakpoint not specified\n", osync::ansi_bred );
                    return 1;
                }
                intv.start_req = std::stoi( args[i + 1] );
                if ( !user_reqs.intervals.empty() && user_reqs.intervals.at( i - 1 ).end_req >= intv.start_req ) {
                    osync::cerr( "Stats does not support overlapping intervals.\n", osync::ansi_bred );
                }
                if ( i + 2 >= args.size() ) {
                    break;
                }
                intv.end_req = std::stoi( args[i + 2] );
            } else if ( std::string::npos != arg_copy.find( ".script" ) ) {
                filename = args[i];
            } else {
                osync::cerr( "Unknown flag. Only specify '-q' or '-r [start] [end]'.\n", osync::ansi_bred );
                return 1;
            }
        }
        if ( user_reqs.intervals.empty() ) {
            user_reqs.intervals.emplace_back( 0, 0 );
        }
        return time_script( filename, user_reqs );
    } catch ( std::exception &e ) {
        auto err = std::string( "Stat program failed with exception " ).append( e.what() ).append( "\n" );
        osync::cerr( err, osync::ansi_bred );
        return 1;
    }
}

int main( int argc, char **argv )
{
    auto args = std::span<const char *const>{ argv, static_cast<size_t>( argc ) }.subspan( 1 );
    if ( args.empty() ) {
        return 0;
    }
    return stats( args );
}

namespace {

int time_script( std::string_view script_name, interval_reqs &user_requests )
{
    std::optional<script::requests> s = script::parse_script( std::string( script_name ) );
    if ( !s ) {
        return 1;
    }
    if ( !validate_intervals( s.value(), user_requests ) ) {
        return 1;
    }
    std::optional<size_t> used_segment = time_allocator( s.value(), user_requests );
    if ( !used_segment ) {
        return 1;
    }
    auto peak = static_cast<double>( s.value().peak );
    auto used = static_cast<double>( used_segment.value() );
    auto util = std::to_string( ( 100.0 * peak ) / used ).append( "\n" );
    std::cout << util;
    return 0;
}

std::optional<size_t> time_allocator( script::requests &s, interval_reqs &user_requests )
{
    init_heap_segment( heap_size );
    if ( !myinit( heap_segment_start(), heap_segment_size() ) ) {
        osync::cerr( "Heap initialization failure.", osync::ansi_bred );
        return {};
    }
    void *heap_end = heap_segment_start();
    size_t cur_size = 0;
    int req = 0;
    size_t current_interval = 0;
    while ( req < s.lines.size() ) {
        if ( current_interval < user_requests.intervals.size()
             && user_requests.intervals[current_interval].start_req == req ) {
            const interval &section = user_requests.intervals[current_interval];
            double total_request_time = 0;
            for ( ; req < section.end_req; ++req ) {
                std::optional<script::heap_delta> request_time
                    = script::time_request( s.lines[req], s, cur_size, heap_end );
                if ( !request_time ) {
                    return {};
                }
                total_request_time += request_time.value().delta_time;
                cur_size = request_time.value().heap_size;
                s.peak = std::max( s.peak, cur_size );
            }
            double new_average = user_requests.interval_averages.emplace_back(
                total_request_time / static_cast<double>( section.end_req - section.start_req )
            );
            auto output = std::to_string( total_request_time )
                              .append( " " )
                              .append( std::to_string( new_average ) )
                              .append( "\n" );
            std::cout << output;
            ++current_interval;
            continue;
        }
        std::optional<size_t> new_size = script::exec_request( s.lines[req], s, cur_size, heap_end );
        if ( !new_size ) {
            return {};
        }
        cur_size = new_size.value();
        s.peak = std::max( s.peak, cur_size );
        ++req;
    }
    return static_cast<uint8_t *>( heap_end ) - static_cast<uint8_t *>( heap_segment_start() );
}

bool validate_intervals( script::requests &s, interval_reqs &user_requests )
{
    for ( size_t i = 0; i < user_requests.intervals.size(); ++i ) {
        if ( s.lines.size() - 1 < user_requests.intervals[i].start_req ) {
            auto err = std::string( "Interval out of range\n" )
                           .append( "Interval start: " )
                           .append( std::to_string( user_requests.intervals[i].start_req ) )
                           .append( "\nScript range: 1-" )
                           .append( std::to_string( s.lines.size() ) )
                           .append( "\n" );
            osync::cerr( err, osync::ansi_bred );
            return false;
        }
        if ( s.lines.size() - 1 < user_requests.intervals[i].end_req || 0 == user_requests.intervals[i].end_req ) {
            user_requests.intervals[i].end_req = static_cast<int>( s.lines.size() ) - 1;
        }
    }
    return true;
}

} // namespace
