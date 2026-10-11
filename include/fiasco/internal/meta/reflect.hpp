#pragma once

// Reflection foundation for fiasco.
//
// GCC 16.2 notes that shape this layer:
//   * Range-returning metafunctions (parameters_of, members_of, ...) return
//     std::vector<info>, which cannot escape constant evaluation ("refers to a
//     result of operator new"); use std::define_static_array to materialise them
//     for `template for` / storage.
//   * `^^T::operator()` reflects a lambda/functor's call operator *as a function
//     entity*, so its parameters carry names. A function *type* (e.g. what a
//     function pointer decays to) only carries parameter types, not names.
//   * Templates instantiated from a module importer cannot look up std names
//     that the template body spells out (e.g. an explicit `std::vector`); this
//     header therefore avoids materialising std containers itself.

#include <meta>

#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace fiasco::detail::meta {

/// The reflection of the function (or function type) that `F` invokes:
///   * class types (lambdas / functors) -> their call operator
///   * function pointers / function types -> the function (type) itself
template <class F>
consteval std::meta::info callable_reflection() {
    using D = std::decay_t<F>;
    if constexpr (std::is_class_v<D>) {
        return ^^D::operator();
    } else if constexpr (std::is_pointer_v<D>) {
        return std::meta::remove_pointer(^^D);
    } else {
        return ^^D;
    }
}

/// The type represented by an entry of `parameters_of(...)`: entries are
/// parameter reflections when reflecting a function entity, but plain type
/// reflections when reflecting a function type.
consteval std::meta::info parameter_type(std::meta::info param) {
    return std::meta::is_type(param) ? param : std::meta::type_of(param);
}

template <class F, std::size_t... Is>
consteval std::meta::info arguments_type_impl(std::index_sequence<Is...>) {
    return std::meta::substitute(
        ^^std::tuple, {parameter_type(std::meta::parameters_of(callable_reflection<F>())[Is])...});
}

/// Introspection of a callable type, computed with reflection instead of
/// hand-written template specialisations.
template <class F>
struct callable_traits {
    /// Reflection of the callable's function (see callable_reflection).
    static consteval std::meta::info reflection() { return callable_reflection<F>(); }

    /// Number of explicit parameters.
    static constexpr std::size_t arity = std::meta::parameters_of(reflection()).size();

    /// std::tuple<parameter types...> (type-adjusted, as reported by type_of).
    using args_tuple = [: arguments_type_impl<F>(std::make_index_sequence<arity>()) :];

    using return_type = [: std::meta::return_type_of(reflection()) :];

    /// True if the callable's parameters carry names (function entities and
    /// lambda call operators do; function types do not).
    static consteval bool compute_has_argument_names() {
        if (arity == 0) {
            return false;
        }
        std::meta::info param = std::meta::parameters_of(reflection())[0];
        return !std::meta::is_type(param) && std::meta::has_identifier(param);
    }
    static constexpr bool has_argument_names = compute_has_argument_names();

    /// Name of the i-th parameter, or an empty view if it has none.
    static consteval std::string_view argument_name(std::size_t i) {
        std::meta::info param = std::meta::parameters_of(reflection())[i];
        if (!std::meta::is_type(param) && std::meta::has_identifier(param)) {
            return std::meta::identifier_of(param);
        }
        return {};
    }
};

}  // namespace fiasco::detail::meta
