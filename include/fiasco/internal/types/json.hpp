#pragma once

#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <meta>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "fiasco/internal/meta/annotations.hpp"

namespace fiasco::detail {

class json_type;
struct json_entry;

template <typename T>
concept JsonSerializable = requires(json_type& j, const T& v) { to_json(j, v); };

template <typename T>
concept JsonDeserializable = requires(const json_type& j, T& v) { from_json(j, v); };

class json_type {
  public:
    struct exception : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // Thrown by parse() for invalid JSON; carries the byte offset of the error
    // (mapped to FastAPI's json_invalid "loc": ["body", <pos>]).
    struct parse_exception : exception {
        parse_exception(const std::string& message, std::size_t position)
            : exception(message),
              m_position(position) {}

        [[nodiscard]] std::size_t position() const noexcept { return m_position; }

      private:
        std::size_t m_position;
    };

    json_type();
    json_type(std::nullptr_t);
    json_type(bool);
    json_type(int);
    json_type(long);
    json_type(long long);
    json_type(unsigned long);
    json_type(unsigned long long);
    json_type(float);
    json_type(double);
    json_type(const std::string&);
    json_type(const char*);
    json_type(const json_type&);
    json_type(json_type&&) noexcept;
    json_type(std::initializer_list<json_entry>);

    static json_type array(std::initializer_list<json_type> values);
    static json_type object();

    template <JsonSerializable T>
    json_type(const T& val)
        : json_type() {
        to_json(*this, val);
    }

    json_type& operator=(const json_type&);
    json_type& operator=(json_type&&) noexcept;

    json_type operator[](const std::string& key);
    json_type operator[](std::size_t index);
    json_type operator[](const std::string& key) const;
    json_type operator[](std::size_t index) const;

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] bool is_boolean() const noexcept;
    [[nodiscard]] bool is_number() const noexcept;
    [[nodiscard]] bool is_string() const noexcept;
    [[nodiscard]] bool is_object() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;

    [[nodiscard]] bool contains(const std::string& key) const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    template <JsonDeserializable T>
    [[nodiscard]] T get() const {
        T val;
        from_json(*this, val);
        return val;
    }

    template <JsonDeserializable T>
    [[nodiscard]] T value(const std::string& key, const T& default_val) const {
        if (contains(key)) {
            return (*this)[key].get<T>();
        }
        return default_val;
    }

    static json_type parse(const std::string& str);
    [[nodiscard]] std::string dump(int indent = -1) const;

    void push_back(json_type value);
    void erase(const std::string& key);
    void merge_patch(const json_type& patch);
    void clear();

    [[nodiscard]] std::vector<std::string> object_keys() const;

    [[nodiscard]] bool operator==(const json_type&) const noexcept;
    [[nodiscard]] bool operator!=(const json_type&) const noexcept;

    ~json_type();

    void* data();
    const void* data() const;

  private:
    using path_segment = std::variant<std::string, std::size_t>;

    struct impl;
    std::shared_ptr<impl> m_data;
    std::vector<path_segment> m_path;
};

// Enables json_type{{"key", val}, {"k2", v2}} without ambiguity vs arrays.
struct json_entry {
    std::string key;
    json_type value;
};

// ---- Built-in primitives -------------------------------------------------
void from_json(const json_type&, std::nullptr_t&);
void from_json(const json_type&, bool&);
void from_json(const json_type&, int&);
void from_json(const json_type&, long&);
void from_json(const json_type&, long long&);
void from_json(const json_type&, unsigned long&);
void from_json(const json_type&, unsigned long long&);
void from_json(const json_type&, float&);
void from_json(const json_type&, double&);
void from_json(const json_type&, std::string&);

void to_json(json_type&, std::nullptr_t);
void to_json(json_type&, bool);
void to_json(json_type&, int);
void to_json(json_type&, long);
void to_json(json_type&, long long);
void to_json(json_type&, unsigned long);
void to_json(json_type&, unsigned long long);
void to_json(json_type&, float);
void to_json(json_type&, double);
void to_json(json_type&, const std::string&);
void to_json(json_type&, const char*);

// ---- json_type self ------------------------------------------------------
template <typename T>
    requires std::same_as<std::remove_cvref_t<T>, json_type>
void to_json(json_type& j, const T& val) {
    j = val;
}

template <typename T>
    requires std::same_as<std::remove_cvref_t<T>, json_type>
