#pragma once

#include <array>
#include <cstddef>
#include <meta>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "fiasco/internal/meta/annotations.hpp"
#include "fiasco/internal/meta/reflect.hpp"

namespace fiasco::detail {

/// One per-request cache slot. `F` is the reflected provider function, so two
/// providers returning the same type still get distinct slots.
template <std::meta::info F>
struct dependency_slot {
    static_assert(std::meta::is_function(F), "a dependency must be a function");
    using result_type = [:std::meta::return_type_of(F):];

    static_assert(!std::is_void_v<result_type>, "a dependency must return a value");
    static_assert(!std::is_reference_v<result_type>,
                  "a cached dependency must return its value, not a reference");

    std::optional<result_type> value;
};

template <std::meta::info F>
struct dependency_traits {
    static constexpr std::size_t arity = std::meta::parameters_of(F).size();
    using args_tuple = [:meta::arguments_type_impl_for(F, std::make_index_sequence<arity>()):];
};

template <auto Providers, typename Seq>
struct dependency_cache_impl;

template <auto Providers, std::size_t... Is>
struct dependency_cache_impl<Providers, std::index_sequence<Is...>> {
    std::tuple<dependency_slot<Providers[Is]>...> slots;
};

/// A per-handler cache. `Providers` is a std::array of reflected functions.
template <auto Providers>
using dependency_cache =
    dependency_cache_impl<Providers, std::make_index_sequence<Providers.size()>>;

namespace meta {

inline constexpr std::size_t max_dependency_nodes = 64;

consteval void collect_dependency(std::meta::info item,
                                  std::array<std::meta::info, max_dependency_nodes>& providers,
                                  std::size_t& provider_count,
                                  std::array<std::meta::info, max_dependency_nodes>& stack,
                                  std::size_t& stack_count) {
    const auto dependency = depends_of(item);
    if (dependency == std::meta::info{}) {
        return;
    }

    for (std::size_t i = 0; i < stack_count; ++i) {
        if (stack[i] == dependency) {
            throw std::meta::exception("dependency cycle detected", dependency);
        }
    }
    for (std::size_t i = 0; i < provider_count; ++i) {
        if (providers[i] == dependency) {
            return;
        }
    }

    if (provider_count == max_dependency_nodes || stack_count == max_dependency_nodes) {
        throw std::meta::exception("too many dependencies", dependency);
    }

    providers[provider_count++] = dependency;
    stack[stack_count++] = dependency;
    for (auto parameter : std::meta::parameters_of(dependency)) {
        collect_dependency(parameter, providers, provider_count, stack, stack_count);
    }
    --stack_count;
}

template <std::meta::info Callable>
consteval std::size_t dependency_count() {
    std::array<std::meta::info, max_dependency_nodes> providers{};
    std::array<std::meta::info, max_dependency_nodes> stack{};
    std::size_t provider_count = 0;
    std::size_t stack_count = 0;

    for (auto parameter : std::meta::parameters_of(Callable)) {
        collect_dependency(parameter, providers, provider_count, stack, stack_count);
    }
    return provider_count;
}

template <std::meta::info Callable>
consteval auto providers_for() {
    constexpr std::size_t count = dependency_count<Callable>();
    std::array<std::meta::info, count> result{};

    std::array<std::meta::info, max_dependency_nodes> providers{};
    std::array<std::meta::info, max_dependency_nodes> stack{};
    std::size_t provider_count = 0;
    std::size_t stack_count = 0;

    for (auto parameter : std::meta::parameters_of(Callable)) {
        collect_dependency(parameter, providers, provider_count, stack, stack_count);
    }

    if constexpr (count > 0) {
        for (std::size_t i = 0; i < count; ++i) {
            result[i] = providers[i];
        }
    }
    return result;
}

}  // namespace meta

}  // namespace fiasco::detail
