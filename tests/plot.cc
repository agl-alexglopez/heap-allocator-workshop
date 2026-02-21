/// Author: Alexander G. Lopez
/// File: plot.cc
/// -------------
/// This file is responsible for gathering all allocators, running them over
/// predetermined or requested scripts and comparing the performance with
/// Matplot++. See the README.md file for more instructions. If looking over the
/// implementation, key points of interest might be the multiprocessing, the
/// error messages if scripts are missing, or the commands used to measure
/// performance across allocators.
///
/// The basic concept is that we create a small executable capable of timing
/// code for each allocator. We then gather these executables and run them with
/// a thread pool and multiprocessing. Once all the data is gathered we plot it
/// with a single thread through Matplot++.
#include "command_queue.hh"
#include "osync.hh"

#include "matplot/core/figure_registry.h"
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
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
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

/// The script commands are carefully tuned to only time sections where the
/// desired behavior is operating.
constexpr std::array<std::array<std::array<char const *, 4>, 20>, 2>
    big_o_timing = {{
        {{
            // Malloc and free commands are targeted at making many
            // uncoalescable free nodes and mallocing them.
            {"-r", "10001", "20000", "scripts/time-insertdelete-05k.script"},
            {"-r", "20001", "40000", "scripts/time-insertdelete-10k.script"},
            {"-r", "30001", "60000", "scripts/time-insertdelete-15k.script"},
            {"-r", "40001", "80000", "scripts/time-insertdelete-20k.script"},
            {"-r", "50001", "100000", "scripts/time-insertdelete-25k.script"},
            {"-r", "60001", "120000", "scripts/time-insertdelete-30k.script"},
            {"-r", "70001", "140000", "scripts/time-insertdelete-35k.script"},
            {"-r", "80001", "160000", "scripts/time-insertdelete-40k.script"},
            {"-r", "90001", "180000", "scripts/time-insertdelete-45k.script"},
            {"-r", "100001", "200000", "scripts/time-insertdelete-50k.script"},
            {"-r", "110001", "220000", "scripts/time-insertdelete-55k.script"},
            {"-r", "120001", "240000", "scripts/time-insertdelete-60k.script"},
            {"-r", "130001", "260000", "scripts/time-insertdelete-65k.script"},
            {"-r", "140001", "280000", "scripts/time-insertdelete-70k.script"},
            {"-r", "150001", "300000", "scripts/time-insertdelete-75k.script"},
            {"-r", "160001", "320000", "scripts/time-insertdelete-80k.script"},
            {"-r", "170001", "340000", "scripts/time-insertdelete-85k.script"},
            {"-r", "180001", "360000", "scripts/time-insertdelete-90k.script"},
            {"-r", "190001", "380000", "scripts/time-insertdelete-95k.script"},
            {"-r", "200001", "400000", "scripts/time-insertdelete-100k.script"},
        }},
        {{
            // Realloc commands are targeted at reallocing many allocated nodes
            // that are surrounded by free nodes.
            {"-r", "15001", "20000", "scripts/time-reallocfree-05k.script"},
            {"-r", "30001", "40000", "scripts/time-reallocfree-10k.script"},
            {"-r", "45001", "60000", "scripts/time-reallocfree-15k.script"},
            {"-r", "60001", "80000", "scripts/time-reallocfree-20k.script"},
            {"-r", "75001", "100000", "scripts/time-reallocfree-25k.script"},
            {"-r", "90001", "120000", "scripts/time-reallocfree-30k.script"},
            {"-r", "105001", "140000", "scripts/time-reallocfree-35k.script"},
            {"-r", "120001", "160000", "scripts/time-reallocfree-40k.script"},
            {"-r", "135001", "180000", "scripts/time-reallocfree-45k.script"},
            {"-r", "150001", "200000", "scripts/time-reallocfree-50k.script"},
            {"-r", "165001", "220000", "scripts/time-reallocfree-55k.script"},
            {"-r", "180001", "240000", "scripts/time-reallocfree-60k.script"},
            {"-r", "195001", "260000", "scripts/time-reallocfree-65k.script"},
            {"-r", "210001", "280000", "scripts/time-reallocfree-70k.script"},
            {"-r", "225001", "300000", "scripts/time-reallocfree-75k.script"},
            {"-r", "240001", "320000", "scripts/time-reallocfree-80k.script"},
            {"-r", "255001", "340000", "scripts/time-reallocfree-85k.script"},
            {"-r", "270001", "360000", "scripts/time-reallocfree-90k.script"},
            {"-r", "285001", "380000", "scripts/time-reallocfree-95k.script"},
            {"-r", "300001", "400000", "scripts/time-reallocfree-100k.script"},
        }},

    }};

