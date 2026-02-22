#include <condition_variable>
#include <cstddef>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <syncstream>
#include <thread>
#include <utility>
#include <vector>

/// The work we do to gather timing is trivially parallelizable. We just need a
/// parent to monitor this small stat generation program and enter the results.
/// So we can have threads become the parents for these parallel processes and
/// they will just add the stats to the runtim metrics container that has
/// preallocated space for them. Because the number of programs we time may grow
/// in the future and the threads each spawn a child process we have 2x the
/// processes. Use a work queue to cap the processes but still maintain
/// consistent parallelism.
class Command_queue {
    // I queue subprocess handling which can often fail so I figured we need
    // more info than void. Futures?
    std::queue<std::optional<std::function<bool()>>> q;
    std::mutex lk;
    std::condition_variable wait;
    std::vector<std::thread> workers;
    void
    start() {
        for (;;) {
            std::unique_lock<std::mutex> ul(lk);
            wait.wait(ul, [this]() -> bool { return !q.empty(); });
            auto new_task = std::move(q.front());
            q.pop();
            ul.unlock();
            if (!new_task.has_value()) {
                return;
            }
            if (!new_task.value()()) {
                std::osyncstream(std::cerr)
                    << "Error running requesting function.\n";
                return;
            }
        }
    }

  public:
    explicit Command_queue(size_t num_workers) {
        for (size_t i = 0; i < num_workers; ++i) {
            workers.emplace_back([this]() { start(); });
        }
    }
    ~Command_queue() {
        for (auto &w : workers) {
            w.join();
        }
    }

    void
    push(std::optional<std::function<bool()>> args) {
        std::unique_lock<std::mutex> const ul(lk);
        q.push(std::move(args));
        wait.notify_one();
    }

    [[nodiscard]] bool
    empty() {
        std::unique_lock<std::mutex> const ul(lk);
        return q.empty();
    }
    Command_queue(Command_queue const &) = delete;
    Command_queue &operator=(Command_queue const &) = delete;
    Command_queue(Command_queue &&) = delete;
    Command_queue &operator=(Command_queue &&) = delete;
};
