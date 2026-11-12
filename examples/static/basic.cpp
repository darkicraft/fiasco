// Static file mounts.
//
//   cmake --build build --target static_files
//   FIASCO_PORT=8080 ./build/examples/static_files
//
//   curl localhost:8080/static/
//   curl localhost:8080/static/style.css
//   curl -I localhost:8080/static/style.css

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

    app.get("/", [] { return "try /static/"; });
    app.mount_static("/static", "examples/static/public", {.html = true});

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
