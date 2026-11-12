#pragma once
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace demo {
// Single consumer. Callbacks own their captured data; references must outlive close().
// Destruction/close must run on the owner thread, never inside a queued callback.
class Worker final {
public:
    enum class Submission { accepted, full, closed };
    explicit Worker(std::size_t capacity) : capacity_(capacity) {
        if (!capacity_) throw std::invalid_argument("capacity must be positive");
        thread_ = std::jthread([this] { run(); });
    }
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    ~Worker() { close(); }

    Submission submit(std::function<void()> task) {
        if (!task) throw std::invalid_argument("empty task");
        std::lock_guard lock(mutex_);
        if (closing_) return Submission::closed;
        if (queue_.size() >= capacity_) return Submission::full;
        queue_.push_back(std::move(task));
        ready_.notify_one();
        return Submission::accepted;
    }
    // Stops admission, drains accepted tasks, then joins before member destruction.
    // Serialise close callers; submit and failure_count remain thread-safe.
    void close() {
        {
            std::lock_guard lock(mutex_);
            closing_ = true;
        }
        ready_.notify_all();
        if (thread_.joinable()) thread_.join();
    }
    std::size_t failure_count() const {
        std::lock_guard lock(mutex_);
        return failures_;
    }
private:
    void run() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock, [this] { return closing_ || !queue_.empty(); });
                if (queue_.empty()) return;
                task = std::move(queue_.front());
                queue_.pop_front();
            }
            try { task(); }
            catch (...) {
                std::lock_guard lock(mutex_);
                ++failures_;
            }
        }
    }
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> queue_;
    bool closing_ = false;
    std::size_t failures_ = 0;
    std::jthread thread_;
};
}
