// Server configuration: host, port, workers, and lifecycle control.
//
//   cmake --build build --target server_configuration
//   FIASCO_PORT=8080 ./build/examples/server_configuration
//
//   curl localhost:8080/info
//
// Bind a specific address with FIASCO_HOST. Use FIASCO_PORT=0 to let the OS
// choose a free port, then read the actual port with server.port().

#include <fiasco/fiasco.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

std::string example_host() {
    if (const char* value = std::getenv("FIASCO_HOST")) {
        return value;
    }
    return "0.0.0.0";
}

}  // namespace

int main() {
    fiasco::server app{fiasco::server_config{
        .network = {
            .host = example_host(),
            .port = example_port(),
            .workers = 4,
        },
    }};

    app.get("/info", [&app]() {
        return fiasco::json{
            {"port", app.port()},
            {"workers", 4},
            {"status", "running"},
        };
    });

    // run() blocks, so start it on another thread to report the bound port.
    std::thread runner([&app] { app.run(); });
    app.wait_until_started();
    std::cout << "listening on port " << app.port() << '\n';

    runner.join();
}
