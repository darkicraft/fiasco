// The smallest useful fiasco server.
//
//   cmake --build build --target hello
//   FIASCO_PORT=8080 ./build/examples/hello
//   curl localhost:8080/

#include <fiasco/fiasco.hpp>

#include <cstdint>
#include <cstdlib>
#include <string>

namespace {

std::uint16_t example_port() {
    if (const char* value = std::getenv("FIASCO_PORT")) {
        return static_cast<std::uint16_t>(std::stoi(value));
    }
    return 8080;
}

}  // namespace

int main() {
    fiasco::server app{
        fiasco::server_config{.network = {.port = example_port()}},
    };

    app.get("/", []() { return "Hello, World!"; });

    app.run();
}
