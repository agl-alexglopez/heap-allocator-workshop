#include <array>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

namespace {

/// These are the commands that focus in on the key lines of the malloc free scripts to time.
constexpr std::array<std::array<char const *, 7>, 20> increasing_malloc_free_size_args = { {
    { "-q", "-s", "10001", "-e", "20000", "scripts/time-insertdelete-05k.script ", nullptr },
    { "-q", "-s", "20001", "-e", "40000", "scripts/time-insertdelete-10k.script ", nullptr },
    { "-q", "-s", "30001", "-e", "60000", "scripts/time-insertdelete-15k.script ", nullptr },
    { "-q", "-s", "40001", "-e", "80000", "scripts/time-insertdelete-20k.script ", nullptr },
    { "-q", "-s", "50001", "-e", "100000", "scripts/time-insertdelete-25k.script ", nullptr },
    { "-q", "-s", "60001", "-e", "120000", "scripts/time-insertdelete-30k.script ", nullptr },
    { "-q", "-s", "70001", "-e", "140000", "scripts/time-insertdelete-35k.script ", nullptr },
    { "-q", "-s", "80001", "-e", "160000", "scripts/time-insertdelete-40k.script ", nullptr },
    { "-q", "-s", "90001", "-e", "180000", "scripts/time-insertdelete-45k.script ", nullptr },
    { "-q", "-s", "100001", "-e", "200000", "scripts/time-insertdelete-50k.script ", nullptr },
    { "-q", "-s", "110001", "-e", "220000", "scripts/time-insertdelete-55k.script ", nullptr },
    { "-q", "-s", "120001", "-e", "240000", "scripts/time-insertdelete-60k.script ", nullptr },
    { "-q", "-s", "130001", "-e", "260000", "scripts/time-insertdelete-65k.script ", nullptr },
    { "-q", "-s", "140001", "-e", "280000", "scripts/time-insertdelete-70k.script ", nullptr },
    { "-q", "-s", "150001", "-e", "300000", "scripts/time-insertdelete-75k.script ", nullptr },
    { "-q", "-s", "160001", "-e", "320000", "scripts/time-insertdelete-80k.script ", nullptr },
    { "-q", "-s", "170001", "-e", "340000", "scripts/time-insertdelete-85k.script ", nullptr },
    { "-q", "-s", "180001", "-e", "360000", "scripts/time-insertdelete-90k.script ", nullptr },
    { "-q", "-s", "190001", "-e", "380000", "scripts/time-insertdelete-95k.script ", nullptr },
    { "-q", "-s", "200001", "-e", "400000", "scripts/time-insertdelete-100k.script", nullptr },
} };

constexpr std::array<std::array<char const *, 7>, 20> increasing_realloc_free_size_args = { {
    { "-q", "-s", "15001", "-e", "20000", "scripts/time-reallocfree-05k.script", nullptr },
    { "-q", "-s", "30001", "-e", "40000", "scripts/time-reallocfree-10k.script", nullptr },
    { "-q", "-s", "45001", "-e", "60000", "scripts/time-reallocfree-15k.script", nullptr },
    { "-q", "-s", "60001", "-e", "80000", "scripts/time-reallocfree-20k.script", nullptr },
    { "-q", "-s", "75001", "-e", "100000", "scripts/time-reallocfree-25k.script", nullptr },
    { "-q", "-s", "90001", "-e", "120000", "scripts/time-reallocfree-30k.script", nullptr },
    { "-q", "-s", "105001", "-e", "140000", "scripts/time-reallocfree-35k.script", nullptr },
    { "-q", "-s", "120001", "-e", "160000", "scripts/time-reallocfree-40k.script", nullptr },
    { "-q", "-s", "135001", "-e", "180000", "scripts/time-reallocfree-45k.script", nullptr },
    { "-q", "-s", "150001", "-e", "200000", "scripts/time-reallocfree-50k.script", nullptr },
    { "-q", "-s", "165001", "-e", "220000", "scripts/time-reallocfree-55k.script", nullptr },
    { "-q", "-s", "180001", "-e", "240000", "scripts/time-reallocfree-60k.script", nullptr },
    { "-q", "-s", "195001", "-e", "260000", "scripts/time-reallocfree-65k.script", nullptr },
    { "-q", "-s", "210001", "-e", "280000", "scripts/time-reallocfree-70k.script", nullptr },
    { "-q", "-s", "225001", "-e", "300000", "scripts/time-reallocfree-75k.script", nullptr },
    { "-q", "-s", "240001", "-e", "320000", "scripts/time-reallocfree-80k.script", nullptr },
    { "-q", "-s", "255001", "-e", "340000", "scripts/time-reallocfree-85k.script", nullptr },
    { "-q", "-s", "270001", "-e", "360000", "scripts/time-reallocfree-90k.script", nullptr },
    { "-q", "-s", "285001", "-e", "380000", "scripts/time-reallocfree-95k.script", nullptr },
    { "-q", "-s", "300001", "-e", "400000", "scripts/time-reallocfree-100k.script", nullptr },
} };

int script_timer_process( std::string_view cmd, std::array<char *const, 7> &args )
{
    std::array<int, 2> comms{};
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
    std::string path = std::filesystem::current_path();
    path.append( std::string( cmd ) );
    execvp( path.data(), args.data() );
    return -1;
}

} // namespace

int main()
{

    return 0;
}
