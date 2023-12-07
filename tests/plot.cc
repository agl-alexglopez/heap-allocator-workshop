#include "command_queue.hh"
#include "osync.hh"

#include "matplot/core/legend.h"
#include "matplot/freestanding/axes_functions.h"
#include "matplot/util/handle_types.h"
// NOLINTNEXTLINE(*include-cleaner)
#include <matplot/matplot.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <optional>
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
constexpr size_t starting_buf_size = 64;

/// The script commands are carefully tuned to only time sections where the desired behavior is operating.
constexpr std::array<std::array<std::array<std::string_view, 4>, 20>, 2> big_o_timing = { {
    { {
        // Malloc and free commands are targeted at making many uncoalescable free nodes and mallocing them.
        { "-r", "10001", "20000", "scripts/time-insertdelete-05k.script" },
        { "-r", "20001", "40000", "scripts/time-insertdelete-10k.script" },
        { "-r", "30001", "60000", "scripts/time-insertdelete-15k.script" },
        { "-r", "40001", "80000", "scripts/time-insertdelete-20k.script" },
        { "-r", "50001", "100000", "scripts/time-insertdelete-25k.script" },
        { "-r", "60001", "120000", "scripts/time-insertdelete-30k.script" },
        { "-r", "70001", "140000", "scripts/time-insertdelete-35k.script" },
        { "-r", "80001", "160000", "scripts/time-insertdelete-40k.script" },
        { "-r", "90001", "180000", "scripts/time-insertdelete-45k.script" },
        { "-r", "100001", "200000", "scripts/time-insertdelete-50k.script" },
        { "-r", "110001", "220000", "scripts/time-insertdelete-55k.script" },
        { "-r", "120001", "240000", "scripts/time-insertdelete-60k.script" },
        { "-r", "130001", "260000", "scripts/time-insertdelete-65k.script" },
        { "-r", "140001", "280000", "scripts/time-insertdelete-70k.script" },
        { "-r", "150001", "300000", "scripts/time-insertdelete-75k.script" },
        { "-r", "160001", "320000", "scripts/time-insertdelete-80k.script" },
        { "-r", "170001", "340000", "scripts/time-insertdelete-85k.script" },
        { "-r", "180001", "360000", "scripts/time-insertdelete-90k.script" },
        { "-r", "190001", "380000", "scripts/time-insertdelete-95k.script" },
        { "-r", "200001", "400000", "scripts/time-insertdelete-100k.script" },
    } },
    { {
        // Realloc commands are targeted at reallocing many allocated nodes that are surrounded by free nodes.
        { "-r", "15001", "20000", "scripts/time-reallocfree-05k.script" },
        { "-r", "30001", "40000", "scripts/time-reallocfree-10k.script" },
        { "-r", "45001", "60000", "scripts/time-reallocfree-15k.script" },
        { "-r", "60001", "80000", "scripts/time-reallocfree-20k.script" },
        { "-r", "75001", "100000", "scripts/time-reallocfree-25k.script" },
        { "-r", "90001", "120000", "scripts/time-reallocfree-30k.script" },
        { "-r", "105001", "140000", "scripts/time-reallocfree-35k.script" },
        { "-r", "120001", "160000", "scripts/time-reallocfree-40k.script" },
        { "-r", "135001", "180000", "scripts/time-reallocfree-45k.script" },
        { "-r", "150001", "200000", "scripts/time-reallocfree-50k.script" },
        { "-r", "165001", "220000", "scripts/time-reallocfree-55k.script" },
        { "-r", "180001", "240000", "scripts/time-reallocfree-60k.script" },
        { "-r", "195001", "260000", "scripts/time-reallocfree-65k.script" },
        { "-r", "210001", "280000", "scripts/time-reallocfree-70k.script" },
        { "-r", "225001", "300000", "scripts/time-reallocfree-75k.script" },
        { "-r", "240001", "320000", "scripts/time-reallocfree-80k.script" },
        { "-r", "255001", "340000", "scripts/time-reallocfree-85k.script" },
        { "-r", "270001", "360000", "scripts/time-reallocfree-90k.script" },
        { "-r", "285001", "380000", "scripts/time-reallocfree-95k.script" },
        { "-r", "300001", "400000", "scripts/time-reallocfree-100k.script" },
    } },

} };

