#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include <unordered_map>

#include "fiasco/internal/logging/log_formatter.hpp"
#include "fiasco/internal/logging/log_level.hpp"
#include "fiasco/internal/logging/log_queue.hpp"
#include "fiasco/internal/logging/log_sink.hpp"
#include "fiasco/internal/logging/logger.hpp"

namespace fiasco::detail {

class log_registry {
  public:
    log_registry();
    log_registry(bool enabled,
                 bool access_log,
                 log_level level,
                 log_format_type format,
                 bool use_colors,
                 fiasco::log_sink sink = {});

    static log_registry& instance();

    logger& get(const std::string& name);

    void set_level(log_level lvl);
    void set_format(log_format_type type, bool use_colors = true);
    void enable_logging(bool en);
    bool logging_enabled() const;
    void enable_access_log(bool en);
    bool access_log_enabled() const;

    void set_sink(std::ostream& os);
    void reset_sink();

    /// Block until every record enqueued before this call has been written to
    /// the current sink. Intended for tests / orderly shutdown so callers do not
    /// have to sleep and guess when the background consumer has caught up.
    void flush();

    log_queue& queue() { return m_queue; }

    ~log_registry();

  private:
    log_registry(const log_registry&) = delete;
    log_registry& operator=(const log_registry&) = delete;

    void consumer_loop();
    void write(const log_record& record);

    log_queue m_queue;
    std::unordered_map<std::string, std::unique_ptr<logger>> m_loggers;
    std::unique_ptr<log_formatter> m_formatter{make_formatter(log_format_type::human)};
    std::thread m_consumer_thread;
    std::atomic<bool> m_running{false};
    mutable std::mutex m_mutex;
    fiasco::log_sink m_sink;
    log_level m_root_level{log_level::info};
    std::atomic<bool> m_logging_enabled{true};
    std::atomic<bool> m_access_log{true};
};

}  // namespace fiasco::detail
