#pragma once

// File-range preparation for `response::file`. The factory records metadata
// only; this layer turns that metadata into a streaming body once the request
// headers are known.

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include "fiasco/contract/request.hpp"
#include "fiasco/contract/response.hpp"

namespace fiasco::detail {

namespace file_response_detail {

inline auto trim(std::string_view value) -> std::string_view {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

inline auto parse_uint(std::string_view value) -> std::optional<std::uintmax_t> {
    std::uintmax_t result = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec != std::errc{} || ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return result;
}

inline auto etag_matches(std::string_view header, std::string_view etag) -> bool {
    header = trim(header);
    if (header == "*") {
        return true;
    }

    std::size_t start = 0;
    while (start <= header.size()) {
        const auto comma = header.find(',', start);
        auto candidate = trim(header.substr(
            start, comma == std::string_view::npos ? std::string_view::npos : comma - start));
        if (candidate.starts_with("W/")) {
            candidate.remove_prefix(2);
        }
        if (candidate == etag) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    return false;
}

enum class range_status { none, valid, invalid };

struct byte_range {
    range_status status = range_status::none;
    std::uintmax_t start = 0;
    std::uintmax_t end = 0;
};

inline auto parse_range(std::string_view header, std::uintmax_t size) -> byte_range {
    header = trim(header);
    if (!header.starts_with("bytes=")) {
        return {};
    }

    header.remove_prefix(6);
    if (header.find(',') != std::string_view::npos) {
        return {.status = range_status::invalid};
    }

    const auto dash = header.find('-');
    if (dash == std::string_view::npos) {
        return {.status = range_status::invalid};
    }

    const auto first = trim(header.substr(0, dash));
    const auto second = trim(header.substr(dash + 1));

    if (first.empty()) {
        const auto suffix = parse_uint(second);
        if (!suffix || *suffix == 0 || size == 0) {
            return {.status = range_status::invalid};
        }
        const auto count = std::min(*suffix, size);
        return {.status = range_status::valid, .start = size - count, .end = size - 1};
    }

    const auto start = parse_uint(first);
    if (!start || *start >= size) {
        return {.status = range_status::invalid};
    }

    std::uintmax_t end = size - 1;
    if (!second.empty()) {
        const auto parsed_end = parse_uint(second);
        if (!parsed_end || *parsed_end < *start) {
            return {.status = range_status::invalid};
        }
        end = std::min(*parsed_end, size - 1);
    }

    return {.status = range_status::valid, .start = *start, .end = end};
}

class file_stream_state {
  public:
    file_stream_state(const std::filesystem::path& path,
                      std::uintmax_t start,
                      std::uintmax_t length)
        : m_file(path, std::ios::binary),
          m_remaining(length) {
        if (m_file) {
            m_file.seekg(static_cast<std::streamoff>(start));
            if (!m_file) {
                m_file.close();
            }
        }
    }

    [[nodiscard]] auto is_open() const -> bool { return m_file.is_open(); }

    auto read() -> task<std::optional<std::span<const std::byte>>> {
        if (!m_file || m_remaining == 0) {
            co_return std::nullopt;
        }

        const auto count = static_cast<std::streamsize>(
            std::min<std::uintmax_t>(m_buffer.size(), m_remaining));
        m_file.read(m_buffer.data(), count);
        const auto got = m_file.gcount();
        if (got <= 0) {
            co_return std::nullopt;
        }

        m_remaining -= static_cast<std::uintmax_t>(got);
        co_return std::as_bytes(
            std::span<const char>(m_buffer.data(), static_cast<std::size_t>(got)));
    }

  private:
    std::ifstream m_file;
    std::array<char, 64 * 1024> m_buffer{};
    std::uintmax_t m_remaining;
};

inline auto prepare_file_response(response& res, const request& req) -> void {
    if (!res.file_source) {
        return;
    }

    const auto file = *res.file_source;
    res.file_source.reset();

    res.headers["Last-Modified"] = file.last_modified;
    res.headers["ETag"] = file.etag;
    res.headers["Accept-Ranges"] = "bytes";

    const auto if_none_match = req.header("If-None-Match");
    if (!if_none_match.empty()) {
        if (etag_matches(if_none_match, file.etag)) {
            res.status_code = 304;
            res.headers.erase("Content-Length");
            res.stream_source.reset();
            return;
        }
    } else {
        const auto if_modified_since = req.header("If-Modified-Since");
        if (!if_modified_since.empty() && trim(if_modified_since) == file.last_modified) {
            res.status_code = 304;
            res.headers.erase("Content-Length");
            res.stream_source.reset();
            return;
        }
    }

    bool use_range = true;
    const auto if_range = req.header("If-Range");
    if (!if_range.empty() && trim(if_range) != file.etag &&
        trim(if_range) != file.last_modified) {
        use_range = false;
    }

    auto range = byte_range{};
    if (use_range) {
        range = parse_range(req.header("Range"), file.size);
    }

    std::uintmax_t start = 0;
    std::uintmax_t length = file.size;

    if (range.status == range_status::invalid) {
        res.status_code = 416;
        res.headers["Content-Range"] = "bytes */" + std::to_string(file.size);
        res.headers["Content-Length"] = "0";
        res.stream_source.reset();
        return;
    }
    if (range.status == range_status::valid) {
        start = range.start;
        length = range.end - range.start + 1;
        res.status_code = 206;
        res.headers["Content-Range"] =
            "bytes " + std::to_string(range.start) + "-" + std::to_string(range.end) + "/" +
            std::to_string(file.size);
        res.headers["Content-Length"] = std::to_string(length);
    } else {
        res.status_code = 200;
        res.headers["Content-Length"] = std::to_string(file.size);
    }

    if (req.method == http_method::head) {
        return;
    }

    auto state = std::make_shared<file_stream_state>(file.path, start, length);
    if (!state->is_open()) {
        res = response::error("Internal Server Error", 500);
        return;
    }

    res.stream_source = std::make_shared<body_source>(body_source{
        .read = [state]() -> task<std::optional<std::span<const std::byte>>> {
            co_return co_await state->read();
        },
    });
}

}  // namespace file_response_detail

using file_response_detail::prepare_file_response;

}  // namespace fiasco::detail