constexpr std::array<std::string_view, 5> line_ticks = { "-o", "--", "-+", "-s", "-*" };
constexpr std::array<std::string_view, 9> loading_bar = { "⣿", "⣷", "⣯", "⣟", "⡿", "⢿", "⣻", "⣽", "⣾" };
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
    script_comparison,
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
    std::optional<std::string> script_name;
    plot_args( heap_operation h, label_pack l, size_t threads, bool quiet )
        : op( h ), interval_labels( l.interval_labels ), response_labels( l.response_labels ),
          utilization_labels( l.utilization_labels ), threads( threads ), quiet( quiet )
    {}
    plot_args() = default;
};

struct allocator_entry
{
    size_t index;
    double script_size;
};

std::vector<path_bin> gather_timer_programs();
void run_bigo_analysis( const std::vector<path_bin> &commands, const plot_args &args );
void plot_script_comparison( const std::vector<path_bin> &commands, plot_args &args );
const data_set &set( const runtime_metrics &m, data_set_type request );
const std::vector<double> &x_axis( const data_set &s );
std::vector<double> &x_axis( data_set &s );
const std::vector<double> &series( const data_set &s, size_t series_index );
std::vector<double> &series( data_set &s, size_t series_index );
const std::vector<data_series> &all_series( const data_set &s );
std::vector<data_series> &all_series( data_set &s );
const std::string &title( const data_set &s, size_t title_index );
std::string &title( data_set &s, size_t title_index );
void twiddle_cursor( command_queue &q );
bool close_process( process_result res );
double parse_quantity_n( std::string_view script_name );
bool parse_metrics( const std::string &output, size_t allocator_index, runtime_metrics &m );
int start_subprocess( std::string_view cmd_path, const std::vector<std::string_view> &args );
bool thread_run_cmd(
    size_t allocator_index, const path_bin &cmd, runtime_metrics &m, std::vector<std::string_view> cmd_list
);
std::optional<size_t> specify_threads( std::string_view thread_request );
bool thread_run_analysis( size_t allocator_index, const path_bin &cmd, runtime_metrics &m, heap_operation s );
void line_plot_stats( const runtime_metrics &m, data_set_type t, labels l, bool quiet );
void bar_chart_stats( const runtime_metrics &m, data_set_type t, labels l, bool quiet );
void plot_runtime( const std::vector<path_bin> &commands, plot_args args );

} // namespace

//////////////////////////////////    User Argument Handling     ///////////////////////////////////

int plot( std::span<const char *const> cli_args )
{
    try {
        const std::vector<path_bin> commands = gather_timer_programs();
        if ( commands.empty() ) {
            return 1;
        }
        plot_args args{};
        for ( const auto *arg : cli_args ) {
            const std::string arg_copy = std::string( arg );
            if ( arg_copy == "-realloc" ) {
                args.op = heap_operation::realloc_free;
            } else if ( arg_copy == "-malloc" ) {
                args.op = heap_operation::malloc_free;
            } else if ( arg_copy.starts_with( "-j" ) ) {
                std::optional<size_t> threads = specify_threads( arg_copy );
                if ( !threads ) {
                    return 1;
                }
                args.threads = threads.value();
            } else if ( arg_copy == "-q" ) {
                args.quiet = true;
            } else if ( arg_copy.find( '/' ) != std::string::npos ) {
                args.op = heap_operation::script_comparison;
                args.script_name.emplace( arg_copy );
            } else {
                auto err = std::string( "Invalid command line request: " ).append( arg_copy ).append( "\n" );
                osync::syncerr( err, osync::ansi_bred );
                return 1;
            }
        }
        if ( args.op == heap_operation::script_comparison ) {
            plot_script_comparison( commands, args );
            return 0;
        }
        run_bigo_analysis( commands, args );
        return 0;
    } catch ( std::exception &e ) {
        auto err = std::string( "Plot program caught exception " ).append( e.what() ).append( "\n" );
        osync::syncerr( err, osync::ansi_bred );
        return 1;
    }
}

int main( int argc, char *argv[] )
{
    auto cli_args = std::span<const char *const>( argv, argc ).subspan( 1 );
    if ( cli_args.empty() ) {
        return 0;
    }
    return plot( cli_args );
}

///////////////////////////       Performance Testing Implementation    ////////////////////////////

