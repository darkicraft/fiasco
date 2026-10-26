#pragma once

#include <concepts>
#include <string>

#include "fiasco/internal/types/json.hpp"

namespace fiasco::detail {

template <typename T>
concept Primitive = std::same_as<T, bool> || std::same_as<T, std::string> ||
                    std::integral<T> || std::floating_point<T>;

template <typename T>
concept FromJson = JsonDeserializable<T>;

template <typename T>
concept ToJson = JsonSerializable<T>;

}  // namespace fiasco::detail