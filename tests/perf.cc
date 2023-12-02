#include <array>
#include <cerrno>
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
#include <string>
#include <string_view>
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

struct big_o_metrics
{
    std::vector<std::pair<std::string, std::vector<std::pair<size_t, double>>>> interval_speed;
    std::vector<std::pair<std::string, std::vector<std::pair<size_t, double>>>> average_response_time;
    std::vector<std::pair<std::string, std::vector<std::pair<size_t, double>>>> overall_utilization;
};

struct allocator_entry
{
    size_t index;
    size_t script_size;
};

constexpr size_t worker_limit = 4;

/// These are the commands that focus in on the key lines of the malloc free scripts to time.
constexpr std::array<std::array<std::string_view, 5>, 20> increasing_malloc_free_size_args = { {
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
} };

constexpr std::array<std::array<std::string_view, 5>, 20> increasing_realloc_free_size_args = { {
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
} };

/// The work we do to gather timing is trivially parallelizable. We just need a parent to
/// monitor this small stat generation program and enter the results. So we can have
/// threads become the parents for these parallel processes and they will just add the
/// stats to the big_o_metrics container that has preallocated space for them. All good.
/// Because the number of programs we time may grow in the future and the threads each
/// spawn a child process we have 2x the processes. Use a work queue to cap the processes.
class command_queue {
    std::queue<std::optional<std::function<void()>>> q_;
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
            new_task.value()();
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
    void push( std::optional<std::function<void()>> args )
    {
        std::unique_lock<std::mutex> ul( lk_ );
        q_.push( std::move( args ) );
        wait_.notify_one();
    }
    command_queue( const command_queue & ) = delete;
    command_queue &operator=( const command_queue & ) = delete;
    command_queue( command_queue && ) = delete;
    command_queue &operator=( command_queue && ) = delete;
};

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
void close_process( process_result res )
{
    close( res.process_id );
    int err = 0;
    waitpid( -1, &err, 0 );
    if ( WIFSIGNALED( err ) && WTERMSIG( err ) == SIGSEGV ) { // NOLINT
        std::cerr << "seg fault\n";
        std::abort();
    }
    if ( res.bytes_read == -1 ) {
        std::cerr << "read error\n";
        std::abort();
    }
}

size_t parse_quantity_n( std::string_view script_name )
{
    const size_t last_dash = script_name.find_last_of( '-' ) + 1;
    const size_t last_k = script_name.find_last_of( 'k' );
    const std::string num = std::string( script_name.substr( last_dash, last_k - last_dash ) );
    return std::stoull( num ) * 1000;
}

void parse_metrics( const std::string &output, const allocator_entry &entry, big_o_metrics &m )
{
    const size_t first_space = output.find_first_of( ' ' );
    const size_t first_newline = output.find_first_of( '\n' );
    const std::string interval_time = output.substr( 0, first_space );
    m.interval_speed[entry.index].second.emplace_back( entry.script_size, stod( interval_time ) );
    const std::string response_average = output.substr( first_space, first_newline - first_space );
    m.average_response_time[entry.index].second.emplace_back( entry.script_size, stod( response_average ) );
    const std::string utilization
        = output.substr( first_newline + 1, output.find_last_of( '%' ) - ( first_newline + 1 ) );
    m.overall_utilization[entry.index].second.emplace_back( entry.script_size, stod( utilization ) );
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
        std::abort();
    }
    std::array<const char *, 7> arg_copy{};
    for ( size_t i = 0; i < args.size(); ++i ) {
        arg_copy.at( i ) = args.at( i ).data();
    }
    arg_copy.back() = nullptr;
    execv( cmd_path.data(), const_cast<char *const *>( arg_copy.data() ) ); // NOLINT
    std::cerr << "child process failed abnormally errno: " << errno << "\n";
    std::abort();
}

void thread_fill_data( const size_t allocator_index, const path_bin &cmd, big_o_metrics &m )
{
    const std::string title = cmd.bin.substr( cmd.bin.find( '_' ) + 1 );
    for ( const auto &args : increasing_malloc_free_size_args ) {
        const size_t script_size = parse_quantity_n( args.back() );
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
        close_process( { process, bytes_read } );
        std::string data = std::string( vec_buf.data() );
        parse_metrics(
            data,
            allocator_entry{
                allocator_index,
                script_size,
            },
            m
        );
    }
}

void time_malloc_frees( const std::vector<path_bin> &commands, big_o_metrics &m )
{
    for ( const auto &c : commands ) {
        const std::string title = c.bin.substr( c.bin.find( '_' ) + 1 );
        m.interval_speed.push_back( { title, {} } );
        m.interval_speed.back().second.reserve( increasing_malloc_free_size_args.size() );
        m.average_response_time.push_back( { title, {} } );
        m.average_response_time.back().second.reserve( increasing_malloc_free_size_args.size() );
        m.overall_utilization.push_back( { title, {} } );
        m.overall_utilization.back().second.reserve( increasing_malloc_free_size_args.size() );
    }
    command_queue workers( worker_limit );
    for ( size_t i = 0; i < commands.size(); ++i ) {
        workers.push( [i, &commands, &m]() { thread_fill_data( i, commands[i], m ); } );
    }
    for ( size_t i = 0; i < worker_limit; ++i ) {
        workers.push( {} );
    }
}

} // namespace

int main()
{
    const std::vector<path_bin> commands = gather_timer_programs();
    big_o_metrics m{};
    time_malloc_frees( commands, m );
    for ( const auto &metric : m.interval_speed ) {
        std::cout << metric.first << ":\n";
        for ( const std::pair<size_t, double> &stat : metric.second ) {
            std::cout << stat.first << " " << stat.second << "\n";
        }
    }
    return 0;
}