namespace {

/////////////////////////  Threading Subproccesses and File Handling   ////////////////////////////////////////

void run_bigo_analysis( const std::vector<path_bin> &commands, const plot_args &args )
{
    if ( args.op == heap_operation::malloc_free ) {
        plot_runtime(
            commands,
            plot_args(
                heap_operation::malloc_free,
                {
                    .interval_labels{
                        "Time(ms) to Complete N Requests",
                        "N Malloc N Free Requests",
                        "Time(ms) to Complete Interval",
                        "output/mallocfree-interval.svg",
                    },
                    .response_labels{
                        "Average Response Time(ms) per Request",
                        "N Malloc N Free Requests",
                        "Average Time(ms) per Request",
                        "output/mallocfree-response.svg",
                    },
                    .utilization_labels{
                        "Utilization % (libc excluded)",
                        "N Malloc N Free Requests",
                        "Percent %",
                        "output/mallocfree-utilization.svg",
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
                        "Time(ms) to Complete N Requests",
                        "N Realloc Requests",
                        "Time(ms) to Complete Interval",
                        "output/realloc-interval.svg",
                    },
                    .response_labels{
                        "Average Response Time(ms) per Request",
                        "N Realloc Requests",
                        "Average Time(ms) per Request",
                        "output/realloc-response.svg",
                    },
                    .utilization_labels{
                        "Utilization % (libc excluded)",
                        "N Realloc Requests",
                        "Percent %",
                        "output/realloc-utilization.svg",
                    },
                },
                args.threads,
                args.quiet
            )
        );
    } else {
        osync::syncerr( "invalid options slipped through command line args.\n", osync::ansi_bred );
    }
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
            osync::syncerr( "could not parse script size\n", osync::ansi_bred );
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
            return thread_run_analysis( i, commands[i], m, args.op );
        } );
    }
    for ( size_t i = 0; i < args.threads; ++i ) {
        // Threads still wait if queue is empty so send a quit signal.
        workers.push( {} );
    }
    // This is just some cursor animation while we wait from main thread. But don't put anything before it!
    twiddle_cursor( workers );
    line_plot_stats( m, data_set_type::interval, args.interval_labels, args.quiet );
    line_plot_stats( m, data_set_type::response, args.response_labels, args.quiet );
    line_plot_stats( m, data_set_type::utilization, args.utilization_labels, args.quiet );
    std::cout << osync::ansi_nil;
}

bool thread_run_analysis( const size_t allocator_index, const path_bin &cmd, runtime_metrics &m, heap_operation s )
{
    const std::string title = cmd.bin.substr( cmd.bin.find( '_' ) + 1 );
    for ( const auto &args : big_o_timing.at( s ) ) {
        thread_run_cmd( allocator_index, cmd, m, std::vector<std::string_view>( args.begin(), args.end() ) );
    }
    return true;
}

void plot_script_comparison( const std::vector<path_bin> &commands, plot_args &args )
{
    runtime_metrics m{};
    std::string_view path_to_script{ args.script_name.value() };
    std::string script( path_to_script.substr( path_to_script.find_last_of( '/' ) + 1 ) );
    script.erase( script.find_last_of( '.' ) );
    std::string save_interval = std::string( "output/interval-" ).append( script ).append( ".svg" );
    std::string save_response = std::string( "output/response-" ).append( script ).append( ".svg" );
    std::string save_utilization = std::string( "output/utilization-" ).append( script ).append( ".svg" );
    args.interval_labels = {
        .title = "Time(ms) to Complete Script",
        .x_label = "Allocators",
        .y_label = "Time(ms)",
        .filename = save_interval,
    };
    args.response_labels = {
        .title = "Average Response Time(ms) during Script",
        .x_label = "Allocators",
        .y_label = "Time(ms)",
        .filename = save_response,
    };
    args.utilization_labels = {
        .title = "Utilization % (libc excluded)",
        .x_label = "Allocators",
        .y_label = "Percent %",
        .filename = save_utilization,
    };
    for ( const auto &c : commands ) {
        std::string title = c.bin.substr( c.bin.find( '_' ) + 1 );
        // Matplot reads underscores as subscripts which messes up legends on graphs. Change to space.
        std::replace( title.begin(), title.end(), '_', ' ' );
        all_series( m.interval_speed ).push_back( { title, {} } );
        all_series( m.average_response_time ).push_back( { title, {} } );
        all_series( m.overall_utilization ).push_back( { title, {} } );
    }
    command_queue workers( args.threads );
    for ( size_t i = 0; i < commands.size(); ++i ) {
        // Not sure if bool is ok here but the filesystem and subproccesses can often fail so signal with
        // bool for easier thread shutdown. Futures could maybe work but seems overkill?
        workers.push( [i, &args, &commands, &m]() -> bool {
            return thread_run_cmd( i, commands[i], m, { args.script_name.value() } );
        } );
    }
    for ( size_t i = 0; i < args.threads; ++i ) {
        // Threads still wait if queue is empty so send a quit signal.
        workers.push( {} );
    }
    // This is just some cursor animation while we wait from main thread. But don't put anything before it!
    twiddle_cursor( workers );
    bar_chart_stats( m, data_set_type::interval, args.interval_labels, args.quiet );
    bar_chart_stats( m, data_set_type::response, args.response_labels, args.quiet );
    bar_chart_stats( m, data_set_type::utilization, args.utilization_labels, args.quiet );
}

