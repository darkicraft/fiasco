// Per-server logging: named loggers, levels, formats, access logs, and sinks.
//
//   cmake --build build --target logging
//   FIASCO_PORT=8080 ./build/examples/logging
//
//   curl localhost:8080/log
//   curl localhost:8080/log/quiet
//
// This example uses the JSON formatter and a custom sink callback. Access logs
// are enabled, so every request is also written through the same sink.

#include <fiasco/fiasco.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

}  // namespace

int main() {
    fiasco::server_config config{
        .network = {.port = example_port()},
        .logging = {
            .enabled = true,
            .access = true,
            .level = fiasco::log_level::debug,
            .format = fiasco::log_format::json,
            .colors = false,
            .sink = [](std::string_view line) {
                std::cerr << "[sink] " << line;
            },
        },
    };

    fiasco::server app{std::move(config)};
    auto& log = app.get_logger("example");

    app.get("/log", [&log]() {
        log.debug("debug message");
        log.info("info message", {{"request", "log"}});
        log.warn("warn message");
        log.error("error message");
        return fiasco::json{{"logged", true}};
    });

    app.get("/log/quiet", [&log]() {
        log.set_level(fiasco::log_level::error);
        log.debug("this one is filtered out");
        log.error("only errors are emitted now");
        return fiasco::json{{"level", "error"}};
    });

    app.run();
}
