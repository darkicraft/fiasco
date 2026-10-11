#pragma once

#include <cstddef>
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
concept JsonTupleLike =
    requires(T& t) { std::tuple_size<std::decay_t<T>>::value; } && !JsonRange<T> && !JsonOptional<T>;

// An aggregate "model": (de)serialised member-wise via reflection. Excludes the
// categories handled above (ranges/maps/optionals/tuples/JSON itself).
template <typename T>
concept JsonModel = std::is_class_v<T> && std::meta::is_aggregate_type(^^T) &&
                    !JsonRange<T> && !JsonOptional<T> && !JsonTupleLike<T> &&
                    !std::same_as<std::remove_cvref_t<T>, json_type>;

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
    seq.clear();
    auto n = j.size();
    if constexpr (requires { seq.reserve(n); }) {
        seq.reserve(n);
    }
    for (std::size_t i = 0; i < n; ++i) {
        typename T::value_type elem;
        from_json(j[i], elem);
        if constexpr (requires(T& c, typename T::value_type& v) { c.push_back(v); }) {
            seq.push_back(std::move(elem));
        } else {
            seq.insert(seq.end(), std::move(elem));
        }
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
    map.clear();
    auto keys = j.object_keys();
    for (const auto& key : keys) {
        typename T::mapped_type val;
        from_json(j[key], val);
        if constexpr (std::convertible_to<decltype(key), typename T::key_type>) {
            map[key] = std::move(val);
        } else {
            typename T::key_type k(key);
            map[std::move(k)] = std::move(val);
        }
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
void to_json_field(json_type& j, const char* key, const T& field) {
    j[key] = json_type(field);
}

template <typename T>
void from_json_field(const json_type& j, const char* key, std::optional<T>& field) {
    if (j.contains(key)) {
        field = j[key].get<std::optional<T>>();
    } else {
        field.reset();
    }
}

template <typename T>
void from_json_field(const json_type& j, const char* key, T& field) {
    field = j[key].get<T>();
}

// ---- Aggregate models (reflection) ----------------------------------------
// Member-wise (de)serialisation driven by std::meta; replaces the FIASCO_MODEL
// macro. Members without an identifier (e.g. anonymous unions, unnamed
// bit-fields) are skipped.
template <JsonModel T>
void to_json(json_type& j, const T& v) {
    json_type obj = json_type::object();
    template for (constexpr auto member :
                  std::define_static_array(std::meta::nonstatic_data_members_of(
                      ^^T, std::meta::access_context::unchecked()))) {
        if constexpr (std::meta::has_identifier(member)) {
            to_json_field(obj, std::meta::identifier_of(member).data(), v.[:member:]);
        }
    }
    j = std::move(obj);
}

template <JsonModel T>
void from_json(const json_type& j, T& v) {
    template for (constexpr auto member :
                  std::define_static_array(std::meta::nonstatic_data_members_of(
                      ^^T, std::meta::access_context::unchecked()))) {
        if constexpr (std::meta::has_identifier(member)) {
            from_json_field(j, std::meta::identifier_of(member).data(), v.[:member:]);
        }
    }
}

}  // namespace fiasco::detail