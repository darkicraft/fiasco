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
                  "a dependency must return its value, not a reference");

    std::optional<result_type> value;
};

/// Identity of one dependency parameter occurrence: the owning callable and the
/// parameter index. Parameter reflection equality is not reliable for this
/// purpose, so the owner/index pair is the cache key.
struct dependency_occurrence {
    std::meta::info owner{};
    std::size_t index = 0;

    constexpr bool operator==(const dependency_occurrence&) const = default;
};

/// One storage slot for a non-cached dependency occurrence.
template <dependency_occurrence Occurrence>
struct occurrence_slot {
    static constexpr auto provider =
        meta::depends_of(std::meta::parameters_of(Occurrence.owner)[Occurrence.index]);
    static_assert(provider != std::meta::info{}, "an occurrence slot requires a dependency");
    using result_type = [:std::meta::return_type_of(provider):];

    static_assert(!std::is_void_v<result_type>, "a dependency must return a value");
    static_assert(!std::is_reference_v<result_type>,
                  "a dependency must return its value, not a reference");

    std::optional<result_type> value;
};

template <std::meta::info F>
struct dependency_traits {
    static constexpr std::size_t arity = std::meta::parameters_of(F).size();
    using args_tuple = [:meta::arguments_type_impl_for(F, std::make_index_sequence<arity>()):];
};

template <auto Providers, auto Occurrences, typename CachedSeq, typename UncachedSeq>
struct dependency_cache_impl;

template <auto Providers, auto Occurrences, std::size_t... Cached, std::size_t... Uncached>
struct dependency_cache_impl<Providers,
                             Occurrences,
                             std::index_sequence<Cached...>,
                             std::index_sequence<Uncached...>> {
    std::tuple<dependency_slot<Providers[Cached]>...> cached;
    std::tuple<occurrence_slot<Occurrences[Uncached]>...> uncached;
};

/// A per-handler cache. `Providers` is a std::array of reflected functions and
/// `Occurrences` is a std::array of non-cached dependency parameters.
template <auto Providers, auto Occurrences>
using dependency_cache = dependency_cache_impl<Providers,
                                               Occurrences,
                                               std::make_index_sequence<Providers.size()>,
                                               std::make_index_sequence<Occurrences.size()>>;

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

consteval void add_uncached_occurrence(std::array<dependency_occurrence, max_dependency_nodes>& out,
                                       std::size_t& count,
                                       dependency_occurrence occurrence) {
    for (std::size_t i = 0; i < count; ++i) {
        if (out[i].owner == occurrence.owner && out[i].index == occurrence.index) {
            return;
        }
    }
    if (count == max_dependency_nodes) {
        throw std::meta::exception("too many uncached dependencies", occurrence.owner);
    }
    out[count++] = occurrence;
}

consteval void collect_uncached_occurrence(
    std::meta::info owner,
    std::size_t index,
    std::array<dependency_occurrence, max_dependency_nodes>& out,
    std::size_t& count,
    std::array<std::meta::info, max_dependency_nodes>& stack,
    std::size_t& stack_count) {
    const auto parameters = std::meta::parameters_of(owner);
    const auto dependency = depends_of(parameters[index]);
    if (dependency == std::meta::info{}) {
        return;
    }

    for (std::size_t i = 0; i < stack_count; ++i) {
        if (stack[i] == dependency) {
            throw std::meta::exception("dependency cycle detected", dependency);
        }
    }
    if (stack_count == max_dependency_nodes) {
        throw std::meta::exception("too many dependencies", dependency);
    }

    if (!depends_use_cache_of(parameters[index])) {
        add_uncached_occurrence(out, count, dependency_occurrence{owner, index});
    }

    stack[stack_count++] = dependency;
    const auto nested = std::meta::parameters_of(dependency);
    for (std::size_t i = 0; i < nested.size(); ++i) {
        collect_uncached_occurrence(dependency, i, out, count, stack, stack_count);
    }
    --stack_count;
}

template <std::meta::info Callable>
consteval std::size_t uncached_count() {
    std::array<dependency_occurrence, max_dependency_nodes> occurrences{};
    std::array<std::meta::info, max_dependency_nodes> stack{};
    std::size_t count = 0;
    std::size_t stack_count = 0;

    for (std::size_t i = 0; i < std::meta::parameters_of(Callable).size(); ++i) {
        collect_uncached_occurrence(Callable, i, occurrences, count, stack, stack_count);
    }
    return count;
}

template <std::meta::info Callable>
consteval auto uncached_parameters_for() {
    constexpr std::size_t count = uncached_count<Callable>();
    std::array<dependency_occurrence, count> result{};

    std::array<dependency_occurrence, max_dependency_nodes> occurrences{};
    std::array<std::meta::info, max_dependency_nodes> stack{};
    std::size_t actual_count = 0;
    std::size_t stack_count = 0;

    for (std::size_t i = 0; i < std::meta::parameters_of(Callable).size(); ++i) {
        collect_uncached_occurrence(Callable, i, occurrences, actual_count, stack, stack_count);
    }

    if constexpr (count > 0) {
        for (std::size_t i = 0; i < count; ++i) {
            result[i] = occurrences[i];
        }
    }
    return result;
}

}  // namespace meta

}  // namespace fiasco::detail