bool thread_run_cmd(
    const size_t allocator_index, const path_bin &cmd, runtime_metrics &m, std::vector<std::string_view> cmd_list
)
{
    // I have had more success with subprocesses when using full paths but maybe not necessary?
    std::string cur_path = std::filesystem::current_path();
    cur_path.append( "/" ).append( cmd_list.back() );
    std::vector<std::string_view> commands{ cmd.bin };
    commands.insert( commands.end(), cmd_list.begin(), cmd_list.end() );
    const int process = start_subprocess( cmd.path, commands );
    // Output from the program we use is two lines max.
    size_t remaining = starting_buf_size;
    std::vector<char> vec_buf( remaining );
    ssize_t bytes_read = 0;
    size_t progress = 0;
    while ( ( bytes_read = read( process, &vec_buf.at( progress ), remaining ) ) > 0 ) {
        progress += bytes_read;
        remaining -= bytes_read;
        if ( remaining == 0 ) {
            remaining = vec_buf.size() * 2;
            vec_buf.resize( vec_buf.size() * 2 );
        }
    }
    if ( !close_process( { process, bytes_read } ) ) {
        // Many threads may output error messages so we will always sync cerr to aid legibility.
        osync::syncerr( "This thread is quitting early, child subprocess failed\n", osync::ansi_bred );
        return false;
    }
    const std::string data = std::string( vec_buf.data() );
    if ( !parse_metrics( data, allocator_index, m ) ) {
        osync::syncerr( "This thread is quitting early due to parsing error\n", osync::ansi_bred );
        return false;
    }
    return true;
}

int start_subprocess( std::string_view cmd_path, const std::vector<std::string_view> &args )
{
    std::array<int, 2> comms{ 0, 0 };
    if ( pipe2( comms.data(), O_CLOEXEC ) < 0 ) {
        osync::syncerr( "Could not open pipe for communication\n", osync::ansi_bred );
        return -1;
    }
    if ( fork() != 0 ) {
        close( comms[1] );
        return comms[0];
    }
    close( comms[0] );
    if ( dup2( comms[1], STDOUT_FILENO ) < 0 ) {
        osync::syncerr( "Child cannot communicate to parent.\n", osync::ansi_bred );
        close( comms[1] );
        exit( 1 );
    }
    std::vector<const char *> arg_copy{};
    arg_copy.reserve( args.size() + 1 );
    for ( const auto &arg : args ) {
        arg_copy.push_back( arg.data() );
    }
    arg_copy.push_back( nullptr );
    // Is this even safe? Passing arguments from c++ to execv was awful. Not sure the safesest way.
    execv( cmd_path.data(), const_cast<char *const *>( arg_copy.data() ) ); // NOLINT
    auto err
        = std::string( "Child process failed abnormally errno: " ).append( std::to_string( errno ) ).append( "\n" );
    osync::syncerr( err, osync::ansi_bred );
    close( comms[1] );
    exit( 1 );
}

