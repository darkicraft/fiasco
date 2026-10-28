#pragma once

#include <array>
#include <cctype>
#include <charconv>
#include <concepts>
#include <functional>
#include <optional>
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
#include "fiasco/internal/routing/dependency.hpp"

namespace fiasco::detail {

// Move-only so handlers may capture move-only state (e.g. unique_ptr) without
// paying std::function's copyable-requirement / allocation model.
// Note: std::move_only_function::operator() is non-const, so handlers are
// invoked through a non-const reference (see route_table::match).
using handler_fn = std::move_only_function<response(request)>;

template <typename T>
struct is_optional_primitive : std::false_type {};

template <typename T>
struct is_optional_primitive<std::optional<T>> : std::bool_constant<Primitive<T>> {};

template <typename T>
concept OptionalPrimitive = is_optional_primitive<std::remove_cvref_t<T>>::value;

/// A scalar source type: either a primitive or an optional primitive.
template <typename T>
concept ScalarParam = Primitive<std::remove_cvref_t<T>> || OptionalPrimitive<T>;

template <Primitive T>
[[nodiscard]] T convert_path_param(std::string_view s) {
    if constexpr (std::same_as<T, std::string>) {
        return std::string(s);
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
    } else if constexpr (std::integral<T> || std::floating_point<T>) {
        T val{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
        if (ec != std::errc()) {
            throw std::runtime_error("bad path param");
        }
        return val;
    }

    return {};  // unreachable
}

inline std::optional<std::string> find_cookie(std::string_view cookie_header,
                                              std::string_view name) {
    std::size_t start = 0;
    while (start < cookie_header.size()) {
        const auto end = cookie_header.find(';', start);
        auto part = cookie_header.substr(
            start, end == std::string_view::npos ? std::string_view::npos : end - start);

        auto first = part.find_first_not_of(" \t");
        auto last = part.find_last_not_of(" \t");
        if (first != std::string_view::npos) {
            part = part.substr(first, last - first + 1);
            const auto eq = part.find('=');
            if (eq != std::string_view::npos && part.substr(0, eq) == name) {
                return std::string(part.substr(eq + 1));
            }
        }

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return std::nullopt;
}

template <typename T>
consteval param_in effective_source(param_in source) {
    using CleanT = std::decay_t<T>;
    if (source != param_in::infer) {
        return source;
    }
    if constexpr (std::same_as<CleanT, request>) {
        return param_in::infer;
    } else if constexpr (ScalarParam<CleanT>) {
        return param_in::infer;
    } else if constexpr (FromJson<CleanT>) {
        return param_in::body;
    } else {
        return param_in::infer;
    }
}

inline constexpr std::size_t max_body_params = 64;

consteval void add_body_key(std::array<std::string_view, max_body_params>& keys,
                            std::size_t& count,
                            std::string_view key) {
    for (std::size_t i = 0; i < count; ++i) {
        if (keys[i] == key) {
            return;
        }
    }
    if (count == max_body_params) {
        throw std::meta::exception("too many body parameters", ^^fiasco::depends);
    }
    keys[count++] = key;
}

template <std::meta::info Parameter>
consteval void collect_body_key(std::array<std::string_view, max_body_params>& keys,
                                std::size_t& count) {
    if (meta::depends_of(Parameter) != std::meta::info{}) {
        return;
    }

    using parameter_type = [:meta::parameter_type(Parameter):];
    if constexpr (effective_source<parameter_type>(meta::source_of(Parameter)) == param_in::body) {
        const auto spec = meta::field_of(Parameter);
        add_body_key(
            keys, count, spec.alias.has ? spec.alias.view() : meta::parameter_name(Parameter));
    }
}

template <std::meta::info Callable, std::size_t I, std::size_t N>
consteval void collect_callable_body_keys(std::array<std::string_view, max_body_params>& keys,
                                          std::size_t& count) {
    if constexpr (I < N) {
        collect_body_key<std::meta::parameters_of(Callable)[I]>(keys, count);
        collect_callable_body_keys<Callable, I + 1, N>(keys, count);
    }
}

template <auto Providers, std::size_t I, std::size_t N>
consteval void collect_provider_body_keys(std::array<std::string_view, max_body_params>& keys,
                                          std::size_t& count) {
    if constexpr (I < N) {
        constexpr auto provider = Providers[I];
        collect_callable_body_keys<provider, 0, std::meta::parameters_of(provider).size()>(keys,
                                                                                           count);
        collect_provider_body_keys<Providers, I + 1, N>(keys, count);
    }
}

/// True when the handler and its dependency graph have more than one unique
/// body field name, which means body parameters are embedded by name.
template <std::meta::info Callable>
consteval bool body_embedding_for() {
    constexpr auto providers = meta::providers_for<Callable>();
    std::array<std::string_view, max_body_params> keys{};
    std::size_t count = 0;

    collect_callable_body_keys<Callable, 0, std::meta::parameters_of(Callable).size()>(keys, count);
    collect_provider_body_keys<providers, 0, providers.size()>(keys, count);

    return count > 1;
}

template <typename T, fiasco::field<> Spec>
[[nodiscard]] T parse_scalar(std::string_view raw, const char* source, std::string_view key) {
    if constexpr (OptionalPrimitive<T>) {
        return T{parse_scalar<typename T::value_type, Spec>(raw, source, key)};
    } else {
        T value{};
        try {
            value = static_cast<T>(convert_path_param<T>(raw));
        } catch (const std::exception&) {
            throw request_validation_error({{.type = std::string(pydantic_parsing_type_name<T>()),
                                             .loc = {std::string(source), std::string(key)},
                                             .msg = std::string(pydantic_parsing_message<T>()),
                                             .input = json_type(std::string(raw))}});
        }

        std::vector<validation_error> errors;
        apply_field_constraints<Spec>(value, json_type(std::string(raw)), std::string(key), errors);
        if (!errors.empty()) {
            throw request_validation_error(prefix_loc(std::move(errors), source));
        }
        return value;
    }
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

struct body_context {
    const request& req;
    bool loaded = false;
    json_type value;

    explicit body_context(const request& request)
        : req(request) {}

    [[nodiscard]] const json_type& get() {
        if (loaded) {
            return value;
        }
        if (req.body.empty()) {
            throw request_validation_error({{.type = "missing",
                                             .loc = {std::string("body")},
                                             .msg = "Field required",
                                             .input = json_type()}});
        }
        try {
            value = json::parse(req.body);
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
        loaded = true;
        return value;
    }
};

template <typename T, fiasco::field<> Spec>
[[nodiscard]] T resolve_body(body_context& body, std::string_view key, bool embed) {
    using CleanT = std::decay_t<T>;
    static_assert(FromJson<CleanT>, "body parameters must be JSON-deserializable");

    const json_type& j = body.get();

    const auto validate = [&](const CleanT& value, const json_type& raw) {
        std::vector<validation_error> errors;
        apply_field_constraints<Spec>(value, raw, std::string(key), errors);
        if (!errors.empty()) {
            throw request_validation_error(prefix_loc(std::move(errors), "body"));
        }
    };

    if (embed) {
        if (!j.contains(std::string(key))) {
            throw request_validation_error({{.type = "missing",
                                             .loc = {std::string("body"), std::string(key)},
                                             .msg = "Field required",
                                             .input = j}});
        }
        const json_type& sub = j[std::string(key)];
        CleanT val{};
        try {
            from_json(sub, val);
        } catch (const request_validation_error& e) {
            throw request_validation_error(
                prefix_loc(prefix_loc(e.errors(), std::string(key)), "body"));
        } catch (const std::exception&) {
            throw request_validation_error(
                {make_type_error<CleanT>({std::string("body"), std::string(key)}, sub)});
        }
        if constexpr (Primitive<CleanT>) {
            validate(val, sub);
        } else if constexpr (OptionalPrimitive<CleanT>) {
            if (val.has_value()) {
                validate(*val, sub);
            }
        }
        return val;
    }

    CleanT val{};
    try {
        from_json(j, val);
    } catch (const request_validation_error& e) {
        throw request_validation_error(prefix_loc(e.errors(), "body"));
    } catch (const std::exception&) {
        throw request_validation_error({{.type = std::string(pydantic_type_name<CleanT>()),
                                         .loc = {std::string("body")},
                                         .msg = std::string(pydantic_message<CleanT>()),
                                         .input = j}});
    }
    if constexpr (Primitive<CleanT>) {
        validate(val, j);
    } else if constexpr (OptionalPrimitive<CleanT>) {
        if (val.has_value()) {
            validate(*val, j);
        }
    }
    return val;
}

template <typename T,
          fiasco::field<> Spec,
          param_in Source,
          std::meta::info Provider,
          dependency_occurrence Occurrence,
          bool UseCache,
          typename Cache>
decltype(auto) resolve_arg(const request& req,
                           std::size_t& path_idx,
                           std::string_view name,
                           body_context& body,
                           bool embed,
                           Cache& cache);

template <std::meta::info Provider, typename Cache, std::size_t... Is>
auto invoke_dependency(const request& req,
                       std::size_t& path_idx,
                       body_context& body,
                       bool embed,
                       Cache& cache,
                       std::index_sequence<Is...>) {
    using traits = dependency_traits<Provider>;
    return [:Provider:](
        resolve_arg<std::tuple_element_t<Is, typename traits::args_tuple>,
                    meta::field_of(std::meta::parameters_of(Provider)[Is]),
                    meta::source_of(std::meta::parameters_of(Provider)[Is]),
                    meta::depends_of(std::meta::parameters_of(Provider)[Is]),
                    dependency_occurrence{Provider, Is},
                    meta::depends_use_cache_of(std::meta::parameters_of(Provider)[Is])>(
            req,
            path_idx,
            meta::parameter_name(std::meta::parameters_of(Provider)[Is]),
            body,
            embed,
            cache)...);
}

template <typename T,
          std::meta::info Provider,
          dependency_occurrence Occurrence,
          bool UseCache,
          typename Cache>
decltype(auto) resolve_dependency(
    const request& req, std::size_t& path_idx, body_context& body, bool embed, Cache& cache) {
    using result_type = typename dependency_slot<Provider>::result_type;
    static_assert(std::same_as<std::remove_cvref_t<T>, result_type>,
                  "dependency return type does not match the handler parameter type");
    static_assert(std::is_lvalue_reference_v<T> || std::copy_constructible<result_type>,
                  "cached dependencies must be taken by reference unless they are copyable");

    auto invoke = [&]() {
        return invoke_dependency<Provider>(
            req,
            path_idx,
            body,
            embed,
            cache,
            std::make_index_sequence<dependency_traits<Provider>::arity>{});
    };

    if constexpr (UseCache) {
        auto& slot = std::get<dependency_slot<Provider>>(cache.cached);
        if (!slot.value) {
            slot.value.emplace(invoke());
        }
        return static_cast<T>(*slot.value);
    } else {
        auto& slot = std::get<occurrence_slot<Occurrence>>(cache.uncached);
        slot.value.emplace(invoke());
        return static_cast<T>(*slot.value);
    }
}

/// Resolve one handler argument according to its explicit source marker or,
/// for unmarked parameters, FastAPI-style inference.
template <typename T,
          fiasco::field<> Spec,
          param_in Source,
          std::meta::info Provider,
          dependency_occurrence Occurrence,
          bool UseCache,
          typename Cache>
decltype(auto) resolve_arg(const request& req,
                           std::size_t& path_idx,
                           std::string_view name,
                           body_context& body,
                           bool embed,
                           Cache& cache) {
    using CleanT = std::decay_t<T>;

    if constexpr (Provider != std::meta::info{}) {
        (void)Spec;
        (void)Source;
        (void)name;
        return resolve_dependency<T, Provider, Occurrence, UseCache>(
            req, path_idx, body, embed, cache);
    } else if constexpr (std::same_as<CleanT, request>) {
        (void)Source;
        (void)path_idx;
        (void)name;
        (void)body;
        (void)embed;
        (void)cache;
        return req;
    } else {
        (void)body;
        (void)embed;
        constexpr bool has_alias = Spec.alias.has;
        const std::string_view key = has_alias ? Spec.alias.view() : name;

        const auto absent = [&](const char* source) -> CleanT {
            if constexpr (OptionalPrimitive<CleanT>) {
                (void)source;
                return CleanT{};
            } else {
                throw request_validation_error({{.type = "missing",
                                                 .loc = {std::string(source), std::string(key)},
                                                 .msg = "Field required",
                                                 .input = json_type()}});
            }
        };

        if constexpr (Source == param_in::form || Source == param_in::file) {
            static_assert(!sizeof(T), "form/file parameters are not implemented");
        } else if constexpr (Source == param_in::body) {
            return resolve_body<CleanT, Spec>(body, key, embed);
        } else if constexpr (Source == param_in::path) {
            static_assert(ScalarParam<CleanT>, "path parameters must be scalar types");
            if (auto it = req.path_params.find(key); it != req.path_params.end()) {
                return parse_scalar<CleanT, Spec>(it->second, "path", key);
            }
            return absent("path");
        } else if constexpr (Source == param_in::query) {
            static_assert(ScalarParam<CleanT>, "query parameters must be scalar types");
            if (auto it = req.query_params.find(std::string(key)); it != req.query_params.end()) {
                return parse_scalar<CleanT, Spec>(it->second, "query", key);
            }
            return absent("query");
        } else if constexpr (Source == param_in::header) {
            static_assert(ScalarParam<CleanT>, "header parameters must be scalar types");
            if (const auto* value = req.find_header(key)) {
                return parse_scalar<CleanT, Spec>(*value, "header", key);
            }
            return absent("header");
        } else if constexpr (Source == param_in::cookie) {
            static_assert(ScalarParam<CleanT>, "cookie parameters must be scalar types");
            if (auto value = find_cookie(req.header("Cookie"), key)) {
                return parse_scalar<CleanT, Spec>(*value, "cookie", key);
            }
            return absent("cookie");
        } else if constexpr (ScalarParam<CleanT>) {
            if (name.empty()) {
                // Positional path parameter: function pointers expose no reflectable
                // parameter names, so they fall back to URL order.
                if (path_idx < req.ordered_path_params.size()) {
                    return parse_scalar<CleanT, Spec>(
                        req.ordered_path_params[path_idx++], "path", key);
                }
                return absent("path");
            }

            if (auto it = req.path_params.find(key); it != req.path_params.end()) {
                return parse_scalar<CleanT, Spec>(it->second, "path", key);
            }
            if (auto it = req.query_params.find(std::string(key)); it != req.query_params.end()) {
                return parse_scalar<CleanT, Spec>(it->second, "query", key);
            }
            return absent("query");
        } else if constexpr (FromJson<CleanT>) {
            return resolve_body<CleanT, Spec>(body, key, embed);
        } else {
            static_assert(!sizeof(T), "unsupported handler parameter type");
        }
    }
}

template <typename T>
using arg_storage_t =
    std::conditional_t<std::same_as<std::decay_t<T>, request>, const request&, std::decay_t<T>>;

template <typename T, std::meta::info Provider>
using parameter_storage_t = std::conditional_t<Provider != std::meta::info{}, T, arg_storage_t<T>>;

template <typename F, typename ArgsTuple, typename Cache, size_t... Is>
response dispatch_impl(F& fn,
                       const request& req,
                       std::size_t& path_idx,
                       body_context& body,
                       bool embed,
                       Cache& cache,
                       std::index_sequence<Is...>) {
    using traits = meta::callable_traits<std::decay_t<F>>;

    // braced init guarantees left-to-right evaluation; regular values are owned by the
    // tuple, while dependency references point into the per-request cache.
    std::tuple<parameter_storage_t<std::tuple_element_t<Is, ArgsTuple>,
                                   meta::depends_of(traits::parameter(Is))>...>
        args{resolve_arg<std::tuple_element_t<Is, ArgsTuple>,
                         meta::field_of(traits::parameter(Is)),
                         meta::source_of(traits::parameter(Is)),
                         meta::depends_of(traits::parameter(Is)),
                         dependency_occurrence{traits::reflection(), Is},
                         meta::depends_use_cache_of(traits::parameter(Is))>(
            req, path_idx, traits::argument_name(Is), body, embed, cache)...};

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
    static constexpr auto providers = meta::providers_for<traits::reflection()>();
    static constexpr auto uncached = meta::uncached_parameters_for<traits::reflection()>();
    static constexpr bool embed_body = body_embedding_for<traits::reflection()>();
    using cache_type = dependency_cache<providers, uncached>;

    return [fn = std::forward<F>(f)](request req) mutable -> response {
        cache_type cache;
        body_context body{req};
        std::size_t path_idx = 0;
        return dispatch_impl<decltype(fn), args_tuple>(
            fn, req, path_idx, body, embed_body, cache, std::make_index_sequence<traits::arity>{});
    };
}

}  // namespace fiasco::detail
