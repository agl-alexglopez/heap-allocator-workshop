#include "matplot/core/legend.h"
#include "matplot/freestanding/axes_functions.h"
#include "matplot/freestanding/plot.h"
#include "matplot/util/handle_types.h"
// NOLINTNEXTLINE(*include-cleaner)
#include <matplot/matplot.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

#ifdef NDEBUG
constexpr std::string_view prog_path = "/build/rel";
#else
constexpr std::string_view prog_path = "/build/deb";
#endif

constexpr size_t default_worker_count = 4;
constexpr size_t max_cores = 20;

/// These are the commands that focus in on the key lines of the malloc free scripts to time.
constexpr std::array<std::array<std::array<std::string_view, 5>, 20>, 2> big_o_timing = { {
    { {
        { "-s", "10001", "-e", "20000", "scripts/time-insertdelete-05k.script" },
        { "-s", "20001", "-e", "40000", "scripts/time-insertdelete-10k.script" },
        { "-s", "30001", "-e", "60000", "scripts/time-insertdelete-15k.script" },
        { "-s", "40001", "-e", "80000", "scripts/time-insertdelete-20k.script" },
        { "-s", "50001", "-e", "100000", "scripts/time-insertdelete-25k.script" },
        { "-s", "60001", "-e", "120000", "scripts/time-insertdelete-30k.script" },
        { "-s", "70001", "-e", "140000", "scripts/time-insertdelete-35k.script" },
        { "-s", "80001", "-e", "160000", "scripts/time-insertdelete-40k.script" },
        { "-s", "90001", "-e", "180000", "scripts/time-insertdelete-45k.script" },
        { "-s", "100001", "-e", "200000", "scripts/time-insertdelete-50k.script" },
        { "-s", "110001", "-e", "220000", "scripts/time-insertdelete-55k.script" },
        { "-s", "120001", "-e", "240000", "scripts/time-insertdelete-60k.script" },
        { "-s", "130001", "-e", "260000", "scripts/time-insertdelete-65k.script" },
        { "-s", "140001", "-e", "280000", "scripts/time-insertdelete-70k.script" },
        { "-s", "150001", "-e", "300000", "scripts/time-insertdelete-75k.script" },
        { "-s", "160001", "-e", "320000", "scripts/time-insertdelete-80k.script" },
        { "-s", "170001", "-e", "340000", "scripts/time-insertdelete-85k.script" },
        { "-s", "180001", "-e", "360000", "scripts/time-insertdelete-90k.script" },
        { "-s", "190001", "-e", "380000", "scripts/time-insertdelete-95k.script" },
        { "-s", "200001", "-e", "400000", "scripts/time-insertdelete-100k.script" },
    } },
    { {
        { "-s", "15001", "-e", "20000", "scripts/time-reallocfree-05k.script" },
        { "-s", "30001", "-e", "40000", "scripts/time-reallocfree-10k.script" },
        { "-s", "45001", "-e", "60000", "scripts/time-reallocfree-15k.script" },
        { "-s", "60001", "-e", "80000", "scripts/time-reallocfree-20k.script" },
        { "-s", "75001", "-e", "100000", "scripts/time-reallocfree-25k.script" },
        { "-s", "90001", "-e", "120000", "scripts/time-reallocfree-30k.script" },
        { "-s", "105001", "-e", "140000", "scripts/time-reallocfree-35k.script" },
        { "-s", "120001", "-e", "160000", "scripts/time-reallocfree-40k.script" },
        { "-s", "135001", "-e", "180000", "scripts/time-reallocfree-45k.script" },
        { "-s", "150001", "-e", "200000", "scripts/time-reallocfree-50k.script" },
        { "-s", "165001", "-e", "220000", "scripts/time-reallocfree-55k.script" },
        { "-s", "180001", "-e", "240000", "scripts/time-reallocfree-60k.script" },
        { "-s", "195001", "-e", "260000", "scripts/time-reallocfree-65k.script" },
        { "-s", "210001", "-e", "280000", "scripts/time-reallocfree-70k.script" },
        { "-s", "225001", "-e", "300000", "scripts/time-reallocfree-75k.script" },
        { "-s", "240001", "-e", "320000", "scripts/time-reallocfree-80k.script" },
        { "-s", "255001", "-e", "340000", "scripts/time-reallocfree-85k.script" },
        { "-s", "270001", "-e", "360000", "scripts/time-reallocfree-90k.script" },
        { "-s", "285001", "-e", "380000", "scripts/time-reallocfree-95k.script" },
        { "-s", "300001", "-e", "400000", "scripts/time-reallocfree-100k.script" },
    } },

} };