void from_json(const json_type& j, T& val) {
    val = j;
}

// ---- Pydantic-style validation errors (FastAPI 422 "detail" entries) -------
struct validation_error {
    std::string type;
    std::vector<std::variant<std::string, std::size_t>> loc;
    std::string msg;
    json_type input;
    std::optional<json_type> ctx{};  // Pydantic's per-error context (constraints)
};

class request_validation_error : public std::exception {
  public:
    explicit request_validation_error(std::vector<validation_error> errors)
        : m_errors(std::move(errors)) {}

    [[nodiscard]] const std::vector<validation_error>& errors() const noexcept { return m_errors; }

    [[nodiscard]] const char* what() const noexcept override { return "request validation error"; }

  private:
    std::vector<validation_error> m_errors;
};

/// Serialise validation errors the way FastAPI does: {"detail": [ ... ]}.
[[nodiscard]] inline std::string to_fastapi_error_json(
    const std::vector<validation_error>& errors) {
    json_type list = json_type::array({});
    for (const auto& err : errors) {
        json_type entry = json_type::object();
        entry["type"] = json_type(err.type);
        json_type loc = json_type::array({});
        for (const auto& segment : err.loc) {
            if (const auto* name = std::get_if<std::string>(&segment)) {
                loc.push_back(json_type(*name));
            } else {
                loc.push_back(json_type(static_cast<long long>(std::get<std::size_t>(segment))));
            }
        }
        entry["loc"] = loc;
        entry["msg"] = json_type(err.msg);
        entry["input"] = err.input;
        if (err.ctx.has_value()) {
            entry["ctx"] = *err.ctx;
        }
        list.push_back(entry);
    }
    json_type out = json_type::object();
    out["detail"] = list;
    return out.dump();
}

/// Prepend a location segment (e.g. "body") to every error's loc, the way
/// Pydantic nests inner errors under their containing field.
[[nodiscard]] inline std::vector<validation_error> prefix_loc(std::vector<validation_error> errors,
                                                              const std::string& segment) {
    for (auto& err : errors) {
        err.loc.insert(err.loc.begin(), segment);
    }
    return errors;
}

// ---- Container / optional / tuple traits ---------------------------------
template <typename T>
concept JsonRange = requires(T& t) {
    std::begin(t);
    std::end(t);
};

template <typename T>
concept JsonMap = JsonRange<T> && requires { typename T::mapped_type; };

// NOTE: checked before JsonSequence. Since C++26 std::optional models a range
// (P3168 range support), it would otherwise be mistaken for a sequence.
template <typename T>
concept JsonOptional = requires(T& t) {
    typename T::value_type;
    t.has_value();
    t.value();
};

template <typename T>
concept JsonSequence = JsonRange<T> && !JsonMap<T> && !JsonOptional<T>;

template <typename T>
concept JsonTupleLike = requires(T& t) { std::tuple_size<std::decay_t<T>>::value; } &&
                        !JsonRange<T> && !JsonOptional<T>;

// An aggregate "model": (de)serialised member-wise via reflection. Excludes the
// categories handled above (ranges/maps/optionals/tuples/JSON itself).
template <typename T>
concept JsonModel =
    std::is_class_v<T> && std::meta::is_aggregate_type(^^T) && !JsonRange<T> && !JsonOptional<T> &&
    !JsonTupleLike<T> && !std::same_as<std::remove_cvref_t<T>, json_type>;

// ---- Pydantic v2 error vocabulary (the subset we can match) ----------------
template <typename T>
consteval std::string_view pydantic_type_name() {
    using D = std::remove_cvref_t<T>;
    if constexpr (std::same_as<D, bool>) {
        return "bool_type";
    } else if constexpr (std::integral<D>) {
        return "int_type";
    } else if constexpr (std::floating_point<D>) {
        return "float_type";
    } else if constexpr (std::same_as<D, std::string>) {
        return "string_type";
    } else if constexpr (JsonMap<D>) {
        return "dict_type";
    } else if constexpr (JsonSequence<D>) {
        return "list_type";
    } else if constexpr (JsonModel<D>) {
        return "model_type";
    } else {
        return "value_error";
    }
}

