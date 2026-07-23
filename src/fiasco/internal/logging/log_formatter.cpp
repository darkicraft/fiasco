#include "fiasco/internal/logging/log_formatter.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fiasco::detail {

// ---- ANSI color helpers ----------------------------------------------------

namespace {

constexpr const char* color_reset   = "\033[0m";
constexpr const char* color_gray    = "\033[90m";
constexpr const char* color_green   = "\033[32m";
constexpr const char* color_yellow  = "\033[33m";
constexpr const char* color_red     = "\033[31m";
constexpr const char* color_cyan    = "\033[36m";
constexpr const char* color_bold    = "\033[1m";

constexpr const char* level_color(log_level lvl) {
    switch (lvl) {
    case log_level::debug: return color_gray;
    case log_level::info:  return color_green;
    case log_level::warn:  return color_yellow;
    case log_level::error: return color_red;
    }
    return color_reset;
}

std::string format_timestamp_human(const std::chrono::system_clock::time_point& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch()).count() % 1000;

    std::tm tm{};
    localtime_r(&tt, &tm);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    std::snprintf(buf + 19, sizeof(buf) - 19, ".%03ld", ms);
    return buf;
}

std::string format_timestamp_json(const std::chrono::system_clock::time_point& tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch()).count() % 1000;

    std::tm tm{};
    gmtime_r(&tt, &tm);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    std::snprintf(buf + 19, sizeof(buf) - 19, ".%03ldZ", ms);
    return buf;
}

std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out += c;
        }
    }
    return out;
}

} // anonymous namespace

// ---- human_log_formatter ---------------------------------------------------

human_log_formatter::human_log_formatter(bool use_colors)
    : m_use_colors(use_colors) {}

std::string human_log_formatter::format(const log_record& record) const {
    std::string out;

    auto ts = format_timestamp_human(record.timestamp);
    const char* lvl = to_string(record.level);
    const char* lvl_c = level_color(record.level);

    if (m_use_colors) {
        out += color_gray;
        out += '[';
        out += ts;
        out += ']';
        out += color_reset;
        out += ' ';

        char level_buf[8];
        std::snprintf(level_buf, sizeof(level_buf), "%-5s", lvl);
        out += lvl_c;
        out += level_buf;
        out += color_reset;
        out += ' ';

        out += color_cyan;
        out += '[';
        out += record.logger_name;
        out += ']';
        out += color_reset;
        out += ' ';

        out += record.message;
    } else {
        out += '[';
        out += ts;
        out += ']';
        out += ' ';

        char level_buf[8];
        std::snprintf(level_buf, sizeof(level_buf), "%-5s", lvl);
        out += level_buf;
        out += ' ';

        out += '[';
        out += record.logger_name;
        out += ']';
        out += ' ';

        out += record.message;
    }

    // Append extra fields if any
    if (!record.fields.empty()) {
        out += " {";
        bool first = true;
        for (const auto& [k, v] : record.fields) {
            if (!first) {
                out += ", ";
            }
            first = false;
            out += k;
            out += '=';
            out += v;
        }
        out += '}';
    }

    out += '\n';
    return out;
}

// ---- json_log_formatter ----------------------------------------------------

std::string json_log_formatter::format(const log_record& record) const {
    std::string out = R"({"timestamp":")";
    out += format_timestamp_json(record.timestamp);
    out += R"(","level":")";
    out += to_string(record.level);
    out += R"(","logger":")";
    out += escape_json(record.logger_name);
    out += R"(","message":")";
    out += escape_json(record.message);

    for (const auto& [k, v] : record.fields) {
        out += R"(",")";
        out += escape_json(k);
        out += R"(":")";
        out += escape_json(v);
    }

    out += "\"}\n";
    return out;
}

// ---- factory ---------------------------------------------------------------

std::unique_ptr<log_formatter> make_formatter(log_format_type type, bool use_colors) {
    switch (type) {
    case log_format_type::human:
        return std::make_unique<human_log_formatter>(use_colors);
    case log_format_type::json:
        return std::make_unique<json_log_formatter>();
    }
    return std::make_unique<human_log_formatter>(use_colors);
}

} // namespace fiasco::detail