constexpr std::array<std::string_view, 5> line_ticks = { "-o", "--", "-+", "-s", "-*" };
constexpr std::array<std::string_view, 9> loading_bar = { "⣿", "⣷", "⣯", "⣟", "⡿", "⢿", "⣻", "⣽", "⣾" };
constexpr std::string_view ansi_red_bold = "\033[38;5;9m";
constexpr std::string_view ansi_green_bold = "\033[38;5;10m";
constexpr std::string_view ansi_nil = "\033[0m";
constexpr std::string_view save_cursor = "\033[s";
constexpr std::string_view restore_cursor = "\033[u";

constexpr size_t loading_limit = 50;

using quantity_measurement = std::pair<std::vector<double>, std::vector<double>>;
using data_series = std::pair<std::string, std::vector<double>>;
using data_set = std::pair<std::vector<double>, std::vector<data_series>>;

struct process_result
{
    int process_id;
    ssize_t bytes_read;
};

struct path_bin
{
    std::string path;
    std::string bin;
};

enum heap_operation
{
    malloc_free,
    realloc_free,
};

enum data_set_type
{
    interval,
    response,
    utilization,
};

struct runtime_metrics
{
    data_set interval_speed;
    data_set average_response_time;
    data_set overall_utilization;
};

struct labels
{
    std::string_view title;
    std::string_view x_label;
    std::string_view y_label;
    std::string_view filename;
};

struct label_pack
{
    labels interval_labels;
    labels response_labels;
    labels utilization_labels;
};

struct plot_args
{
    heap_operation op{};
    labels interval_labels;
    labels response_labels;
    labels utilization_labels;
    size_t threads{ default_worker_count };
    bool quiet{ false };
    plot_args( heap_operation h, label_pack l, size_t threads, bool quiet )
        : op( h ), interval_labels( l.interval_labels ), response_labels( l.response_labels ),
          utilization_labels( l.utilization_labels ), threads( threads ), quiet( quiet )
    {}
    plot_args() = default;
};

const data_set &set( const runtime_metrics &m, data_set_type request )
{
    switch ( request ) {
    case interval:
        return m.interval_speed;
    case response:
        return m.average_response_time;
    case utilization:
        return m.overall_utilization;
    default: {
        std::cerr << "invalid request type for a data set that does not exist.\n";
        std::abort();
    }
    }
}

const std::vector<double> &x_axis( const data_set &s )
{
    return s.first;
}

std::vector<double> &x_axis( data_set &s )
{
    return s.first;
}

const std::vector<double> &series( const data_set &s, size_t series_index )
{
    return s.second[series_index].second;
}

std::vector<double> &series( data_set &s, size_t series_index )
{
    return s.second[series_index].second;
}

const std::vector<data_series> &all_series( const data_set &s )
{
    return s.second;
}

std::vector<data_series> &all_series( data_set &s )
{
    return s.second;
}

const std::string &title( const data_set &s, size_t title_index )
{
    return s.second[title_index].first;
}

std::string &title( data_set &s, size_t title_index )
{
    return s.second[title_index].first;
}

struct allocator_entry
{
    size_t index;
    double script_size;
};

/// The work we do to gather timing is trivially parallelizable. We just need a parent to
/// monitor this small stat generation program and enter the results. So we can have
/// threads become the parents for these parallel processes and they will just add the
/// stats to the runtim metrics container that has preallocated space for them.
/// Because the number of programs we time may grow in the future and the threads each
/// spawn a child process we have 2x the processes. Use a work queue to cap the processes
/// but still maintain consistent parallelism. Maybe allow -j[CORES] flag for user at CLI?
class command_queue {
    std::queue<std::optional<std::function<bool()>>> q_;
    std::mutex lk_;
    std::condition_variable wait_;
    std::vector<std::thread> workers_;
    void start()
    {
        for ( ;; ) {
            std::unique_lock<std::mutex> ul( lk_ );
            wait_.wait( ul, [this]() -> bool { return !q_.empty(); } );
            auto new_task = std::move( q_.front() );
            q_.pop();
            ul.unlock();
            if ( !new_task.has_value() ) {
                return;
            }
            if ( !new_task.value()() ) {
                std::cerr << "Error running requesting function.\n";
                return;
            }
        }
    }

