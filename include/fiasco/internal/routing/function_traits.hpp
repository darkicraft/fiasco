#pragma once

#include <cctype>
#include <charconv>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/http/response.hpp"
#include "fiasco/internal/json.hpp"
#include "fiasco/internal/meta/reflect.hpp"
#include "fiasco/internal/routing/concepts.hpp"

namespace fiasco::detail {

// Move-only so handlers may capture move-only state (e.g. unique_ptr) without
// paying std::function's copyable-requirement / allocation model.
// Note: std::move_only_function::operator() is non-const, so handlers are
// invoked through a non-const reference (see route_table::match).
using handler_fn = std::move_only_function<response(request)>;

template <Primitive T>
[[nodiscard]] T convert_path_param(std::string_view s) {
    if constexpr (std::same_as<T, std::string>) {
        return std::string(s);
    } else if constexpr (std::same_as<T, int> || std::same_as<T, long>) {
        T val;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
        if (ec != std::errc()) {
            throw std::runtime_error("bad path param");
        }
        return val;
    } else if constexpr (std::same_as<T, long long>) {
        long long val;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
        if (ec != std::errc()) {
            throw std::runtime_error("bad path param");
        }
        return val;
    } else if constexpr (std::same_as<T, float>) {
        float val;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
        if (ec != std::errc()) {
            throw std::runtime_error("bad path param");
        }
        return val;
    } else if constexpr (std::same_as<T, double>) {
        double val;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
        if (ec != std::errc()) {
            throw std::runtime_error("bad path param");
        }
        return val;
    } else if constexpr (std::same_as<T, bool>) {
        std::string lowered(s);
        for (char& c : lowered) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on" ||
            lowered == "t" || lowered == "y" || lowered == "enable") {
            return true;
        }
        if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off" ||
            lowered == "f" || lowered == "n" || lowered == "disable") {
            return false;
        }
        throw std::runtime_error("bad path param");
    }

    return {};  // unreachable
}

template <typename T>
[[nodiscard]] response serialize_return(T&& val) {
    using CleanT = std::decay_t<T>;
    if constexpr (std::same_as<CleanT, response>) {
        return std::forward<T>(val);
    } else if constexpr (std::same_as<CleanT, std::string>) {
        return response::text(std::forward<T>(val));
    } else if constexpr (std::same_as<CleanT, const char*>) {
        return response::text(std::string(val));
    } else if constexpr (Primitive<CleanT>) {
        return response::text(std::to_string(val));
    } else if constexpr (ToJson<CleanT>) {
        json j = val;
        return response::json(j.dump());
    } else {
        static_assert(!sizeof(T),
                      "Handler return type must be a primitive, "
                      "fiasco::response, or a reflectable model type");
    }

    return {};  // unreachable
}

