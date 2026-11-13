#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/http/response.hpp"
#include "fiasco/internal/middleware.hpp"

namespace fiasco::middleware {

using fiasco::detail::async;
using fiasco::detail::http_method;
using fiasco::detail::next_handler;
using fiasco::detail::request;
using fiasco::detail::response;

struct cors_config {
    std::vector<std::string> allow_origins{};
    std::vector<std::string> allow_methods{"GET"};
    std::vector<std::string> allow_headers{};
    bool allow_credentials = false;
    std::optional<std::string> allow_origin_regex{};
    bool allow_private_network = false;
    std::vector<std::string> expose_headers{};
    std::uint32_t max_age = 600;
};

namespace cors_detail {

inline std::string ascii_lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

inline std::string ascii_upper(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return result;
}

inline std::string join(const std::vector<std::string>& values, std::string_view separator) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) {
            result += separator;
        }
        result += value;
    }
    return result;
}

inline void append_vary(response& res, std::string_view value) {
    auto it = res.headers.find("Vary");
    if (it == res.headers.end()) {
        res.headers["Vary"] = std::string(value);
        return;
    }
    if (it->second.find(value) == std::string::npos) {
        it->second += ", ";
        it->second += value;
    }
}

inline void add_headers(response& res,
                        const std::unordered_map<std::string, std::string>& headers) {
    for (const auto& [name, value] : headers) {
        res.headers[name] = value;
    }
}

struct cors_state {
    explicit cors_state(cors_config config) {
        allow_all_origins =
            std::find(config.allow_origins.begin(), config.allow_origins.end(), "*") !=
            config.allow_origins.end();
        allow_all_headers =
            std::find(config.allow_headers.begin(), config.allow_headers.end(), "*") !=
            config.allow_headers.end();
        allow_credentials = config.allow_credentials;
        allow_private_network = config.allow_private_network;
        explicit_allow_origin = !allow_all_origins || allow_credentials;
        origins = std::move(config.allow_origins);

        if (allow_all_headers) {
            allow_headers = {"*"};
        } else {
            allow_headers = {"accept", "accept-language", "content-language", "content-type"};
            for (auto& header : config.allow_headers) {
                header = ascii_lower(header);
                if (std::find(allow_headers.begin(), allow_headers.end(), header) ==
                    allow_headers.end()) {
                    allow_headers.push_back(std::move(header));
                }
            }
            std::sort(allow_headers.begin(), allow_headers.end());
        }

        if (config.allow_origin_regex) {
            origin_regex.emplace(*config.allow_origin_regex);
        }

        if (allow_all_origins) {
            simple_headers["Access-Control-Allow-Origin"] = "*";
        }
        if (allow_credentials) {
            simple_headers["Access-Control-Allow-Credentials"] = "true";
        }
        if (!config.expose_headers.empty()) {
            simple_headers["Access-Control-Expose-Headers"] = join(config.expose_headers, ", ");
        }

        preflight_headers["Vary"] =
            "Origin, Access-Control-Request-Method, Access-Control-Request-Headers, "
            "Access-Control-Request-Private-Network";
        if (!explicit_allow_origin) {
            preflight_headers["Access-Control-Allow-Origin"] = "*";
        }

        if (std::find(config.allow_methods.begin(), config.allow_methods.end(), "*") !=
            config.allow_methods.end()) {
            methods = {"DELETE", "GET", "HEAD", "OPTIONS", "PATCH", "POST", "PUT"};
        } else {
            methods.reserve(config.allow_methods.size());
            for (auto& method : config.allow_methods) {
                methods.push_back(ascii_upper(method));
            }
        }
        preflight_headers["Access-Control-Allow-Methods"] = join(methods, ", ");
        preflight_headers["Access-Control-Max-Age"] = std::to_string(config.max_age);
        if (!allow_all_headers && !allow_headers.empty()) {
            preflight_headers["Access-Control-Allow-Headers"] = join(allow_headers, ", ");
        }
        if (allow_credentials) {
            preflight_headers["Access-Control-Allow-Credentials"] = "true";
        }
    }

