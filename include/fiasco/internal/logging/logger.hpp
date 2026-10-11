#pragma once

#include <string>
#include <unordered_map>

#include "fiasco/internal/logging/log_level.hpp"

namespace fiasco::detail {

class log_queue;

class logger {
  public:
    void debug(const std::string& msg);
    void debug(const std::string& msg, std::unordered_map<std::string, std::string> fields);
    void info(const std::string& msg);
    void info(const std::string& msg, std::unordered_map<std::string, std::string> fields);
    void warn(const std::string& msg);
    void warn(const std::string& msg, std::unordered_map<std::string, std::string> fields);
    void error(const std::string& msg);
    void error(const std::string& msg, std::unordered_map<std::string, std::string> fields);

    void set_level(log_level lvl);
    log_level level() const;

    const std::string& name() const;

  private:
    friend class log_registry;

    logger(std::string name, log_queue& queue, log_level level);

    void log(log_level lvl,
             std::string msg,
             std::unordered_map<std::string, std::string> fields = {});

    std::string m_name;
    log_queue* m_queue;
    log_level m_level;
};

}  // namespace fiasco::detail