template <typename T>
consteval std::string_view pydantic_message() {
    using D = std::remove_cvref_t<T>;
    if constexpr (std::same_as<D, bool>) {
        return "Input should be a valid boolean";
    } else if constexpr (std::integral<D>) {
        return "Input should be a valid integer";
    } else if constexpr (std::floating_point<D>) {
        return "Input should be a valid number";
    } else if constexpr (std::same_as<D, std::string>) {
        return "Input should be a valid string";
    } else if constexpr (JsonMap<D>) {
        return "Input should be a valid dictionary";
    } else if constexpr (JsonSequence<D>) {
        return "Input should be a valid list";
    } else if constexpr (JsonModel<D>) {
        return "Input should be a valid dictionary";
    } else {
        return "Invalid value";
    }
}

template <typename T>
consteval std::string_view pydantic_parsing_type_name() {
    using D = std::remove_cvref_t<T>;
    if constexpr (std::same_as<D, bool>) {
        return "bool_parsing";
    } else if constexpr (std::integral<D>) {
        return "int_parsing";
    } else if constexpr (std::floating_point<D>) {
        return "float_parsing";
    } else {
        return "value_error";
    }
}

template <typename T>
consteval std::string_view pydantic_parsing_message() {
    using D = std::remove_cvref_t<T>;
    if constexpr (std::same_as<D, bool>) {
        return "Input should be a valid boolean, unable to interpret input";
    } else if constexpr (std::integral<D>) {
        return "Input should be a valid integer, unable to parse string as an integer";
    } else if constexpr (std::floating_point<D>) {
        return "Input should be a valid number, unable to parse string as a number";
    } else {
        return "Invalid value";
    }
}

/// Build a Pydantic-style error for a value that failed to become `T`. Uses the
/// `*_parsing` vocabulary when a scalar field received a JSON string (matching
/// Pydantic), and the `*_type` vocabulary otherwise.
template <typename T>
[[nodiscard]] inline validation_error make_type_error(
    std::vector<std::variant<std::string, std::size_t>> loc, const json_type& value) {
    using D = std::remove_cvref_t<T>;
    constexpr bool scalar = std::integral<D> || std::floating_point<D> || std::same_as<D, bool>;
    const bool parsing = scalar && value.is_string();
    return validation_error{
        .type = std::string(parsing ? pydantic_parsing_type_name<D>() : pydantic_type_name<D>()),
        .loc = std::move(loc),
        .msg = std::string(parsing ? pydantic_parsing_message<D>() : pydantic_message<D>()),
        .input = value};
}

template <JsonSequence T>
void to_json(json_type& j, const T& seq) {
    json_type arr = json_type::array({});
    for (const auto& elem : seq) {
        arr.push_back(json_type(elem));
    }
    j = std::move(arr);
}

template <JsonSequence T>
void from_json(const json_type& j, T& seq) {
    using ValueT = typename T::value_type;
    std::vector<validation_error> errors;

    seq.clear();
    auto n = j.size();
    if constexpr (requires { seq.reserve(n); }) {
        seq.reserve(n);
    }
    for (std::size_t i = 0; i < n; ++i) {
        ValueT elem{};
        try {
            from_json(j[i], elem);
        } catch (const request_validation_error& nested) {
            for (auto err : nested.errors()) {
                err.loc.insert(err.loc.begin(), i);
                errors.push_back(std::move(err));
            }
            continue;
        } catch (const std::exception&) {
            errors.push_back(make_type_error<ValueT>({i}, j[i]));
            continue;
        }
        if constexpr (requires(T& c, ValueT& v) { c.push_back(v); }) {
            seq.push_back(std::move(elem));
        } else {
            seq.insert(seq.end(), std::move(elem));
        }
    }

    if (!errors.empty()) {
        throw request_validation_error(std::move(errors));
    }
}

template <JsonMap T>
void to_json(json_type& j, const T& map) {
    json_type obj;
    for (const auto& [key, value] : map) {
        json_type jv(value);
        // Use lvalue so copy-assignment via data() is picked, not move-assignment
        // (move-assignment on a sub-view temporary would detach from the parent).
        if constexpr (std::convertible_to<decltype(key), std::string>) {
            obj[static_cast<const std::string&>(key)] = jv;
        } else {
            obj[std::to_string(key)] = jv;
        }
    }
    j = std::move(obj);
}

