#pragma once

#include <condition_variable>
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
        m_cv.wait(lock, [this] { return !m_records.empty() || m_stopped; });
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
            m_stopped = true;
        }
        m_cv.notify_all();
    }

    size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_records.size();
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<log_record> m_records;
    bool m_stopped = false;
};

} // namespace fiasco::detail
