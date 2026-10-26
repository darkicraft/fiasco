#include "fiasco/internal/types/json.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <charconv>
#include <cmath>
#include <string>

namespace fiasco::detail {

struct json_type::impl {
    nlohmann::ordered_json m_json;
};

json_type::json_type()
    : m_data(std::make_shared<impl>()) {}

json_type::json_type(std::nullptr_t)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = nullptr;
}

json_type::json_type(bool b)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = b;
}

json_type::json_type(std::int64_t i)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = i;
}

json_type::json_type(std::uint64_t u)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = u;
}

json_type::json_type(double d)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = d;
}

json_type::json_type(const std::string& s)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = s;
}

json_type::json_type(const char* s)
    : m_data(std::make_shared<impl>()) {
    m_data->m_json = s;
}

json_type::json_type(const json_type& other)
    : m_data(other.m_data),
      m_path(other.m_path) {}

json_type::json_type(json_type&& other) noexcept
    : m_data(std::move(other.m_data)),
      m_path(std::move(other.m_path)) {
    other.m_data = std::make_shared<impl>();
}

json_type::json_type(std::initializer_list<json_entry> entries)
    : m_data(std::make_shared<impl>()) {
    auto& obj = m_data->m_json;
    for (const auto& e : entries) {
        obj[e.key] = *static_cast<const nlohmann::ordered_json*>(e.value.data());
    }
}

json_type json_type::object() {
    json_type j;
    j.m_data->m_json = nlohmann::ordered_json::object();
    return j;
}

json_type json_type::array(std::initializer_list<json_type> values) {
    json_type j;
    j.m_data->m_json = nlohmann::ordered_json::array();
    auto& arr = j.m_data->m_json;
    for (const auto& v : values) {
        arr.push_back(*static_cast<const nlohmann::ordered_json*>(v.data()));
    }
    return j;
}

json_type& json_type::operator=(const json_type& other) {
    if (this != &other) {
        auto& self = *static_cast<nlohmann::ordered_json*>(data());
        const auto& src = *static_cast<const nlohmann::ordered_json*>(other.data());
        self = src;
    }
    return *this;
}

json_type& json_type::operator=(json_type&& other) noexcept {
    if (this != &other) {
        auto& self = *static_cast<nlohmann::ordered_json*>(data());
        auto& src = *static_cast<nlohmann::ordered_json*>(other.data());
        self = std::move(src);
    }
    return *this;
}

json_type json_type::operator[](const std::string& key) {
    json_type result;
    result.m_data = m_data;
    result.m_path = m_path;
    result.m_path.push_back(key);
    return result;
}

json_type json_type::operator[](std::size_t index) {
    json_type result;
    result.m_data = m_data;
    result.m_path = m_path;
    result.m_path.push_back(index);
    return result;
}

json_type json_type::operator[](const std::string& key) const {
    const auto& self = *static_cast<const nlohmann::ordered_json*>(data());
    json_type result;
    result.m_data = std::make_shared<impl>();
    // Safe lookup: a missing key yields a null value (nlohmann's const
    // operator[] on a missing key is undefined behaviour).
    if (self.is_object()) {
        if (auto it = self.find(key); it != self.end()) {
            result.m_data->m_json = *it;
        }
    }
    return result;
}

json_type json_type::operator[](std::size_t index) const {
    const auto& self = *static_cast<const nlohmann::ordered_json*>(data());
    json_type result;
    result.m_data = std::make_shared<impl>();
    if (self.is_array() && index < self.size()) {
        result.m_data->m_json = self[index];
    }
    return result;
}

bool json_type::is_null() const noexcept {
    return static_cast<const nlohmann::ordered_json*>(data())->is_null();
}

bool json_type::is_boolean() const noexcept {
    return static_cast<const nlohmann::ordered_json*>(data())->is_boolean();
}

bool json_type::is_number() const noexcept {
    return static_cast<const nlohmann::ordered_json*>(data())->is_number();
}

bool json_type::is_string() const noexcept {
    return static_cast<const nlohmann::ordered_json*>(data())->is_string();
}

bool json_type::is_object() const noexcept {
    return static_cast<const nlohmann::ordered_json*>(data())->is_object();
}

bool json_type::is_array() const noexcept {
    return static_cast<const nlohmann::ordered_json*>(data())->is_array();
}

bool json_type::contains(const std::string& key) const noexcept {
    return static_cast<const nlohmann::ordered_json*>(data())->contains(key);
}