  public:
    explicit command_queue( size_t num_workers )
    {
        for ( size_t i = 0; i < num_workers; ++i ) {
            workers_.emplace_back( [this]() { start(); } );
        }
    }
    ~command_queue()
    {
        for ( auto &w : workers_ ) {
            w.join();
        }
    }
    void push( std::optional<std::function<bool()>> args )
    {
        const std::unique_lock<std::mutex> ul( lk_ );
        q_.push( std::move( args ) );
        wait_.notify_one();
    }

    [[nodiscard]] bool empty()
    {
        const std::unique_lock<std::mutex> ul( lk_ );
        return q_.empty();
    }

    command_queue( const command_queue & ) = delete;
    command_queue &operator=( const command_queue & ) = delete;
    command_queue( command_queue && ) = delete;
    command_queue &operator=( command_queue && ) = delete;
};

/// Just for fun.
void wait( command_queue &q )
{
    size_t dist = 0;
    bool max_loading_bar = false;
    std::cout << ansi_red_bold;
    while ( !q.empty() ) {
        std::cout << save_cursor;
        for ( size_t i = 0; i < loading_limit; ++i ) {
            std::cout << loading_bar.at( ( i + dist ) % loading_bar.size() ) << std::flush;
            if ( !max_loading_bar && i > dist ) {
                break;
            }
        }
        std::cout << restore_cursor;
        ++dist;
        max_loading_bar = max_loading_bar || dist == 0;
        std::this_thread::sleep_for( std::chrono::milliseconds( 60 ) );
    }
    std::cout << ansi_green_bold;
    for ( size_t i = 0; i < loading_limit; ++i ) {
        std::cout << loading_bar.at( ( i + dist ) % loading_bar.size() ) << std::flush;
    }
    std::cout << "\n";
}

std::vector<path_bin> gather_timer_programs()
{
    std::vector<path_bin> commands{};
    std::string path = std::filesystem::current_path();
    path.append( prog_path );
    for ( const auto &entry : std::filesystem::directory_iterator( path ) ) {
        const std::string as_str = entry.path();
        const size_t last_entry = as_str.find_last_of( '/' );
        const std::string_view is_timer = std::string_view{ as_str }.substr( last_entry + 1 );
        if ( is_timer.starts_with( "stats_" ) ) {
            commands.emplace_back( as_str, std::string( is_timer ) );
        }
    }
    return commands;
}

bool close_process( process_result res )
{
    close( res.process_id );
    int err = 0;
    const int wait_err = waitpid( -1, &err, 0 );
    if ( WIFSIGNALED( err ) && WTERMSIG( err ) == SIGSEGV ) { // NOLINT
        std::cerr << "seg fault waitpid returned " << wait_err << "\n";
        return false;
    }
    if ( res.bytes_read == -1 ) {
        std::cerr << "read error\n";
        return false;
    }
    return true;
}

double parse_quantity_n( std::string_view script_name )
{
    const size_t last_dash = script_name.find_last_of( '-' ) + 1;
    const size_t last_k = script_name.find_last_of( 'k' );
    const std::string num = std::string( script_name.substr( last_dash, last_k - last_dash ) );
    try {
        return std::stod( num ) * 1000;
    } catch ( std::invalid_argument &a ) {
        std::cerr << "Caught invalid argument: " << a.what() << "\n";
        return 0;
    }
}

bool parse_metrics( const std::string &output, const size_t allocator_index, runtime_metrics &m )
{
    try {
        const size_t first_space = output.find_first_of( ' ' );
        const size_t first_newline = output.find_first_of( '\n' );
        const std::string interval_time = output.substr( 0, first_space );
        series( m.interval_speed, allocator_index ).emplace_back( stod( interval_time ) );
        const std::string response_average = output.substr( first_space, first_newline - first_space );
        series( m.average_response_time, allocator_index ).emplace_back( stod( response_average ) );
        const std::string utilization
            = output.substr( first_newline + 1, output.find_last_of( '%' ) - ( first_newline + 1 ) );
        series( m.overall_utilization, allocator_index ).emplace_back( stod( utilization ) );
        return true;
    } catch ( std::invalid_argument &a ) {
        std::cerr << "Error parsing string input to metrics\n";
        return false;
    }
}

