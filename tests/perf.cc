#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

#ifdef NDEBUG
constexpr std::string_view prog_path = "/build/rel";
#else
constexpr std::string_view prog_path = "/build/deb";
#endif

struct path_bin
{
    std::string path;
    std::string bin;
};

struct runtime_per_n
{
    size_t n;
    double runtime;
};

struct allocator_big_o
{
    std::string allocator;
    std::vector<runtime_per_n> stats;
};

/// These are the commands that focus in on the key lines of the malloc free scripts to time.
constexpr std::array<std::array<std::string_view, 6>, 20> increasing_malloc_free_size_args = { {
    { "-q", "-s", "10001", "-e", "20000", "scripts/time-insertdelete-05k.script" },
    { "-q", "-s", "20001", "-e", "40000", "scripts/time-insertdelete-10k.script" },
    { "-q", "-s", "30001", "-e", "60000", "scripts/time-insertdelete-15k.script" },
    { "-q", "-s", "40001", "-e", "80000", "scripts/time-insertdelete-20k.script" },
    { "-q", "-s", "50001", "-e", "100000", "scripts/time-insertdelete-25k.script" },
    { "-q", "-s", "60001", "-e", "120000", "scripts/time-insertdelete-30k.script" },
    { "-q", "-s", "70001", "-e", "140000", "scripts/time-insertdelete-35k.script" },
    { "-q", "-s", "80001", "-e", "160000", "scripts/time-insertdelete-40k.script" },
    { "-q", "-s", "90001", "-e", "180000", "scripts/time-insertdelete-45k.script" },
    { "-q", "-s", "100001", "-e", "200000", "scripts/time-insertdelete-50k.script" },
    { "-q", "-s", "110001", "-e", "220000", "scripts/time-insertdelete-55k.script" },
    { "-q", "-s", "120001", "-e", "240000", "scripts/time-insertdelete-60k.script" },
    { "-q", "-s", "130001", "-e", "260000", "scripts/time-insertdelete-65k.script" },
    { "-q", "-s", "140001", "-e", "280000", "scripts/time-insertdelete-70k.script" },
    { "-q", "-s", "150001", "-e", "300000", "scripts/time-insertdelete-75k.script" },
    { "-q", "-s", "160001", "-e", "320000", "scripts/time-insertdelete-80k.script" },
    { "-q", "-s", "170001", "-e", "340000", "scripts/time-insertdelete-85k.script" },
    { "-q", "-s", "180001", "-e", "360000", "scripts/time-insertdelete-90k.script" },
    { "-q", "-s", "190001", "-e", "380000", "scripts/time-insertdelete-95k.script" },
    { "-q", "-s", "200001", "-e", "400000", "scripts/time-insertdelete-100k.script" },
} };

constexpr std::array<std::array<std::string_view, 6>, 20> increasing_realloc_free_size_args = { {
    { "-q", "-s", "15001", "-e", "20000", "scripts/time-reallocfree-05k.script" },
    { "-q", "-s", "30001", "-e", "40000", "scripts/time-reallocfree-10k.script" },
    { "-q", "-s", "45001", "-e", "60000", "scripts/time-reallocfree-15k.script" },
    { "-q", "-s", "60001", "-e", "80000", "scripts/time-reallocfree-20k.script" },
    { "-q", "-s", "75001", "-e", "100000", "scripts/time-reallocfree-25k.script" },
    { "-q", "-s", "90001", "-e", "120000", "scripts/time-reallocfree-30k.script" },
    { "-q", "-s", "105001", "-e", "140000", "scripts/time-reallocfree-35k.script" },
    { "-q", "-s", "120001", "-e", "160000", "scripts/time-reallocfree-40k.script" },
    { "-q", "-s", "135001", "-e", "180000", "scripts/time-reallocfree-45k.script" },
    { "-q", "-s", "150001", "-e", "200000", "scripts/time-reallocfree-50k.script" },
    { "-q", "-s", "165001", "-e", "220000", "scripts/time-reallocfree-55k.script" },
    { "-q", "-s", "180001", "-e", "240000", "scripts/time-reallocfree-60k.script" },
    { "-q", "-s", "195001", "-e", "260000", "scripts/time-reallocfree-65k.script" },
    { "-q", "-s", "210001", "-e", "280000", "scripts/time-reallocfree-70k.script" },
    { "-q", "-s", "225001", "-e", "300000", "scripts/time-reallocfree-75k.script" },
    { "-q", "-s", "240001", "-e", "320000", "scripts/time-reallocfree-80k.script" },
    { "-q", "-s", "255001", "-e", "340000", "scripts/time-reallocfree-85k.script" },
    { "-q", "-s", "270001", "-e", "360000", "scripts/time-reallocfree-90k.script" },
    { "-q", "-s", "285001", "-e", "380000", "scripts/time-reallocfree-95k.script" },
    { "-q", "-s", "300001", "-e", "400000", "scripts/time-reallocfree-100k.script" },
} };