bool close_process( process_result res )
{
    close( res.process_id );
    int err = 0;
    const int wait_err = waitpid( -1, &err, 0 );
    if ( WIFSIGNALED( err ) && WTERMSIG( err ) == SIGSEGV ) { // NOLINT
        auto err = std::string( "Seg fault waitpid returned " ).append( std::to_string( wait_err ) ).append( "\n" );
        osync::syncerr( err, osync::ansi_bred );
        return false;
    }
    if ( res.bytes_read == -1 ) {
        osync::syncerr( "read error\n", osync::ansi_bred );
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
        auto err = std::string( "Caught invalid argument: " ).append( a.what() ).append( "\n" );
        osync::syncerr( err, osync::ansi_bred );
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
        osync::syncerr( "Error parsing string input to metrics\n", osync::ansi_bred );
        return false;
    }
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

//////////////////////////////   Plotting with Matplot++    ///////////////////////////////////////////

void line_plot_stats( const runtime_metrics &m, data_set_type t, labels l, bool quiet )
{
    // From what I can tell to get the "figure" we specify true for quiet mode so not every change causes a redraw.
    auto p = matplot::gcf( true );
    // The axes_object is the core "object" we are working with that will frame "plots".
    auto axes = p->current_axes();
    axes->title( l.title );
    axes->xlabel( l.x_label );
    axes->grid( true );
    axes->font_size( 14.0 );
    size_t tick_style = 0;
    for ( const auto &allocator : set( m, t ).second ) {
        if ( allocator.first == "list segregated" ) {
            continue;
        }
        // Now we grab a "plot" where we want to put our actual data with x data and y data.
        auto plot = axes->plot( x_axis( m.overall_utilization ), allocator.second, line_ticks.at( tick_style ) );
        plot->line_width( 2 );
        // It is always sketchy to use the .back() function, but I am not sure if there is a better associative way.
        if ( ::matplot::legend()->strings().empty() ) {
            osync::syncerr( "No legend entry generated for matplot++ plot. Has API changed?\n", osync::ansi_bred );
            return;
        }
        // It seems the legend is freestanding but still associated with the current axes_object. Not plot!
        // So a hidden legend entry is automatically generated when we add a plot. Edit this with correct name.
        ::matplot::legend()->strings().back() = allocator.first;
        // Need to plot the other allocator lines, not just one, so hold the plots in between then plot all at end.
        matplot::hold( true );
        ++tick_style %= line_ticks.size();
    }
    ::matplot::legend()->location( matplot::legend::general_alignment::bottomright );
    ::matplot::legend()->num_rows( 3 );
    ::matplot::legend()->box( false );
    ::matplot::legend()->font_size( 12.0 );
    p->size( 1920, 1080 );
    p->save( std::string( l.filename ) );
    matplot::hold( false );
    auto msg = std::string( "plot saved: " ).append( l.filename ).append( "\n" );
    std::cout << msg;
    if ( quiet ) {
        return;
    }
    // Because we are in quiet mode hopefully we have had no flashing and the this will be the only plot shown.
    p->show();
}

void bar_chart_stats( const runtime_metrics &m, data_set_type t, labels l, bool quiet )
{
    // From what I can tell to get the "figure" we specify true for quiet mode so not every change causes a redraw.
    auto p = matplot::gcf( true );
    // The axes_object is the core "object" we are working with that will frame "plots".
    auto axes = p->current_axes();
    axes->title( l.title );
    axes->xlabel( l.x_label );
    axes->grid( true );
    axes->font_size( 14.0 );
    size_t tick_style = 0;
    std::vector<std::string> tick_labels{};
    std::vector<double> bar_data{};
    for ( const auto &allocator : set( m, t ).second ) {
        // Now we grab a "plot" where we want to put our actual data with x data and y data.
        bar_data.push_back( allocator.second.front() );
        tick_labels.push_back( allocator.first );
        ++tick_style %= line_ticks.size();
    }
    auto plot = axes->bar( bar_data );
    axes->x_axis().ticklabels( tick_labels );
    axes->x_axis().tickangle( 20.0 );
    p->size( 1920, 1080 );
    p->save( std::string( l.filename ) );
    auto msg = std::string( "plot saved: " ).append( l.filename ).append( "\n" );
    std::cout << msg;
    if ( quiet ) {
        return;
    }
    // Because we are in quiet mode hopefully we have had no flashing and the this will be the only plot shown.
    p->show();
}

std::optional<size_t> specify_threads( std::string_view thread_request )
{
    if ( thread_request == "-j" ) {
        osync::syncerr(
            "Invalid core count requested. Did you mean -j[CORES] without a space?\n", osync::ansi_bred
        );
        return 1;
    }
    std::string cores
        = std::string( std::string_view{ thread_request }.substr( thread_request.find_first_not_of( "-j" ) ) );
    try {
        // We spawn a process for each thread which means 2x processes so divide the request in half.
        size_t result = std::stoull( cores ) / 2;
        if ( result == 0 || result > max_cores ) {
            result = default_worker_count;
        }
        return result;
    } catch ( std::invalid_argument &e ) {
        auto err = std::string( "Invalid core count requested from " )
                       .append( e.what() )
                       .append( ": " )
                       .append( cores )
                       .append( "\n" );
        osync::syncerr( err, osync::ansi_bred );
        return {};
    }
}

//////////////////////////////    Helpers to Access Data in Types    //////////////////////////////////////

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
        osync::syncerr( "invalid request type for a data set that does not exist.\n", osync::ansi_bred );
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

/////////////////////////////             Just for Fun             //////////////////////////////////////

void twiddle_cursor( command_queue &q )
{
    size_t dist = 0;
    bool max_loading_bar = false;
    std::cout << osync::ansi_bred;
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
    std::cout << osync::ansi_bgrn;
    for ( size_t i = 0; i < loading_limit; ++i ) {
        std::cout << loading_bar.at( ( i + dist ) % loading_bar.size() );
    }
    std::cout << "\n";
}

} // namespace