bool json_type::empty() const noexcept {
    return static_cast<const nlohmann::ordered_json*>(data())->empty();
}

std::size_t json_type::size() const noexcept {
    return static_cast<const nlohmann::ordered_json*>(data())->size();
}

json_type json_type::parse(const std::string& str) {
    try {
        json_type j;
        j.m_data->m_json = nlohmann::ordered_json::parse(str);
        return j;
    } catch (const nlohmann::ordered_json::parse_error& e) {
        throw json_type::parse_exception(e.what(), static_cast<std::size_t>(e.byte));
    } catch (const nlohmann::ordered_json::exception& e) {
        throw json_type::exception(e.what());
    }
}

std::string json_type::dump(int indent) const {
    return static_cast<const nlohmann::ordered_json*>(data())->dump(indent);
}

void json_type::push_back(json_type value) {
    auto& arr = *static_cast<nlohmann::ordered_json*>(data());
    arr.push_back(std::move(*static_cast<nlohmann::ordered_json*>(value.data())));
}

void json_type::erase(const std::string& key) {
    auto& obj = *static_cast<nlohmann::ordered_json*>(data());
    obj.erase(key);
}

void json_type::merge_patch(const json_type& patch) {
    auto& obj = *static_cast<nlohmann::ordered_json*>(data());
    const auto& p = *static_cast<const nlohmann::ordered_json*>(patch.data());
    obj.merge_patch(p);
}

void json_type::clear() {
    static_cast<nlohmann::ordered_json*>(data())->clear();
}

bool json_type::operator==(const json_type& other) const noexcept {
    return *static_cast<const nlohmann::ordered_json*>(data()) ==
           *static_cast<const nlohmann::ordered_json*>(other.data());
}

bool json_type::operator!=(const json_type& other) const noexcept {
    return !(*this == other);
}

std::vector<std::string> json_type::object_keys() const {
    std::vector<std::string> keys;
    const auto& j = *static_cast<const nlohmann::ordered_json*>(data());
    if (!j.is_object()) {
        return keys;
    }
    for (const auto& [key, value] : j.items()) {
        keys.push_back(key);
    }
    return keys;
}

void* json_type::data() {
    nlohmann::ordered_json* current = &m_data->m_json;
    for (const auto& seg : m_path) {
        if (auto* key = std::get_if<std::string>(&seg)) {
            current = &(*current)[*key];
        } else {
            current = &(*current)[std::get<std::size_t>(seg)];
        }
    }
    return current;
}

const void* json_type::data() const {
    const nlohmann::ordered_json* current = &m_data->m_json;
    for (const auto& seg : m_path) {
        if (auto* key = std::get_if<std::string>(&seg)) {
            current = &(*current)[*key];
        } else {
            current = &(*current)[std::get<std::size_t>(seg)];
        }
    }
    return current;
}

void from_json(const json_type& j, std::nullptr_t&) {
    static_cast<const nlohmann::ordered_json*>(j.data())->get<std::nullptr_t>();
}

namespace {

std::string ascii_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// Pydantic-lax integer conversion: integers, integral floats, booleans and
// numeric strings are accepted; fractional floats are int_from_float and
// non-numeric strings are int_parsing (never a silent narrowing).
template <typename T>
void read_integral(const json_type& j, T& val) {
    const auto& n = *static_cast<const nlohmann::ordered_json*>(j.data());
    if (n.is_number_integer() || n.is_number_unsigned()) {
        val = n.get<T>();
        return;
    }
    if (n.is_number_float()) {
        const double d = n.get<double>();
        if (std::isfinite(d) && d == std::floor(d)) {
            val = static_cast<T>(d);
            return;
        }
        throw request_validation_error(
            {{.type = "int_from_float",
              .loc = {},
              .msg = "Input should be a valid integer, got a number with a fractional part",
              .input = j}});
    }
    if (n.is_boolean()) {
        val = n.get<bool>() ? T{1} : T{0};
        return;
    }
    if (n.is_string()) {
        const std::string& s = n.get_ref<const std::string&>();
        T parsed{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), parsed);
        if (ec == std::errc() && ptr == s.data() + s.size()) {
            val = parsed;
            return;
        }
        throw request_validation_error({{.type = std::string(pydantic_parsing_type_name<T>()),
                                         .loc = {},
                                         .msg = std::string(pydantic_parsing_message<T>()),
                                         .input = j}});
    }
    throw request_validation_error({{.type = std::string(pydantic_type_name<T>()),
                                     .loc = {},
                                     .msg = std::string(pydantic_message<T>()),
                                     .input = j}});
}