constexpr std::array<std::string_view, 5> line_ticks
    = {"-o", "--", "-+", "-s", "-*"};
constexpr std::array<std::string_view, 9> loading_bar
    = {"⣿", "⣷", "⣯", "⣟", "⡿", "⢿", "⣻", "⣽", "⣾"};
constexpr std::string_view save_cursor = "\033[s";
constexpr std::string_view restore_cursor = "\033[u";

constexpr size_t loading_limit = 50;

using quantity_measurement
    = std::pair<std::vector<double>, std::vector<double>>;
using data_series = std::pair<std::string, std::vector<double>>;
using data_set = std::pair<std::vector<double>, std::vector<data_series>>;

struct process_result {
    int process_id;
    ssize_t bytes_read;
};

struct path_bin {
    std::string path;
    std::string bin;
};

enum heap_operation : uint8_t {
    malloc_free,
    realloc_free,
    script_comparison,
};

enum data_set_type : uint8_t {
    interval,
    response,
    utilization,
};

struct runtime_metrics {
    data_set interval_speed;
    data_set average_response_time;
    data_set overall_utilization;
};

struct labels {
    std::string_view title;
    std::string_view x_label;
    std::string_view y_label;
    std::string_view filename;
};

struct label_pack {
    labels interval_labels;
    labels response_labels;
    labels utilization_labels;
};

struct plot_args {
    heap_operation op{};
    labels interval_labels;
    labels response_labels;
    labels utilization_labels;
    size_t threads{default_worker_count};
    bool quiet{false};
    std::optional<std::string> script_name;
    plot_args(heap_operation h, label_pack l, size_t threads, bool quiet)
        : op(h), interval_labels(l.interval_labels),
          response_labels(l.response_labels),
          utilization_labels(l.utilization_labels), threads(threads),
          quiet(quiet) {
    }
    plot_args() = default;
};

struct allocator_entry {
    size_t index;
    double script_size;
};

std::vector<path_bin> gather_timer_programs();
int run_bigo_analysis(std::vector<path_bin> const &commands,
                      plot_args const &args);
int plot_script_comparison(std::vector<path_bin> const &commands,
                           plot_args &args);
data_set const &set(runtime_metrics const &m, data_set_type request);
std::vector<double> const &x_axis(data_set const &s);
std::vector<double> &x_axis(data_set &s);
std::vector<double> &series(data_set &s, size_t series_index);
std::vector<data_series> &all_series(data_set &s);
void twiddle_cursor(command_queue &q);
bool close_process(process_result res);
double parse_quantity_n(std::string_view script_name);
bool parse_metrics(std::string const &output, size_t allocator_index,
                   runtime_metrics &m);
int start_subprocess(std::string_view cmd_path,
                     std::vector<char const *> const &args);
bool thread_run_cmd(size_t allocator_index, path_bin const &cmd,
                    runtime_metrics &m, std::vector<char const *> cmd_list);
std::optional<size_t> specify_threads(std::string_view thread_request);
bool thread_run_analysis(size_t allocator_index, path_bin const &cmd,
                         runtime_metrics &m, heap_operation s);
bool scripts_generated();
void line_plot_stats(runtime_metrics const &m, data_set_type t, labels l,
                     bool quiet);