template <JsonMap T>
void from_json(const json_type& j, T& map) {
    using MappedT = typename T::mapped_type;
    std::vector<validation_error> errors;

    map.clear();
    auto keys = j.object_keys();
    for (const auto& key : keys) {
        MappedT val{};
        try {
            from_json(j[key], val);
        } catch (const request_validation_error& nested) {
            for (auto err : nested.errors()) {
                err.loc.insert(err.loc.begin(), key);
                errors.push_back(std::move(err));
            }
            continue;
        } catch (const std::exception&) {
            errors.push_back(make_type_error<MappedT>({key}, j[key]));
            continue;
        }
        if constexpr (std::convertible_to<decltype(key), typename T::key_type>) {
            map[key] = std::move(val);
        } else {
            typename T::key_type k(key);
            map[std::move(k)] = std::move(val);
        }
    }

    if (!errors.empty()) {
        throw request_validation_error(std::move(errors));
    }
}

template <JsonOptional T>
void to_json(json_type& j, const T& opt) {
    if (opt.has_value()) {
        to_json(j, opt.value());
    } else {
        to_json(j, nullptr);
    }
}

template <JsonOptional T>
void from_json(const json_type& j, T& opt) {
    using Inner = typename T::value_type;
    if (j.is_null()) {
        opt.reset();
    } else {
        Inner val;
        from_json(j, val);
        opt = std::move(val);
    }
}

template <JsonTupleLike T>
void to_json(json_type& j, const T& tuple) {
    json_type arr = json_type::array({});
    std::apply([&arr](const auto&... args) { (arr.push_back(json_type(args)), ...); }, tuple);
    j = std::move(arr);
}

template <JsonTupleLike T, size_t... Is>
void from_json_tuple_impl(const json_type& j, T& tuple, std::index_sequence<Is...>) {
    ((from_json(j[Is], std::get<Is>(tuple))), ...);
}

template <JsonTupleLike T>
void from_json(const json_type& j, T& tuple) {
    from_json_tuple_impl(j, tuple, std::make_index_sequence<std::tuple_size_v<T>>{});
}

// ---- Field-level helpers ---------------------------------------------------
template <typename T>
void to_json_field(json_type& j, std::string_view key, const T& field) {
    j[std::string(key)] = json_type(field);
}

// ---- Constraint engine (Pydantic-exact) ------------------------------------
// `Spec` is a non-type template parameter so its values are constant
// expressions; that lets the applicability checks below be static_asserts.

/// Render a numeric bound the way Pydantic does *in messages* (no trailing
/// ".0"): 5.0 -> "5", 5.5 -> "5.5".
[[nodiscard]] inline std::string constraint_number(double v) {
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    if (ec != std::errc()) {
        return std::to_string(v);
    }
    return std::string(buf, ptr);
}

constexpr bool is_integral_value(double v) {
    if (v < -1.0e18 || v > 1.0e18) {
        return false;
    }
    return static_cast<double>(static_cast<long long>(v)) == v;
}

constexpr const char* plural(std::size_t n, const char* one, const char* many) {
    return n == 1 ? one : many;
}

/// ctx keeps the field's numeric type: integers for integer fields, floats
/// (e.g. 5.0) for floating fields.
template <class D>
[[nodiscard]] json_type numeric_ctx(const char* name, double bound) {
    json_type ctx = json_type::object();
    if constexpr (std::integral<std::remove_cvref_t<D>>) {
        ctx[name] = json_type(static_cast<long long>(bound));
    } else {
        ctx[name] = json_type(bound);
    }
    return ctx;
}

