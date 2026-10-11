#pragma once

#include <memory>
#include <string>

#include "fiasco/internal/logging/log_record.hpp"

namespace fiasco::detail {

class log_formatter {
  public:
    virtual ~log_formatter() = default;
    virtual std::string format(const log_record& record) const = 0;
};

enum class log_format_type { human, json };

class human_log_formatter final : public log_formatter {
  public:
    explicit human_log_formatter(bool use_colors = true);
    std::string format(const log_record& record) const override;

  private:
    bool m_use_colors;
};

class json_log_formatter final : public log_formatter {
  public:
    std::string format(const log_record& record) const override;
};

std::unique_ptr<log_formatter> make_formatter(log_format_type type, bool use_colors = true);

}  // namespace fiasco::detail
