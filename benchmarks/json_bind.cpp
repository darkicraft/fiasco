// Fiasco side of the JSON bind-and-validate benchmark.
//
//   ./benchmarks/fiasco_json_bind 8080
//   wrk -t4 -c50 -d10s -s benchmarks/post.lua http://127.0.0.1:8080/validate

#include <fiasco/fiasco.hpp>

#include <cstdint>
#include <cstdlib>
#include <string>

namespace {

struct user_input {
    std::string username;
    int age;
};

struct user_view {
    std::string username;
    int age;
};

}  // namespace

int main(int argc, char** argv) {
    const auto port = argc > 1 ? static_cast<std::uint16_t>(std::stoi(argv[1]))
                               : static_cast<std::uint16_t>(8080);
    unsigned int workers = 0;
    if (const char* value = std::getenv("FIASCO_WORKERS")) {
        workers = static_cast<unsigned int>(std::stoul(value));
    }

    fiasco::app app;

    app.post("/validate", [](user_input input) -> user_view {
        return {input.username, input.age + 1};
    });

    app.run(fiasco::server_config{
        .network = {.port = port, .workers = workers},
    });
}