int allocator_stats_subprocess( std::string_view cmd_path, const std::array<std::string_view, 6> &args )
{
    std::array<int, 2> comms{ 0, 0 };
    if ( pipe2( comms.data(), O_CLOEXEC ) < 0 ) {
        std::cerr << "could not open pipe for communication\n";
        std::abort();
    }
    if ( fork() != 0 ) {
        close( comms[1] );
        return comms[0];
    }
    close( comms[0] );
    if ( dup2( comms[1], STDOUT_FILENO ) < 0 ) {
        std::cerr << "child cannot communicate to parent.\n";
        close( comms[1] );
        exit( 1 );
    }
    std::array<const char *, 7> arg_copy{};
    for ( size_t i = 0; i < args.size(); ++i ) {
        arg_copy.at( i ) = args.at( i ).data();
    }
    arg_copy.back() = nullptr;
    execv( cmd_path.data(), const_cast<char *const *>( arg_copy.data() ) ); // NOLINT
    std::cerr << "child process failed abnormally errno: " << errno << "\n";
    close( comms[1] );
    exit( 1 );
}

bool thread_fill_data( const size_t allocator_index, const path_bin &cmd, runtime_metrics &m, heap_operation s )
{
    const std::string title = cmd.bin.substr( cmd.bin.find( '_' ) + 1 );
    for ( const auto &args : big_o_timing.at( s ) ) {
        std::string cur_path = std::filesystem::current_path();
        cur_path.append( "/" ).append( args[4] );
        const int process = allocator_stats_subprocess(
            cmd.path,
            {
                cmd.bin,
                args[0],
                args[1],
                args[2],
                args[3],
                cur_path,
            }
        );
        std::vector<char> vec_buf( 64 );
        ssize_t bytes_read = 0;
        size_t progress = 0;
        while ( ( bytes_read = read( process, &vec_buf.at( progress ), 64 ) ) > 0 ) {
            progress += bytes_read;
        }
        if ( !close_process( { process, bytes_read } ) ) {
            std::cerr << "This thread is quitting early, child subprocess failed\n";
            return false;
        }
        const std::string data = std::string( vec_buf.data() );
        if ( !parse_metrics( data, allocator_index, m ) ) {
            std::cerr << "This thread is quitting early due to parsing error\n";
            return false;
        }
    }
    return true;
}

void line_plot_stats( const runtime_metrics &m, data_set_type t, labels l, [[maybe_unused]] bool quiet )
{
    matplot::title( l.title );
    matplot::xlabel( l.x_label );
    matplot::ylabel( l.y_label );
    matplot::grid( true );
    size_t tick_style = 0;
    for ( const auto &allocator : set( m, t ).second ) {
        matplot::plot( x_axis( m.overall_utilization ), allocator.second, line_ticks.at( tick_style ) )
            ->line_width( 2 );
        ::matplot::legend()->strings().back() = allocator.first;
        matplot::hold( true );
        ++tick_style %= line_ticks.size();
    }
    ::matplot::legend()->location( matplot::legend::general_alignment::topleft );
    matplot::save( std::string( l.filename ) );
    matplot::hold( false );
    matplot::show();
}

