#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>

#include "fiasco/internal/logging/log_record.hpp"

namespace fiasco::detail {

class log_queue {
  public:
    log_queue() = default;

    void enqueue(log_record record) {
        {
            std::lock_guard lock(m_mutex);
            m_records.push_back(std::move(record));
        }
        m_enqueued.fetch_add(1, std::memory_order_release);
        m_cv.notify_one();
    }

    std::optional<log_record> try_dequeue() {
        std::lock_guard lock(m_mutex);
        if (m_records.empty()) {
            return std::nullopt;
        }
        auto record = std::move(m_records.front());
        m_records.pop_front();
        return record;
    }

    log_record dequeue_wait() {
        std::unique_lock lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_records.empty() || m_stopped.load(); });
        if (m_records.empty()) {
            return {};
        }
        auto record = std::move(m_records.front());
        m_records.pop_front();
        return record;
    }

    void stop() {
        {
            std::lock_guard lock(m_mutex);
            m_stopped.store(true);
        }
        m_cv.notify_all();
        m_flush_cv.notify_all();
    }

    size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_records.size();
    }

    /// Total number of records enqueued so far (monotonically increasing).
    std::uint64_t enqueued() const noexcept { return m_enqueued.load(std::memory_order_acquire); }

    /// Called by the consumer once a record has been fully written. Publishes the
    /// increment with release semantics so that a flush() observed via
    /// wait_until_processed() happens-after the store to the log sink.
    void mark_processed() noexcept {
        m_processed.fetch_add(1, std::memory_order_release);
        m_flush_cv.notify_all();
    }

    /// Block until at least `target` records have been processed (i.e. written
    /// to the sink), or until the queue is stopped. Pair with enqueued() to
    /// implement a synchronous flush.
    void wait_until_processed(std::uint64_t target) {
        std::unique_lock lock(m_flush_mutex);
        m_flush_cv.wait(lock, [&] {
            return m_processed.load(std::memory_order_acquire) >= target || m_stopped.load();
        });
    }

  private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<log_record> m_records;
    std::atomic<bool> m_stopped{false};

    // Progress counters for flush().
    std::atomic<std::uint64_t> m_enqueued{0};
    std::atomic<std::uint64_t> m_processed{0};
    std::mutex m_flush_mutex;
    std::condition_variable m_flush_cv;
};

}  // namespace fiasco::detail
