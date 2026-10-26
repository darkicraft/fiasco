#pragma once

#include <functional>
#include <ostream>
#include <string_view>
#include <utility>

namespace fiasco {

using log_sink = std::move_only_function<void(std::string_view)>;

inline log_sink make_ostream_log_sink(std::ostream& os, bool flush = true) {
    return [&os, flush](std::string_view formatted) {
        os.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
        if (flush) {
            os.flush();
        }
    };
}

}  // namespace fiasco