void plot_runtime( const std::vector<path_bin> &commands, plot_args args )
{
    runtime_metrics m{};
    x_axis( m.interval_speed ).push_back( 0 );
    x_axis( m.average_response_time ).push_back( 0 );
    x_axis( m.overall_utilization ).push_back( 0 );
    for ( const auto &args : big_o_timing.at( args.op ) ) {
        const double script_size = parse_quantity_n( args.back() );
        if ( script_size == 0 ) {
            std::cerr << "could not parse script size\n";
            return;
        }
        x_axis( m.interval_speed ).push_back( script_size );
        x_axis( m.average_response_time ).push_back( script_size );
        x_axis( m.overall_utilization ).push_back( script_size );
    }
    for ( const auto &c : commands ) {
        std::string title = c.bin.substr( c.bin.find( '_' ) + 1 );
        // Matplot reads underscores as subscripts which messes up legends on graphs. Change to space.
        std::replace( title.begin(), title.end(), '_', ' ' );
        all_series( m.interval_speed ).push_back( { title, { 0.0 } } );
        all_series( m.interval_speed ).back().second.reserve( big_o_timing.at( args.op ).size() );
        all_series( m.average_response_time ).push_back( { title, { 0.0 } } );
        all_series( m.average_response_time ).back().second.reserve( big_o_timing.at( args.op ).size() );
        all_series( m.overall_utilization ).push_back( { title, { 0.0 } } );
        all_series( m.overall_utilization ).back().second.reserve( big_o_timing.at( args.op ).size() );
    }
    command_queue workers( args.threads );
    for ( size_t i = 0; i < commands.size(); ++i ) {
        // Not sure if bool is ok here but the filesystem and subproccesses can often fail so signal with
        // bool for easier thread shutdown. Futures could maybe work but seems overkill?
        workers.push( [i, &args, &commands, &m]() -> bool {
            return thread_fill_data( i, commands[i], m, args.op );
        } );
    }
    for ( size_t i = 0; i < args.threads; ++i ) {
        // Threads still wait if queue is empty so send a quit signal.
        workers.push( {} );
    }
    // Don't mind this function it's just some cursor animation while we wait. But don't put anything before it!
    wait( workers );
    line_plot_stats( m, data_set_type::interval, args.interval_labels, args.quiet );
    line_plot_stats( m, data_set_type::response, args.response_labels, args.quiet );
    line_plot_stats( m, data_set_type::utilization, args.utilization_labels, args.quiet );
}

} // namespace

int main( int argc, char **argv )
{
    const std::vector<path_bin> commands = gather_timer_programs();
    plot_args args{};
    for ( const auto &arg : std::span<const char *const>( argv, argc ).subspan( 1 ) ) {
        const std::string arg_copy = std::string( arg );
        if ( arg_copy == "-realloc" ) {
            args.op = heap_operation::realloc_free;
        } else if ( arg_copy == "-malloc" ) {
            args.op = heap_operation::malloc_free;
        } else if ( arg_copy.starts_with( "-j" ) ) {
            if ( arg_copy == "-j" ) {
                std::cerr << "Invalid core count requested. Did you mean -j[CORES] without a space?\n";
                return 1;
            }
            std::string cores
                = std::string( std::string_view{ arg_copy }.substr( arg_copy.find_first_not_of( "-j" ) ) );
            try {
                // We spawn a process for each thread which means 2x processes so divide the request in half.
                args.threads = std::stoull( cores ) / 2;
                if ( args.threads == 0 || args.threads > max_cores ) {
                    args.threads = default_worker_count;
                }
            } catch ( std::invalid_argument &e ) {
                std::cerr << "Invalid core count requested from " << e.what() << ": " << cores << "\n";
                return 1;
            }
        } else if ( arg_copy == "-q" ) {
            args.quiet = true;
        } else {
            std::cerr << "Invalid command line request: " << arg_copy << "\n";
            return 1;
        }
    }
    if ( args.op == heap_operation::malloc_free ) {
        plot_runtime(
            commands,
            plot_args(
                heap_operation::malloc_free,
                {
                    .interval_labels{
                        "Time to Complete N Requests",
                        "N Malloc N Free Requests",
                        "Time(ms) to Complete Interval",
                        "output/mallocfree_interval.svg",
                    },
                    .response_labels{
                        "Average Response Time per Request",
                        "N Malloc N Free Requests",
                        "Average Time(ms) per Request",
                        "output/mallocfree_response.svg",
                    },
                    .utilization_labels{
                        "Average Utilization",
                        "N Malloc N Free Requests",
                        "Time(ms)",
                        "output/mallocfree_utilization.svg",
                    },
                },
                args.threads,
                args.quiet
            )
        );
    } else if ( args.op == heap_operation::realloc_free ) {
        plot_runtime(
            commands,
            plot_args(
                heap_operation::realloc_free,
                {
                    .interval_labels{
                        "Time to Complete N Requests",
                        "N Realloc Requests",
                        "Time(ms) to Complete Interval",
                        "output/realloc_interval.svg",
                    },
                    .response_labels{
                        "Average Response Time per Request",
                        "N Realloc Requests",
                        "Average Time(ms) per Request",
                        "output/realloc_response.svg",
                    },
                    .utilization_labels{
                        "Average Utilization",
                        "N Realloc Requests",
                        "Time(ms)",
                        "output/realloc_utilization.svg",
                    },
                },
                args.threads,
                args.quiet
            )
        );
    } else {
        std::cerr << "invalid options slipped through command line args.\n";
        return 1;
    }
    return 0;
}
