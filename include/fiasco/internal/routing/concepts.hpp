#pragma once

#include <concepts>
#include <string>

#include "fiasco/internal/types/json.hpp"

namespace fiasco::detail {

template <typename T>
concept Primitive = std::same_as<T, int> || std::same_as<T, long> || std::same_as<T, long long> ||
                    std::same_as<T, float> || std::same_as<T, double> || std::same_as<T, bool> ||
                    std::same_as<T, std::string>;

template <typename T>
concept FromJson = JsonDeserializable<T>;

template <typename T>
concept ToJson = JsonSerializable<T>;

}  // namespace fiasco::detail