// Pydantic-lax float conversion: numbers, booleans and numeric strings.
template <typename T>
void read_floating(const json_type& j, T& val) {
    const auto& n = *static_cast<const nlohmann::ordered_json*>(j.data());
    if (n.is_number()) {
        val = n.get<T>();
        return;
    }
    if (n.is_boolean()) {
        val = n.get<bool>() ? T{1} : T{0};
        return;
    }
    if (n.is_string()) {
        const std::string& s = n.get_ref<const std::string&>();
        T parsed{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), parsed);
        if (ec == std::errc() && ptr == s.data() + s.size()) {
            val = parsed;
            return;
        }
        throw request_validation_error({{.type = std::string(pydantic_parsing_type_name<T>()),
                                         .loc = {},
                                         .msg = std::string(pydantic_parsing_message<T>()),
                                         .input = j}});
    }
    throw request_validation_error({{.type = std::string(pydantic_type_name<T>()),
                                     .loc = {},
                                     .msg = std::string(pydantic_message<T>()),
                                     .input = j}});
}

}  // namespace

void from_json(const json_type& j, bool& val) {
    const auto& n = *static_cast<const nlohmann::ordered_json*>(j.data());
    const auto bool_parsing = [&j] {
        return request_validation_error(
            {{.type = "bool_parsing",
              .loc = {},
              .msg = "Input should be a valid boolean, unable to interpret input",
              .input = j}});
    };
    if (n.is_boolean()) {
        val = n.get<bool>();
        return;
    }
    if (n.is_number_integer() || n.is_number_unsigned()) {
        const long long i = n.get<long long>();
        if (i == 1) {
            val = true;
            return;
        }
        if (i == 0) {
            val = false;
            return;
        }
        throw bool_parsing();
    }
    if (n.is_number_float()) {
        const double d = n.get<double>();
        if (d == 1.0) {
            val = true;
            return;
        }
        if (d == 0.0) {
            val = false;
            return;
        }
        throw bool_parsing();
    }
    if (n.is_string()) {
        const std::string s = ascii_lower(n.get_ref<const std::string&>());
        if (s == "1" || s == "true" || s == "yes" || s == "on" || s == "t" || s == "y" ||
            s == "enable") {
            val = true;
            return;
        }
        if (s == "0" || s == "false" || s == "no" || s == "off" || s == "f" || s == "n" ||
            s == "disable") {
            val = false;
            return;
        }
        throw bool_parsing();
    }
    throw request_validation_error(
        {{.type = "bool_type", .loc = {}, .msg = "Input should be a valid boolean", .input = j}});
}

void from_json(const json_type& j, std::int64_t& val) {
    read_integral(j, val);
}
void from_json(const json_type& j, std::uint64_t& val) {
    read_integral(j, val);
}
void from_json(const json_type& j, double& val) {
    read_floating(j, val);
}

void from_json(const json_type& j, std::string& val) {
    const auto& n = *static_cast<const nlohmann::ordered_json*>(j.data());
    if (n.is_string()) {
        val = n.get<std::string>();
        return;
    }
    throw request_validation_error(
        {{.type = "string_type", .loc = {}, .msg = "Input should be a valid string", .input = j}});
}

void to_json(json_type& j, std::nullptr_t) {
    *static_cast<nlohmann::ordered_json*>(j.data()) = nullptr;
}

void to_json(json_type& j, bool val) {
    *static_cast<nlohmann::ordered_json*>(j.data()) = val;
}

void to_json(json_type& j, std::int64_t val) {
    *static_cast<nlohmann::ordered_json*>(j.data()) = val;
}

void to_json(json_type& j, std::uint64_t val) {
    *static_cast<nlohmann::ordered_json*>(j.data()) = val;
}

void to_json(json_type& j, double val) {
    *static_cast<nlohmann::ordered_json*>(j.data()) = val;
}

void to_json(json_type& j, const std::string& val) {
    *static_cast<nlohmann::ordered_json*>(j.data()) = val;
}

void to_json(json_type& j, const char* val) {
    *static_cast<nlohmann::ordered_json*>(j.data()) = val;
}

json_type::~json_type() = default;

}  // namespace fiasco::detail