template <fiasco::field<> Spec, class D>
void apply_field_constraints(const D& value, const json_type& raw, const std::string& loc_key,
                             std::vector<validation_error>& errors) {
    using U = std::remove_cvref_t<D>;
    constexpr bool is_bool = std::same_as<U, bool>;
    constexpr bool is_number = !is_bool && (std::integral<U> || std::floating_point<U>);
    constexpr bool is_string = std::same_as<U, std::string>;
    constexpr bool is_sized = JsonSequence<U> || JsonMap<U>;
    constexpr bool is_lengthy = is_string || is_sized;

    static_assert(!Spec.gt.has || is_number, "'gt' is only valid on numeric fields");
    static_assert(!Spec.ge.has || is_number, "'ge' is only valid on numeric fields");
    static_assert(!Spec.lt.has || is_number, "'lt' is only valid on numeric fields");
    static_assert(!Spec.le.has || is_number, "'le' is only valid on numeric fields");
    static_assert(!Spec.multiple_of.has || is_number,
                  "'multiple_of' is only valid on numeric fields");
    static_assert(!Spec.min_length.has || is_lengthy,
                  "'min_length' is only valid on string/sequence fields");
    static_assert(!Spec.max_length.has || is_lengthy,
                  "'max_length' is only valid on string/sequence fields");
    static_assert(!Spec.pattern.has, "'pattern' is not implemented yet");
    if constexpr (is_number && std::integral<U>) {
        static_assert(!Spec.gt.has || is_integral_value(Spec.gt.value),
                      "'gt' must be an integer on an integer field");
        static_assert(!Spec.ge.has || is_integral_value(Spec.ge.value),
                      "'ge' must be an integer on an integer field");
        static_assert(!Spec.lt.has || is_integral_value(Spec.lt.value),
                      "'lt' must be an integer on an integer field");
        static_assert(!Spec.le.has || is_integral_value(Spec.le.value),
                      "'le' must be an integer on an integer field");
        static_assert(!Spec.multiple_of.has || is_integral_value(Spec.multiple_of.value),
                      "'multiple_of' must be an integer on an integer field");
    }

    auto raise = [&](std::string type, std::string msg, json_type ctx) {
        errors.push_back(validation_error{.type = std::move(type),
                                          .loc = {loc_key},
                                          .msg = std::move(msg),
                                          .input = raw,
                                          .ctx = std::move(ctx)});
    };

    if constexpr (is_number) {
        const double v = static_cast<double>(value);
        // Pydantic's check order (verified): multiple_of, le, lt, ge, gt.
        if (Spec.multiple_of.has) {
            bool ok = false;
            if constexpr (std::integral<U>) {
                const auto m = static_cast<long long>(Spec.multiple_of.value);
                ok = m != 0 && static_cast<long long>(value) % m == 0;
            } else {
                const double m = Spec.multiple_of.value;
                ok = m != 0.0 && std::fmod(v, m) == 0.0;
            }
            if (!ok) {
                raise("multiple_of",
                      "Input should be a multiple of " +
                          constraint_number(Spec.multiple_of.value),
                      numeric_ctx<D>("multiple_of", Spec.multiple_of.value));
                return;
            }
        }
        if (Spec.le.has && !(v <= Spec.le.value)) {
            raise("less_than_equal",
                  "Input should be less than or equal to " + constraint_number(Spec.le.value),
                  numeric_ctx<D>("le", Spec.le.value));
            return;
        }
        if (Spec.lt.has && !(v < Spec.lt.value)) {
            raise("less_than", "Input should be less than " + constraint_number(Spec.lt.value),
                  numeric_ctx<D>("lt", Spec.lt.value));
            return;
        }
        if (Spec.ge.has && !(v >= Spec.ge.value)) {
            raise("greater_than_equal",
                  "Input should be greater than or equal to " + constraint_number(Spec.ge.value),
                  numeric_ctx<D>("ge", Spec.ge.value));
            return;
        }
        if (Spec.gt.has && !(v > Spec.gt.value)) {
            raise("greater_than", "Input should be greater than " + constraint_number(Spec.gt.value),
                  numeric_ctx<D>("gt", Spec.gt.value));
            return;
        }
    } else if constexpr (is_string) {
        const auto n = value.size();
        if (Spec.min_length.has && n < Spec.min_length.value) {
            json_type ctx = json_type::object();
            ctx["min_length"] = json_type(static_cast<std::uint64_t>(Spec.min_length.value));
            raise("string_too_short",
                  "String should have at least " + std::to_string(Spec.min_length.value) +
                      plural(Spec.min_length.value, " character", " characters"),
                  std::move(ctx));
            return;
        }
        if (Spec.max_length.has && n > Spec.max_length.value) {
            json_type ctx = json_type::object();
            ctx["max_length"] = json_type(static_cast<std::uint64_t>(Spec.max_length.value));
            raise("string_too_long",
                  "String should have at most " + std::to_string(Spec.max_length.value) +
                      plural(Spec.max_length.value, " character", " characters"),
                  std::move(ctx));
            return;
        }
    } else if constexpr (is_sized) {
        const char* field_type = JsonMap<U> ? "Dictionary" : "List";
        const auto n = value.size();
        if (Spec.min_length.has && n < Spec.min_length.value) {
            json_type ctx = json_type::object();
            ctx["field_type"] = json_type(field_type);
            ctx["min_length"] = json_type(static_cast<std::uint64_t>(Spec.min_length.value));
            ctx["actual_length"] = json_type(static_cast<std::uint64_t>(value.size()));
            raise("too_short",
                  std::string(field_type) + " should have at least " +
                      std::to_string(Spec.min_length.value) +
                      plural(Spec.min_length.value, " item", " items") +
                      " after validation, not " + std::to_string(value.size()),
                  std::move(ctx));
            return;
        }
        if (Spec.max_length.has && n > Spec.max_length.value) {
            json_type ctx = json_type::object();
            ctx["field_type"] = json_type(field_type);
            ctx["max_length"] = json_type(static_cast<std::uint64_t>(Spec.max_length.value));
            ctx["actual_length"] = json_type(static_cast<std::uint64_t>(value.size()));
            raise("too_long",
                  std::string(field_type) + " should have at most " +
                      std::to_string(Spec.max_length.value) +
                      plural(Spec.max_length.value, " item", " items") +
                      " after validation, not " + std::to_string(value.size()),
                  std::move(ctx));
            return;
        }
    }
}