void bar_chart_stats(runtime_metrics const &m, data_set_type t, labels l,
                     bool quiet);
int plot_runtime(std::vector<path_bin> const &commands, plot_args args);

//////////////////////////////////    User Argument Handling

int
plot(std::span<char const *const> cli_args) {
    try {
        std::vector<path_bin> const commands = gather_timer_programs();
        if (commands.empty()) {
            return 1;
        }
        plot_args args{};
        for (auto const *arg : cli_args) {
            std::string const arg_copy = std::string(arg);
            if (arg_copy == "-realloc") {
                args.op = heap_operation::realloc_free;
            } else if (arg_copy == "-malloc") {
                args.op = heap_operation::malloc_free;
            } else if (arg_copy.starts_with("-j")) {
                std::optional<size_t> threads = specify_threads(arg_copy);
                if (!threads) {
                    return 1;
                }
                args.threads = threads.value();
            } else if (arg_copy == "-q") {
                args.quiet = true;
            } else if (arg_copy.find('/') != std::string::npos) {
                args.op = heap_operation::script_comparison;
                args.script_name.emplace(arg_copy);
            } else {
                auto err = std::string("Invalid command line request: ")
                               .append(arg_copy)
                               .append("\n");
                osync::syncerr(err, osync::ansi_bred);
                return 1;
            }
        }
        if (args.op == heap_operation::script_comparison) {
            return plot_script_comparison(commands, args);
        }
        if (!scripts_generated()) {
            return 1;
        }
        return run_bigo_analysis(commands, args);
    } catch (std::exception &e) {
        auto err = std::string("Plot program caught exception ")
                       .append(e.what())
                       .append("\n");
        osync::syncerr(err, osync::ansi_bred);
        return 1;
    }
}

} // namespace

int
main(int argc, char *argv[]) {
    auto cli_args = std::span<char const *const>(argv, argc).subspan(1);
    if (cli_args.empty()) {
        return 0;
    }
    return plot(cli_args);
}

///////////////////////////       Performance Testing Implementation

