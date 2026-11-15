#pragma once

/// @file
/// @brief Small vector-backed header map.
/// @internal

// HTTP messages carry a handful of headers, so a linear vector is faster than
// a hash map: no bucket array, no per-entry node allocation, and request
// capacity is retained across keep-alive messages. The interface covers the
// subset of std::unordered_map used by the framework and its tests.

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fiasco::detail {

class header_map {
  public:
    using value_type = std::pair<std::string, std::string>;
    using iterator = std::vector<value_type>::iterator;
    using const_iterator = std::vector<value_type>::const_iterator;

    [[nodiscard]] bool empty() const noexcept { return m_entries.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }

    void clear() noexcept { m_entries.clear(); }
    void reserve(std::size_t count) { m_entries.reserve(count); }

    iterator begin() noexcept { return m_entries.begin(); }
    iterator end() noexcept { return m_entries.end(); }
    const_iterator begin() const noexcept { return m_entries.begin(); }
    const_iterator end() const noexcept { return m_entries.end(); }
    const_iterator cbegin() const noexcept { return m_entries.cbegin(); }
    const_iterator cend() const noexcept { return m_entries.cend(); }

    iterator find(std::string_view key) noexcept {
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->first == key) {
                return it;
            }
        }
        return m_entries.end();
    }

    const_iterator find(std::string_view key) const noexcept {
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->first == key) {
                return it;
            }
        }
        return m_entries.end();
    }

    [[nodiscard]] bool contains(std::string_view key) const noexcept {
        return find(key) != m_entries.end();
    }

    [[nodiscard]] std::size_t count(std::string_view key) const noexcept {
        return contains(key) ? 1 : 0;
    }

    std::string& operator[](std::string_view key) {
        if (auto it = find(key); it != m_entries.end()) {
            return it->second;
        }
        m_entries.emplace_back(std::string(key), std::string());
        return m_entries.back().second;
    }

    std::string& operator[](std::string&& key) {
        if (auto it = find(key); it != m_entries.end()) {
            return it->second;
        }
        m_entries.emplace_back(std::move(key), std::string());
        return m_entries.back().second;
    }

    std::string& operator[](const std::string& key) { return (*this)[std::string_view(key)]; }

    std::string& operator[](const char* key) { return (*this)[std::string_view(key)]; }

    std::string& at(std::string_view key) {
        if (auto it = find(key); it != m_entries.end()) {
            return it->second;
        }
        throw std::out_of_range("header not found");
    }

    const std::string& at(std::string_view key) const {
        if (auto it = find(key); it != m_entries.end()) {
            return it->second;
        }
        throw std::out_of_range("header not found");
    }

    std::size_t erase(std::string_view key) {
        if (auto it = find(key); it != m_entries.end()) {
            m_entries.erase(it);
            return 1;
        }
        return 0;
    }

    template <typename Key, typename Value>
    void emplace(Key&& key, Value&& value) {
        const std::string_view view(key);
        if (auto it = find(view); it != m_entries.end()) {
            it->second = std::forward<Value>(value);
            return;
        }
        m_entries.emplace_back(std::forward<Key>(key), std::forward<Value>(value));
    }

    template <typename Key, typename Value>
    void insert_or_assign(Key&& key, Value&& value) {
        emplace(std::forward<Key>(key), std::forward<Value>(value));
    }

  private:
    std::vector<value_type> m_entries;
};

}  // namespace fiasco::detail
