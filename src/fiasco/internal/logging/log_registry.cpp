#include "fiasco/internal/logging/log_registry.hpp"

#include <iostream>
#include <mutex>
#include <utility>

namespace fiasco::detail {

log_registry& log_registry::instance() {
    static log_registry registry;
    return registry;
}

log_registry::log_registry()
    : log_registry(true, true, log_level::info, log_format_type::human, true, {}) {}

log_registry::log_registry(bool enabled,
                           bool access_log,
                           log_level level,
                           log_format_type format,
                           bool use_colors,
                           fiasco::log_sink sink)
    : m_formatter(make_formatter(format, use_colors)),
      m_sink(sink ? std::move(sink) : fiasco::make_ostream_log_sink(std::cout)),
      m_root_level(level),
      m_logging_enabled(enabled),
      m_access_log(access_log) {
    m_running = true;
    m_consumer_thread = std::thread(&log_registry::consumer_loop, this);
}

void log_registry::set_sink(std::ostream& os) {
    std::lock_guard lock(m_mutex);
    m_sink = fiasco::make_ostream_log_sink(os);
}

void log_registry::reset_sink() {
    std::lock_guard lock(m_mutex);
    m_sink = fiasco::make_ostream_log_sink(std::cout);
}

void log_registry::flush() {
    // Wait for all records enqueued as of now to be written. mark_processed()
    // is called by the consumer after write(), and publishing it here gives the
    // caller a happens-before edge with the sink write.
    m_queue.wait_until_processed(m_queue.enqueued());
}

log_registry::~log_registry() {
    m_queue.stop();
    if (m_consumer_thread.joinable()) {
        m_consumer_thread.join();
    }
}

logger& log_registry::get(const std::string& name) {
    std::lock_guard lock(m_mutex);
    auto it = m_loggers.find(name);
    if (it != m_loggers.end()) {
        return *it->second;
    }
    auto ptr = std::unique_ptr<logger>(
        new logger(name, m_queue, m_root_level, m_logging_enabled));
    auto& ref = *ptr;
    m_loggers[name] = std::move(ptr);
    return ref;
}

void log_registry::set_level(log_level lvl) {
    std::lock_guard lock(m_mutex);
    m_root_level = lvl;
    for (auto& [name, log] : m_loggers) {
        log->m_level = lvl;
    }
}

void log_registry::set_format(log_format_type type, bool use_colors) {
    std::lock_guard lock(m_mutex);
    m_formatter = make_formatter(type, use_colors);
}

void log_registry::enable_logging(bool en) {
    m_logging_enabled.store(en, std::memory_order_relaxed);
}

bool log_registry::logging_enabled() const {
    return m_logging_enabled.load(std::memory_order_relaxed);
}

void log_registry::enable_access_log(bool en) {
    m_access_log.store(en, std::memory_order_relaxed);
}

bool log_registry::access_log_enabled() const {
    return m_access_log.load(std::memory_order_relaxed);
}

void log_registry::consumer_loop() {
    while (true) {
        auto record = m_queue.dequeue_wait();
        if (record.logger_name.empty()) {
            break;
        }
        write(record);
        m_queue.mark_processed();
    }
}

void log_registry::write(const log_record& record) {
    std::lock_guard lock(m_mutex);
    auto formatted = m_formatter->format(record);
    m_sink(formatted);
}

}  // namespace fiasco::detail
