#pragma once

// Structural value types used to build annotation payloads.
//
// An annotation value (P3394) must have a *structural type*, but the obvious
// std types are not structural in libstdc++:
//   * `std::optional<T>` has a private base (`_Optional_base`)
//   * `std::string_view` has a private member (`_M_len`)
// and a `const char*` to a *string literal* cannot be reflected at all
// ([temp.arg.nontype] forbids pointer/reference template arguments that point
// at a string literal). These small all-public replacements fix both:
//   * `opt<T>` — structural `std::optional`
//   * `lit`    — structural string, backed by `std::define_static_string`,
//                which materialises a real static object (a permitted constant).

#include <meta>

#include <cstddef>
#include <string_view>

namespace fiasco::detail {

/// Structural replacement for `std::optional<T>`.
template <class T>
struct opt {
    T value{};
    bool has = false;

    constexpr opt() = default;
    constexpr opt(T v)
        : value(v),
          has(true) {}

    [[nodiscard]] constexpr bool engaged() const { return has; }
};

/// Structural replacement for `std::string_view` holding a string literal.
struct lit {
    const char* p = nullptr;
    bool has = false;

    constexpr lit() = default;

    template <std::size_t N>
    constexpr lit(const char (&s)[N])
        : p(std::define_static_string(s)),
          has(true) {}

    [[nodiscard]] constexpr std::string_view view() const {
        return p ? std::string_view{p} : std::string_view{};
    }
};

}  // namespace fiasco::detail