// ---- Aggregate models (reflection) ----------------------------------------
// Member-wise (de)serialisation driven by std::meta; replaces the FIASCO_MODEL
// macro. Members without an identifier (e.g. anonymous unions, unnamed
// bit-fields) are skipped. Deserialisation collects *all* FastAPI-style
// validation errors instead of failing fast.
//
// Requiredness is exact 1:1 with Pydantic: a member is *required* iff it has no
// default member initializer; `std::optional<T>` controls nullability only.
template <JsonModel T>
void to_json(json_type& j, const T& v) {
    json_type obj = json_type::object();
    template for (constexpr auto member :
                  std::define_static_array(std::meta::nonstatic_data_members_of(
                      ^^T, std::meta::access_context::unchecked()))) {
        if constexpr (std::meta::has_identifier(member)) {
            static_assert(fiasco::detail::meta::field_annotation_count(member) <= 1,
                          "only one field/path/query/body annotation is allowed per member");
            constexpr auto spec = fiasco::detail::meta::field_of(member);
            constexpr std::string_view key =
                spec.alias.has ? spec.alias.view() : std::meta::identifier_of(member);
            to_json_field(obj, key, v.[:member:]);
        }
    }
    j = std::move(obj);
}

template <JsonModel T>
void from_json(const json_type& j, T& v) {
    std::vector<validation_error> errors;

    template for (constexpr auto member :
                  std::define_static_array(std::meta::nonstatic_data_members_of(
                      ^^T, std::meta::access_context::unchecked()))) {
        if constexpr (std::meta::has_identifier(member)) {
            static_assert(fiasco::detail::meta::field_annotation_count(member) <= 1,
                          "only one field/path/query/body annotation is allowed per member");
            using FieldT = std::remove_cvref_t<decltype(v.[:member:])>;
            constexpr auto spec = fiasco::detail::meta::field_of(member);
            constexpr bool required = !std::meta::has_default_member_initializer(member);
            constexpr std::string_view key =
                spec.alias.has ? spec.alias.view() : std::meta::identifier_of(member);
            const std::string key_str(key);

            if (!j.contains(key_str)) {
                if constexpr (required) {
                    errors.push_back(validation_error{
                        .type = "missing", .loc = {key_str}, .msg = "Field required", .input = j});
                }
                continue;
            }

            try {
                from_json(j[key_str], v.[:member:]);
            } catch (const request_validation_error& nested) {
                for (auto err : nested.errors()) {
                    err.loc.insert(err.loc.begin(), key_str);
                    errors.push_back(std::move(err));
                }
                continue;
            } catch (const std::exception&) {
                errors.push_back(make_type_error<FieldT>({key_str}, j[key_str]));
                continue;
            }

            if constexpr (JsonOptional<FieldT>) {
                if (v.[:member:].has_value()) {
                    apply_field_constraints<spec>(*v.[:member:], j[key_str], key_str, errors);
                }
            } else {
                apply_field_constraints<spec>(v.[:member:], j[key_str], key_str, errors);
            }
        }
    }

    if (!errors.empty()) {
        throw request_validation_error(std::move(errors));
    }
}

}  // namespace fiasco::detail