/// Resolve one handler argument, FastAPI-style, by parameter *name*:
///   1. fiasco::request  -> pass the raw request
///   2. Primitive        -> a path param if the route captured `name`, else a
///                          query param
///   3. FromJson         -> the request body (embedded under `name` when the
///                          handler has more than one body parameter)
/// Failures are reported as FastAPI-compatible request validation errors.
template <typename T>
decltype(auto) resolve_arg(const request& req,
                           std::size_t& path_idx,
                           std::string_view name,
                           std::size_t body_count) {
    using CleanT = std::decay_t<T>;

    if constexpr (std::same_as<CleanT, request>) {
        (void)path_idx;
        (void)name;
        (void)body_count;
        return req;
    } else if constexpr (Primitive<CleanT>) {
        const auto parse = [&](std::string_view raw, const char* source) -> CleanT {
            try {
                return static_cast<CleanT>(convert_path_param<CleanT>(raw));
            } catch (const std::exception&) {
                throw request_validation_error(
                    {{.type = std::string(pydantic_parsing_type_name<CleanT>()),
                      .loc = {std::string(source), std::string(name)},
                      .msg = std::string(pydantic_parsing_message<CleanT>()),
                      .input = json_type(std::string(raw))}});
            }
        };

        if (name.empty()) {
            // Positional path parameter: function pointers expose no reflectable
            // parameter names, so they fall back to URL order.
            if (path_idx < req.ordered_path_params.size()) {
                return parse(req.ordered_path_params[path_idx++], "path");
            }
            throw request_validation_error({{.type = "missing",
                                             .loc = {std::string("path")},
                                             .msg = "Field required",
                                             .input = json_type()}});
        }

        if (auto it = req.path_params.find(name); it != req.path_params.end()) {
            return parse(it->second, "path");
        }
        if (auto it = req.query_params.find(std::string(name)); it != req.query_params.end()) {
            return parse(it->second, "query");
        }
        throw request_validation_error({{.type = "missing",
                                         .loc = {std::string("query"), std::string(name)},
                                         .msg = "Field required",
                                         .input = json_type()}});
    } else if constexpr (FromJson<CleanT>) {
        (void)path_idx;
        if (req.body.empty()) {
            throw request_validation_error({{.type = "missing",
                                             .loc = {std::string("body")},
                                             .msg = "Field required",
                                             .input = json_type()}});
        }
        json_type j;
        try {
            j = json::parse(req.body);
        } catch (const json_type::parse_exception& e) {
            throw request_validation_error({{.type = "json_invalid",
                                             .loc = {std::string("body"), e.position()},
                                             .msg = "JSON decode error",
                                             .input = json_type::object()}});
        } catch (const json::exception&) {
            throw request_validation_error({{.type = "json_invalid",
                                             .loc = {std::string("body"), std::size_t{0}},
                                             .msg = "JSON decode error",
                                             .input = json_type::object()}});
        }

        // With multiple body parameters FastAPI embeds each under its name.
        if (body_count > 1) {
            if (!j.contains(std::string(name))) {
                throw request_validation_error({{.type = "missing",
                                                 .loc = {std::string("body"), std::string(name)},
                                                 .msg = "Field required",
                                                 .input = j}});
            }
            json_type sub = j[std::string(name)];
            try {
                CleanT val;
                from_json(sub, val);
                return val;
            } catch (const request_validation_error& e) {
                throw request_validation_error(
                    prefix_loc(prefix_loc(e.errors(), std::string(name)), "body"));
            } catch (const std::exception&) {
                throw request_validation_error(
                    {make_type_error<CleanT>({std::string("body"), std::string(name)}, sub)});
            }
        }

        try {
            CleanT val;
            from_json(j, val);
            return val;
        } catch (const request_validation_error& e) {
            throw request_validation_error(prefix_loc(e.errors(), "body"));
        } catch (const std::exception&) {
            throw request_validation_error({{.type = std::string(pydantic_type_name<CleanT>()),
                                             .loc = {std::string("body")},
                                             .msg = std::string(pydantic_message<CleanT>()),
                                             .input = j}});
        }
    }
}

template <typename T>
using arg_storage_t =
    std::conditional_t<std::same_as<std::decay_t<T>, request>, const request&, std::decay_t<T>>;

/// A handler parameter that comes from the JSON body: a non-scalar JSON type
/// (model/container/optional), as opposed to a path/query scalar.
template <typename T>
concept BodyParam = FromJson<std::decay_t<T>> && !Primitive<std::decay_t<T>> &&
                    !std::same_as<std::decay_t<T>, request>;

template <typename F, typename ArgsTuple, size_t... Is>
response dispatch_impl(F& fn,
                       const request& req,
                       std::size_t& path_idx,
                       std::index_sequence<Is...>) {
    using traits = meta::callable_traits<std::decay_t<F>>;
    constexpr std::size_t body_count =
        ((BodyParam<std::tuple_element_t<Is, ArgsTuple>> ? 1u : 0u) + ... + 0u);

    // braced init guarantees left-to-right evaluation; values are owned by the tuple
    std::tuple<arg_storage_t<std::tuple_element_t<Is, ArgsTuple>>...> args{
        resolve_arg<std::tuple_element_t<Is, ArgsTuple>>(
            req, path_idx, traits::argument_name(Is), body_count)...};

    if constexpr (std::is_void_v<std::invoke_result_t<F, std::tuple_element_t<Is, ArgsTuple>...>>) {
        fn(static_cast<std::tuple_element_t<Is, ArgsTuple>>(std::get<Is>(args))...);
        return response::empty();
    } else {
        return serialize_return(
            fn(static_cast<std::tuple_element_t<Is, ArgsTuple>>(std::get<Is>(args))...));
    }
}

template <typename F>
[[nodiscard]] handler_fn make_handler(F&& f) {
    using traits = meta::callable_traits<std::decay_t<F>>;
    using args_tuple = traits::args_tuple;

    return [fn = std::forward<F>(f)](request req) mutable -> response {
        std::size_t path_idx = 0;
        return dispatch_impl<decltype(fn), args_tuple>(
            fn, req, path_idx, std::make_index_sequence<traits::arity>{});
    };
}

}  // namespace fiasco::detail
