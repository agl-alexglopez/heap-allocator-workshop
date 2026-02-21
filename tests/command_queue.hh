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
class command_queue {
    // I queue subprocess handling which can often fail so I figured we need
    // more info than void. Futures?
    std::queue<std::optional<std::function<bool()>>> q_;
    std::mutex lk_;
    std::condition_variable wait_;
    std::vector<std::thread> workers_;
    void
    start() {
        for (;;) {
            std::unique_lock<std::mutex> ul(lk_);
            wait_.wait(ul, [this]() -> bool { return !q_.empty(); });
            auto new_task = std::move(q_.front());
            q_.pop();
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
    explicit command_queue(size_t num_workers) {
        for (size_t i = 0; i < num_workers; ++i) {
            workers_.emplace_back([this]() { start(); });
        }
    }
    ~command_queue() {
        for (auto &w : workers_) {
            w.join();
        }
    }

    void
    push(std::optional<std::function<bool()>> args) {
        const std::unique_lock<std::mutex> ul(lk_);
        q_.push(std::move(args));
        wait_.notify_one();
    }

    [[nodiscard]] bool
    empty() {
        const std::unique_lock<std::mutex> ul(lk_);
        return q_.empty();
    }
    command_queue(const command_queue &) = delete;
    command_queue &operator=(const command_queue &) = delete;
    command_queue(command_queue &&) = delete;
    command_queue &operator=(command_queue &&) = delete;
};
