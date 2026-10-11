#include "fiasco/internal/logging/access_log.hpp"

#include "fiasco/internal/http/request.hpp"

namespace fiasco::detail {

std::string access_log::build_message() const {
    std::string msg;
    msg += client;
    msg += " \"";
    msg += method_to_string(method);
    msg += ' ';
    msg += path;
    msg += " HTTP/1.1\" ";
    msg += std::to_string(status_code);
    msg += ' ';
    msg += std::to_string(duration_us / 1000);
    msg += "ms";
    return msg;
}

}  // namespace fiasco::detail
