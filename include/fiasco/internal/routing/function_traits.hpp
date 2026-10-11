#pragma once

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
        return s == "true" || s == "1";
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

/// Dispatch order:
///   1. fiasco::request  -> pass raw request
///   2. Primitive        -> next positional path param
///   3. FromJson         -> deserialize request body
template <typename T>
decltype(auto) resolve_arg(const request& req, size_t& path_idx) {
    using CleanT = std::decay_t<T>;

    if constexpr (std::same_as<CleanT, request>) {
        return req;
    } else if constexpr (Primitive<CleanT>) {
        if (path_idx >= req.ordered_path_params.size()) {
            throw std::runtime_error("Not enough path params for handler argument at position " +
                                     std::to_string(path_idx));
        }
        return static_cast<CleanT>(convert_path_param<CleanT>(req.ordered_path_params[path_idx++]));
    } else if constexpr (FromJson<CleanT>) {
        if (req.body.empty()) {
            throw json::exception("request body is empty");
        }
        try {
            auto j = json::parse(req.body);
            CleanT val;
            from_json(j, val);
            return val;
        } catch (const json::exception& e) {
            throw json::exception(std::string("invalid request body: ") + e.what());
        }
    }
}

template <typename T>
using arg_storage_t =
    std::conditional_t<std::same_as<std::decay_t<T>, request>, const request&, std::decay_t<T>>;

template <typename F, typename ArgsTuple, size_t... Is>
response dispatch_impl(F& fn, const request& req, size_t& path_idx, std::index_sequence<Is...>) {
    // braced init guarantees left-to-right evaluation; values are owned by the tuple
    std::tuple<arg_storage_t<std::tuple_element_t<Is, ArgsTuple>>...> args{
        resolve_arg<std::tuple_element_t<Is, ArgsTuple>>(req, path_idx)...};

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
        size_t path_idx = 0;
        return dispatch_impl<decltype(fn), args_tuple>(
            fn, req, path_idx, std::make_index_sequence<traits::arity>{});
    };
}

}  // namespace fiasco::detail
