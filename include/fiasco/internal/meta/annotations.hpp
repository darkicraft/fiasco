#pragma once

// Annotation DSL for FastAPI-style field/parameter metadata.
//
//   struct User {
//       int age [[= field{.gt = 5, .le = 10}]];
//       std::string name [[= field{.min_length = 2, .alias = "user"}]];
//   };
//
//   app.get("/items/{id}",
//           [](long id [[= path{.ge = 1}]],
//              std::optional<int> page [[= query{}]]) { ... });
//
//   app.get("/me", [](User user [[= depends{^^get_current_user}]]) { ... });
//
// Markers are TYPES (`field<source>` + aliases), so `field{...}`/`path{...}`
// are syntactically uniform. `depends` stores the reflected provider function.
//
// GCC 16.2 constraints that shape the payload (all verified):
//   * An annotation value must be *structural*. `std::optional<T>` (private
//     base) and `std::string_view` (private _M_len) are NOT, hence `opt<T>`/`lit`
//     (see types/structural.hpp).
//   * A pointer to a *string literal* cannot be reflected ("reflect_constant
//     failed"); `lit` stores a pointer produced by `std::define_static_string`,
//     which materialises a real static object and therefore is reflectable.

#include "fiasco/internal/types/structural.hpp"

#include <meta>

namespace fiasco {

/// Where a handler parameter is bound from. `infer` means "figure it out"
/// (path if the name is in the route, else query for scalars, else body).
enum class param_in { infer, path, query, header, cookie, body, form, file };

/// Field/parameter metadata. `In` is the explicit source; `field<>` (i.e.
/// `field<param_in::infer>`) is the "inferred" form used on model members.
template <param_in In = param_in::infer>
struct field {
    detail::opt<double> gt{}, ge{}, lt{}, le{}, multiple_of{};
    detail::opt<std::size_t> min_length{}, max_length{};
    detail::lit pattern{}, alias{}, title{}, description{};
    bool embed = false;

    static constexpr param_in in = In;
};

using path = field<param_in::path>;
using query = field<param_in::query>;
using body = field<param_in::body>;
using header = field<param_in::header>;
using cookie = field<param_in::cookie>;
using form = field<param_in::form>;
using file = field<param_in::file>;

/// A handler or dependency parameter supplied by another callable. `fn` is the
/// reflected function entity, e.g. `[[= depends{^^get_current_user}]]`.
struct depends {
    std::meta::info fn{};
};

}  // namespace fiasco

namespace fiasco::detail::meta {

/// True if `type` is some `field<...>` specialisation.
consteval bool is_field_type(std::meta::info type) {
    return std::meta::has_template_arguments(type) &&
           std::meta::template_of(type) == ^^fiasco::field;
}

/// How many `field<...>` annotations `item` carries.
consteval std::size_t field_annotation_count(std::meta::info item) {
    std::size_t count = 0;
    for (auto a : std::meta::annotations_of(item)) {
        if (is_field_type(std::meta::type_of(a))) {
            ++count;
        }
    }
    return count;
}

/// The source declared by a marker, or `infer` if there is none.
consteval fiasco::param_in source_of(std::meta::info item) {
    for (auto a : std::meta::annotations_of(item)) {
        auto type = std::meta::type_of(a);
        if (is_field_type(type)) {
            return std::meta::extract<fiasco::param_in>(std::meta::template_arguments_of(type)[0]);
        }
    }
    return fiasco::param_in::infer;
}

namespace detail {

template <fiasco::param_in In>
consteval fiasco::field<> to_infer(fiasco::field<In> f) {
    return fiasco::field<>{
        .gt = f.gt,
        .ge = f.ge,
        .lt = f.lt,
        .le = f.le,
        .multiple_of = f.multiple_of,
        .min_length = f.min_length,
        .max_length = f.max_length,
        .pattern = f.pattern,
        .alias = f.alias,
        .title = f.title,
        .description = f.description,
        .embed = f.embed,
    };
}

template <class M>
consteval fiasco::field<> read_field(std::meta::info item) {
    return to_infer(std::meta::extract<M>(std::meta::annotations_of_with_type(item, ^^M)[0]));
}

}  // namespace detail

/// The dependency function attached to a parameter, or a null reflection if
/// the parameter is not annotated with `depends`.
consteval std::meta::info depends_of(std::meta::info item) {
    const auto annotations = std::meta::annotations_of_with_type(item, ^^fiasco::depends);
    if (annotations.empty()) {
        return {};
    }
    return std::meta::extract<fiasco::depends>(annotations[0]).fn;
}

/// The `field` metadata attached to a member or parameter, normalised to the
/// inferred form (`field<>`), or a default-constructed `field<>` if none.
consteval fiasco::field<> field_of(std::meta::info item) {
    if (!std::meta::annotations_of_with_type(item, ^^fiasco::field<>).empty()) {
        return detail::read_field<fiasco::field<>>(item);
    }
    if (!std::meta::annotations_of_with_type(item, ^^fiasco::path).empty()) {
        return detail::read_field<fiasco::path>(item);
    }
    if (!std::meta::annotations_of_with_type(item, ^^fiasco::query).empty()) {
        return detail::read_field<fiasco::query>(item);
    }
    if (!std::meta::annotations_of_with_type(item, ^^fiasco::body).empty()) {
        return detail::read_field<fiasco::body>(item);
    }
    if (!std::meta::annotations_of_with_type(item, ^^fiasco::header).empty()) {
        return detail::read_field<fiasco::header>(item);
    }
    if (!std::meta::annotations_of_with_type(item, ^^fiasco::cookie).empty()) {
        return detail::read_field<fiasco::cookie>(item);
    }
    if (!std::meta::annotations_of_with_type(item, ^^fiasco::form).empty()) {
        return detail::read_field<fiasco::form>(item);
    }
    if (!std::meta::annotations_of_with_type(item, ^^fiasco::file).empty()) {
        return detail::read_field<fiasco::file>(item);
    }
    return {};
}

}  // namespace fiasco::detail::meta
