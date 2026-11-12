// File responses and download disposition.
//
//   cmake --build build --target response_file
//   FIASCO_PORT=8080 ./build/examples/response_file
//
//   curl -i localhost:8080/inline
//   curl -OJ localhost:8080/download

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
    fiasco::app app;

    app.get("/inline", [] {
        return fiasco::response::file("examples/responses/download.txt");
    });

    app.get("/download", [] {
        return fiasco::response::file(
            "examples/responses/download.txt",
            {.filename = "fiasco-download.txt"});
    });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