size_t parse_quantity_n( std::string_view script_name )
{
    const size_t last_dash = script_name.find_last_of( '-' ) + 1;
    const size_t last_k = script_name.find_last_of( 'k' );
    const std::string num = std::string( script_name.substr( last_dash, last_k - last_dash ) );
    return std::stoull( num ) * 1000;
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
        if ( is_timer.starts_with( "time_" ) ) {
            commands.emplace_back( as_str, std::string( is_timer ) );
        }
    }
    return commands;
}

int script_timer_process( const path_bin &cmd, const std::array<std::string_view, 6> &args )
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
    std::string cur_path = std::filesystem::current_path();
    cur_path.append( "/" );
    std::array<std::string, 7> local_copy = {
        std::string( cmd.bin ),
        std::string( args[0] ),
        std::string( args[1] ),
        std::string( args[2] ),
        std::string( args[3] ),
        std::string( args[4] ),
        cur_path + std::string( args[5] ),
    };
    std::array<char *, 8> arg_copy{};
    for ( size_t i = 0; i < local_copy.size(); ++i ) {
        arg_copy.at( i ) = local_copy.at( i ).data();
    }
    arg_copy.back() = nullptr;
    execv( cmd.path.data(), static_cast<char *const *>( arg_copy.data() ) ); // NOLINT
    std::cerr << "child process failed abnormally errno: " << errno << "\n";
    std::abort();
}

std::unordered_map<std::string, std::vector<std::pair<size_t, double>>>
time_malloc_frees( const std::vector<path_bin> &commands )
{
    std::unordered_map<std::string, std::vector<std::pair<size_t, double>>> times{};
    for ( const auto &c : commands ) {
        auto allocator_entry = times.insert( { c.bin.substr( c.bin.find( '_' ) + 1 ), {} } );
        for ( const auto &args : increasing_malloc_free_size_args ) {
            const int timer_process = script_timer_process( c, args );
            std::vector<char> vec_buf( 50 );
            ssize_t bytes_read = 0;
            while ( ( bytes_read = read( timer_process, vec_buf.data(), 50 ) ) > 0 ) {}
            close( timer_process );
            int err = 0;
            waitpid( -1, &err, 0 );
            if ( WIFSIGNALED( err ) && WTERMSIG( err ) == SIGSEGV ) { // NOLINT
                std::cerr << "seg fault\n";
                std::abort();
            }
            if ( bytes_read == -1 ) {
                std::cerr << "read error\n";
                std::abort();
            }
            std::string buf( vec_buf.data() );
            buf = buf.substr( 0, buf.find_first_of( '\n' ) );
            allocator_entry.first->second.emplace_back( parse_quantity_n( args.back() ), stod( buf ) );
        }
    }
    return times;
}

} // namespace

int main()
{
    const std::vector<path_bin> commands = gather_timer_programs();
    const std::unordered_map<std::string, std::vector<std::pair<size_t, double>>> times
        = time_malloc_frees( commands );
    for ( const auto &t : times ) {
        std::cout << t.first << ":\n";
        for ( const std::pair<size_t, double> &stat : t.second ) {
            std::cout << stat.first << " " << stat.second << "\n";
        }
    }
    return 0;
}
