#include "fiasco/internal/logging/logger.hpp"

#include <utility>

#include "fiasco/internal/logging/log_queue.hpp"
#include "fiasco/internal/logging/log_record.hpp"

namespace fiasco::detail {

logger::logger(std::string name, log_queue& queue, log_level level)
    : m_name(std::move(name)),
      m_queue(&queue),
      m_level(level) {}

void logger::set_level(log_level lvl) {
    m_level = lvl;
}

log_level logger::level() const {
    return m_level;
}

const std::string& logger::name() const {
    return m_name;
}

void logger::log(log_level lvl,
                  std::string msg,
                  std::unordered_map<std::string, std::string> fields) {
    if (lvl < m_level) {
        return;
    }
    log_record record;
    record.level = lvl;
    record.logger_name = m_name;
    record.message = std::move(msg);
    record.fields = std::move(fields);
    m_queue->enqueue(std::move(record));
}

#define LOGGER_METHOD(level_name, level_enum)                                  \
    void logger::level_name(const std::string& msg) {                          \
        log(log_level::level_enum, msg);                                       \
    }                                                                          \
    void logger::level_name(                                                   \
        const std::string& msg,                                                \
        std::unordered_map<std::string, std::string> fields) {                 \
        log(log_level::level_enum, msg, std::move(fields));                    \
    }

LOGGER_METHOD(debug, debug)
LOGGER_METHOD(info, info)
LOGGER_METHOD(warn, warn)
LOGGER_METHOD(error, error)

#undef LOGGER_METHOD

} // namespace fiasco::detail