    [[nodiscard]] bool is_allowed_origin(std::string_view origin) const {
        if (allow_all_origins) {
            return true;
        }
        if (origin_regex && std::regex_match(origin.begin(), origin.end(), *origin_regex)) {
            return true;
        }
        return std::find(origins.begin(), origins.end(), origin) != origins.end();
    }

    bool allow_all_origins = false;
    bool allow_all_headers = false;
    bool allow_credentials = false;
    bool allow_private_network = false;
    bool explicit_allow_origin = false;
    std::vector<std::string> origins;
    std::vector<std::string> methods;
    std::vector<std::string> allow_headers;
    std::optional<std::regex> origin_regex;
    std::unordered_map<std::string, std::string> simple_headers;
    std::unordered_map<std::string, std::string> preflight_headers;
};

inline response preflight_response(const cors_state& state, const request& req) {
    const std::string origin = req.header("Origin");
    const std::string requested_method = ascii_upper(req.header("Access-Control-Request-Method"));
    const std::string requested_headers = req.header("Access-Control-Request-Headers");
    std::vector<std::string> failures;

    response res = response::text("OK", 200);
    add_headers(res, state.preflight_headers);

    if (state.is_allowed_origin(origin)) {
        if (state.explicit_allow_origin) {
            res.headers["Access-Control-Allow-Origin"] = origin;
        }
    } else {
        failures.emplace_back("origin");
    }

    if (std::find(state.methods.begin(), state.methods.end(), requested_method) ==
        state.methods.end()) {
        failures.emplace_back("method");
    }

    if (!requested_headers.empty()) {
        if (state.allow_all_headers) {
            res.headers["Access-Control-Allow-Headers"] = requested_headers;
        } else {
            std::size_t start = 0;
            while (start <= requested_headers.size()) {
                const auto end = requested_headers.find(',', start);
                const auto part = requested_headers.substr(
                    start, end == std::string::npos ? std::string::npos : end - start);
                const auto header = ascii_lower(part);
                if (std::find(state.allow_headers.begin(), state.allow_headers.end(), header) ==
                    state.allow_headers.end()) {
                    failures.emplace_back("headers");
                    break;
                }
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
        }
    }

    if (req.has_header("Access-Control-Request-Private-Network")) {
        if (state.allow_private_network) {
            res.headers["Access-Control-Allow-Private-Network"] = "true";
        } else {
            failures.emplace_back("private-network");
        }
    }

    if (!failures.empty()) {
        res.status_code = 400;
        res.body = "Disallowed CORS " + join(failures, ", ") + "\n";
    }
    return res;
}

}  // namespace cors_detail

inline fn cors(cors_config config = {}) {
    auto state = std::make_shared<cors_detail::cors_state>(std::move(config));

    return [state](request& req, const next_handler& next) -> async {
        const std::string origin = req.header("Origin");
        if (req.method == http_method::options && !origin.empty() &&
            req.has_header("Access-Control-Request-Method")) {
            co_return cors_detail::preflight_response(*state, req);
        }

        auto res = co_await next(req);
        if (origin.empty()) {
            co_return res;
        }

        cors_detail::add_headers(res, state->simple_headers);
        if (state->allow_all_origins && state->allow_credentials) {
            res.headers["Access-Control-Allow-Origin"] = origin;
            cors_detail::append_vary(res, "Origin");
        } else if (!state->allow_all_origins && state->is_allowed_origin(origin)) {
            res.headers["Access-Control-Allow-Origin"] = origin;
            cors_detail::append_vary(res, "Origin");
        } else {
            cors_detail::append_vary(res, "Origin");
        }
        co_return res;
    };
}

}  // namespace fiasco::middleware
