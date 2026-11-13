#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "fiasco/contract/response.hpp"
#include "fiasco/stream/generator.hpp"

namespace fiasco::stream_detail {

template <typename T, typename Serialize>
class serialized_source {
  public:
    serialized_source(fiasco::stream<T> source, Serialize serialize)
        : m_source(std::move(source)),
          m_serialize(std::move(serialize)) {}

    auto read() -> detail::task<std::optional<std::span<const std::byte>>> {
        auto value = co_await m_source.next();
        if (!value) {
            co_return std::nullopt;
        }

        m_current = m_serialize(*value);
        co_return std::as_bytes(std::span<const char>(m_current.data(), m_current.size()));
    }

    void cancel() noexcept { m_source.cancel(); }

  private:
    fiasco::stream<T> m_source;
    Serialize m_serialize;
    std::string m_current;
};

template <typename T, typename Serialize>
auto make_serialized_response(std::string content_type,
                              fiasco::stream<T> source,
                              Serialize serialize) -> detail::response {
    auto state =
        std::make_shared<serialized_source<T, Serialize>>(std::move(source), std::move(serialize));

    detail::response result;
    result.headers["Content-Type"] = std::move(content_type);
    result.stream_source = std::make_shared<detail::body_source>(detail::body_source{
        .read = [state]() -> detail::task<std::optional<std::span<const std::byte>>> {
            co_return co_await state->read();
        },
        .cancel = [state] { state->cancel(); },
    });
    return result;
}

}  // namespace fiasco::stream_detail