namespace {

/////////////////////////  Threading Subproccesses and File Handling

int
run_bigo_analysis(std::vector<path_bin> const &commands,
                  plot_args const &args) {
    if (args.op == heap_operation::malloc_free) {
        return plot_runtime(
            commands, plot_args(heap_operation::malloc_free,
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
                                args.threads, args.quiet));
    }
    if (args.op == heap_operation::realloc_free) {
        return plot_runtime(
            commands, plot_args(heap_operation::realloc_free,
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
                                args.threads, args.quiet));
    }
    osync::syncerr("invalid options slipped through command line args.\n",
                   osync::ansi_bred);
    return 1;
}

int
plot_runtime(std::vector<path_bin> const &commands, plot_args args) {
    runtime_metrics m{};
    x_axis(m.interval_speed).push_back(0);
    x_axis(m.average_response_time).push_back(0);
    x_axis(m.overall_utilization).push_back(0);
    for (auto const &args : big_o_timing.at(args.op)) {
        double const script_size = parse_quantity_n(args.back());
        if (script_size == 0) {
            osync::syncerr("could not parse script size\n", osync::ansi_bred);
            return 1;
        }
        x_axis(m.interval_speed).push_back(script_size);
        x_axis(m.average_response_time).push_back(script_size);
        x_axis(m.overall_utilization).push_back(script_size);
    }
    for (auto const &c : commands) {
        std::string title = c.bin.substr(c.bin.find('_') + 1);
        // Matplot reads underscores as subscripts which messes up legends on
        // graphs. Change to space.
        std::replace(title.begin(), title.end(), '_', ' ');
        all_series(m.interval_speed).push_back({title, {0.0}});
        all_series(m.interval_speed)
            .back()
            .second.reserve(big_o_timing.at(args.op).size());
        all_series(m.average_response_time).push_back({title, {0.0}});
        all_series(m.average_response_time)
            .back()
            .second.reserve(big_o_timing.at(args.op).size());
        all_series(m.overall_utilization).push_back({title, {0.0}});
        all_series(m.overall_utilization)
            .back()
            .second.reserve(big_o_timing.at(args.op).size());
    }
    command_queue workers(args.threads);
    for (size_t i = 0; i < commands.size(); ++i) {
        // Not sure if bool is ok here but the filesystem and subproccesses can
        // often fail so signal with bool for easier thread shutdown. Futures
        // could maybe work but seems overkill?
        workers.push([i, &args, &commands, &m]() -> bool {
            return thread_run_analysis(i, commands[i], m, args.op);
        });
    }
    for (size_t i = 0; i < args.threads; ++i) {
        // Threads still wait if queue is empty so send a quit signal.
        workers.push({});
    }
    // This is just some cursor animation while we wait from main thread. But
    // don't put anything before it!
    twiddle_cursor(workers);
    line_plot_stats(m, data_set_type::interval, args.interval_labels,
                    args.quiet);
    line_plot_stats(m, data_set_type::response, args.response_labels,
                    args.quiet);
    line_plot_stats(m, data_set_type::utilization, args.utilization_labels,
                    args.quiet);
    std::cout << osync::ansi_nil;
    return 0;
}

bool
thread_run_analysis(size_t const allocator_index, path_bin const &cmd,
                    runtime_metrics &m, heap_operation s) {
    for (auto const &args : big_o_timing.at(s)) {
        thread_run_cmd(allocator_index, cmd, m,
                       std::vector<char const *>(args.begin(), args.end()));
    }
    return true;
}

int
plot_script_comparison(std::vector<path_bin> const &commands, plot_args &args) {
    if (!args.script_name) {
        osync::cerr("No script provided for plotting comparison",
                    osync::ansi_bred);
        return 1;
    }
    std::string const script_name_str(args.script_name.value());
    if (!std::ifstream(script_name_str, std::ios_base::in).good()) {
        auto const msg
            = std::string(
                  "Could not find the following file for script comparison:\n")
                  .append(script_name_str)
                  .append("\n");
        osync::cerr(msg, osync::ansi_bred);
        return 1;
    }
    runtime_metrics m{};
    std::string_view const path_to_script{script_name_str};
    std::string script(
        path_to_script.substr(path_to_script.find_last_of('/') + 1));
    script.erase(script.find_last_of('.'));
    auto const save_interval
        = std::string("output/interval-").append(script).append(".svg");
    auto const save_response
        = std::string("output/response-").append(script).append(".svg");
    auto const save_utilization
        = std::string("output/utilization-").append(script).append(".svg");
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
    for (auto const &c : commands) {
        std::string title = c.bin.substr(c.bin.find('_') + 1);
        // Matplot reads underscores as subscripts which messes up legends on
        // graphs. Change to space.
        std::replace(title.begin(), title.end(), '_', ' ');
        all_series(m.interval_speed).push_back({title, {}});
        all_series(m.average_response_time).push_back({title, {}});
        all_series(m.overall_utilization).push_back({title, {}});
    }
    command_queue workers(args.threads);
    for (size_t i = 0; i < commands.size(); ++i) {
        // Not sure if bool is ok here but the filesystem and subproccesses can
        // often fail so signal with bool for easier thread shutdown. Futures
        // could maybe work but seems overkill?
        workers.push([i, &args, &commands, &m]() -> bool {
            return thread_run_cmd(i, commands[i], m,
                                  {args.script_name.value().data()});
        });
    }
    for (size_t i = 0; i < args.threads; ++i) {
        // Threads still wait if queue is empty so send a quit signal.
        workers.push({});
    }
    // This is just some cursor animation while we wait from main thread. But
    // don't put anything before it!
    twiddle_cursor(workers);
    bar_chart_stats(m, data_set_type::interval, args.interval_labels,
                    args.quiet);
    bar_chart_stats(m, data_set_type::response, args.response_labels,
                    args.quiet);
    bar_chart_stats(m, data_set_type::utilization, args.utilization_labels,
                    args.quiet);
    return 0;
}

bool
thread_run_cmd(size_t const allocator_index, path_bin const &cmd,
               runtime_metrics &m, std::vector<char const *> cmd_list) {
    // I have had more success with subprocesses when using full paths but maybe
    // not necessary?
    std::string cur_path = std::filesystem::current_path();
    cur_path.append("/").append(cmd_list.back());
    std::vector<char const *> commands{cmd.bin.data()};
    commands.insert(commands.end(), cmd_list.begin(), cmd_list.end());
    // Be compliant with execv interface and end arguments with nullptr.
    commands.push_back(nullptr);
    int const process = start_subprocess(cmd.path, commands);
    // Output from the program we use is two lines max.
    size_t remaining = starting_buf_size;
    std::vector<char> vec_buf(remaining);
    ssize_t bytes_read = 0;
    size_t progress = 0;
    while ((bytes_read = read(process, &vec_buf.at(progress), remaining)) > 0) {
        progress += bytes_read;
        remaining -= bytes_read;
        if (remaining == 0) {
            remaining = vec_buf.size() * 2;
            vec_buf.resize(vec_buf.size() * 2);
        }
    }
    if (!close_process({process, bytes_read})) {
        // Many threads may output error messages so we will always sync cerr to
        // aid legibility.
        osync::syncerr(
            "This thread is quitting early, child subprocess failed\n",
            osync::ansi_bred);
        return false;
    }
    std::string const data = std::string(vec_buf.data());
    if (!parse_metrics(data, allocator_index, m)) {
        osync::syncerr("This thread is quitting early due to parsing error\n",
                       osync::ansi_bred);
        return false;
    }
    return true;
}

int
start_subprocess(std::string_view cmd_path,
                 std::vector<char const *> const &args) {
    std::array<int, 2> comms{0, 0};
    if (pipe2(comms.data(), O_CLOEXEC) < 0) {
        osync::syncerr("Could not open pipe for communication\n",
                       osync::ansi_bred);
        return -1;
    }
    if (fork() != 0) {
        close(comms[1]);
        return comms[0];
    }
    close(comms[0]);
    if (dup2(comms[1], STDOUT_FILENO) < 0) {
        osync::syncerr("Child cannot communicate to parent.\n",
                       osync::ansi_bred);
        close(comms[1]);
        exit(1);
    }
    // Passing arguments from c++ to execv is awful. NOLINTNEXTLINE
    execv(cmd_path.data(), const_cast<char *const *>(args.data()));
    auto err = std::string("Child process failed abnormally errno: ")
                   .append(std::to_string(errno))
                   .append("\n");
    osync::syncerr(err, osync::ansi_bred);
    close(comms[1]);
    exit(1);
}

bool
close_process(process_result res) {
    close(res.process_id);
    int err = 0;
    int const wait_err = waitpid(-1, &err, 0);
    if (WIFSIGNALED(err) && WTERMSIG(err) == SIGSEGV) { // NOLINT
        auto err = std::string("Seg fault waitpid returned ")
                       .append(std::to_string(wait_err))
                       .append("\n");
        osync::syncerr(err, osync::ansi_bred);
        return false;
    }
    if (res.bytes_read == -1) {
        osync::syncerr("read error\n", osync::ansi_bred);
        return false;
    }
    return true;
}

double
parse_quantity_n(std::string_view script_name) {
    size_t const last_dash = script_name.find_last_of('-') + 1;
    size_t const last_k = script_name.find_last_of('k');
    std::string const num
        = std::string(script_name.substr(last_dash, last_k - last_dash));
    try {
        return std::stod(num) * 1000;
    } catch (std::invalid_argument &a) {
        auto err = std::string("Caught invalid argument: ")
                       .append(a.what())
                       .append("\n");
        osync::syncerr(err, osync::ansi_bred);
        return 0;
    }
}

bool
parse_metrics(std::string const &output, size_t const allocator_index,
              runtime_metrics &m) {
    try {
        size_t const first_space = output.find_first_of(' ');
        size_t const first_newline = output.find_first_of('\n');
        std::string const interval_time = output.substr(0, first_space);
        series(m.interval_speed, allocator_index)
            .emplace_back(stod(interval_time));
        std::string const response_average
            = output.substr(first_space, first_newline - first_space);
        series(m.average_response_time, allocator_index)
            .emplace_back(stod(response_average));
        std::string const utilization = output.substr(
            first_newline + 1, output.find_last_of('%') - (first_newline + 1));
        series(m.overall_utilization, allocator_index)
            .emplace_back(stod(utilization));
        return true;
    } catch (std::invalid_argument &a) {
        osync::syncerr("Error parsing string input to metrics\n",
                       osync::ansi_bred);
        return false;
    }
}

std::vector<path_bin>
gather_timer_programs() {
    std::vector<path_bin> commands{};
    std::string path = std::filesystem::current_path();
    path.append(prog_path);
    for (auto const &entry : std::filesystem::directory_iterator(path)) {
        std::string const as_str = entry.path();
        size_t const last_entry = as_str.find_last_of('/');
        std::string_view const is_timer
            = std::string_view{as_str}.substr(last_entry + 1);
        if (is_timer.starts_with("stats_")) {
            commands.emplace_back(as_str, std::string(is_timer));
        }
    }
    return commands;
}

std::optional<size_t>
specify_threads(std::string_view thread_request) {
    if (thread_request == "-j") {
        osync::syncerr("Invalid core count requested. Did you mean -j[CORES] "
                       "without a space?\n",
                       osync::ansi_bred);
        return 1;
    }
    auto const cores = std::string(std::string_view{thread_request}.substr(
        thread_request.find_first_not_of("-j")));
    try {
        size_t result = std::stoull(cores);
        if (result == 0 || result > max_cores) {
            result = default_worker_count;
        }
        if (result == 1) {
            return result;
        }
        // We spawn a process for each thread which means 2x processes so divide
        // the request in half.
        return result / 2;
    } catch (std::invalid_argument &e) {
        auto err = std::string("Invalid core count requested from ")
                       .append(e.what())
                       .append(": ")
                       .append(cores)
                       .append("\n");
        osync::syncerr(err, osync::ansi_bred);
        return {};
    }
}

bool
scripts_generated() {
    std::vector<std::string> missing_files{};
    for (auto const &command_series : big_o_timing) {
        for (auto const &commands : command_series) {
            std::string const fpath_and_name(commands.back());
            if (!std::ifstream(fpath_and_name, std::ios_base::in).good()) {
                missing_files.push_back(fpath_and_name);
            }
        }
    }
    if (missing_files.empty()) {
        return true;
    }
    osync::cerr("See script generation instructions. Missing the following "
                "files for plot analysis:\n",
                osync::ansi_bred);
    for (auto const &missing : missing_files) {
        osync::cerr(missing, osync::ansi_bred);
        osync::cerr("\n", osync::ansi_bred);
    }
    return false;
}

//////////////////////////////   Plotting with Matplot++

void
line_plot_stats(runtime_metrics const &m, data_set_type t, labels l,
                bool quiet) {
    // To get the "figure" specify quiet mode so changes don't cause redraw.
    auto p = matplot::gcf(true);
    // The axes_object frames "plots".
    auto axes = p->current_axes();
    axes->title(l.title);
    axes->xlabel(l.x_label);
    axes->grid(true);
    axes->font_size(14.0);
    size_t tick_style = 0;
    for (auto const &allocator : set(m, t).second) {
        // Grab a "plot" where actual data with x data and y data belongs.
        auto plot = axes->plot(x_axis(m.overall_utilization), allocator.second,
                               line_ticks.at(tick_style));
        plot->line_width(2);
        // Sketchy .back() function. Is there a better associative way?
        if (::matplot::legend()->strings().empty()) {
            osync::syncerr("No legend entry generated for matplot++ plot. Has "
                           "API changed?\n",
                           osync::ansi_bred);
            return;
        }
        // Legend is freestanding but still associated with the current
        // axes_object. Not plot! So a hidden legend entry is automatically
        // generated when a plot is added. Edit this with correct name.
        ::matplot::legend()->strings().back() = allocator.first;
        // Need to plot the other allocator lines, not just one, so hold the
        // plots in between then plot all at end.
        matplot::hold(true);
        ++tick_style %= line_ticks.size();
    }
    ::matplot::legend()->location(
        matplot::legend::general_alignment::bottomright);
    ::matplot::legend()->num_rows(3);
    ::matplot::legend()->box(false);
    ::matplot::legend()->font_size(14.0);
    p->size(1920, 1080);
    p->save(std::string(l.filename));
    matplot::hold(false);
    auto msg = std::string("plot saved: ").append(l.filename).append("\n");
    std::cout << msg;
    if (quiet) {
        return;
    }
    // Because we are in quiet mode hopefully we have had no flashing and the
    // this will be the only plot shown.
    p->show();
}

// See comments in line plot status for what is happening here. It is same.
void
bar_chart_stats(runtime_metrics const &m, data_set_type t, labels l,
                bool quiet) {
    auto p = matplot::gcf(true);
    auto axes = p->current_axes();
    axes->title(l.title);
    axes->xlabel(l.x_label);
    axes->grid(true);
    axes->font_size(14.0);
    size_t tick_style = 0;
    std::vector<std::string> tick_labels{};
    std::vector<double> bar_data{};
    for (auto const &allocator : set(m, t).second) {
        bar_data.push_back(allocator.second.front());
        tick_labels.push_back(allocator.first);
        ++tick_style %= line_ticks.size();
    }
    auto plot = axes->bar(bar_data);
    axes->x_axis().ticklabels(tick_labels);
    axes->x_axis().tickangle(20.0);
    p->size(1920, 1080);
    p->save(std::string(l.filename));
    auto msg = std::string("plot saved: ").append(l.filename).append("\n");
    std::cout << msg;
    if (quiet) {
        return;
    }
    p->show();
}

//////////////////////////////    Helpers to Access Data in Types

data_set const &
set(runtime_metrics const &m, data_set_type request) {
    switch (request) {
        case interval:
            return m.interval_speed;
        case response:
            return m.average_response_time;
        case utilization:
            return m.overall_utilization;
        default: {
            osync::syncerr(
                "invalid request type for a data set that does not exist.\n",
                osync::ansi_bred);
            std::abort();
        }
    }
}

std::vector<double> const &
x_axis(data_set const &s) {
    return s.first;
}

std::vector<double> &
x_axis(data_set &s) {
    return s.first;
}

std::vector<double> &
series(data_set &s, size_t series_index) {
    return s.second[series_index].second;
}

std::vector<data_series> &
all_series(data_set &s) {
    return s.second;
}

/////////////////////////////             Just for Fun

void
twiddle_cursor(command_queue &q) {
    size_t dist = 0;
    bool max_loading_bar = false;
    std::cout << osync::ansi_bred;
    while (!q.empty()) {
        std::cout << save_cursor;
        for (size_t i = 0; i < loading_limit; ++i) {
            std::cout << loading_bar.at((i + dist) % loading_bar.size())
                      << std::flush;
            if (!max_loading_bar && i > dist) {
                break;
            }
        }
        std::cout << restore_cursor;
        ++dist;
        max_loading_bar = max_loading_bar || dist == 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
    std::cout << osync::ansi_bgrn;
    for (size_t i = 0; i < loading_limit; ++i) {
        std::cout << loading_bar.at((i + dist) % loading_bar.size());
    }
    std::cout << "\n";
}

} // namespace
