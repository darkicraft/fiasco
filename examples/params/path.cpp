// Path parameters are bound by name, not position.
//
//   cmake --build build --target path_params
//   FIASCO_PORT=8080 ./build/examples/path_params
//
//   curl 'localhost:8080/types/7/9000000000/1234567890123/0.25/9.5/true/hello'
//   curl 'localhost:8080/fixed/127/65535/2147483647/9000000000'
//   curl 'localhost:8080/types/not-an-int/1/2/0.5/1.5/false/x'   # 422
//
// The parameter order in the lambda is intentionally scrambled: names, not
// positions, decide the binding.

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

    app.get("/types/{i}/{lg}/{ll}/{f}/{d}/{b}/{s}",
            [](std::string s, bool b, double d, float f, long long ll, long lg, int i) {
                return fiasco::json{{"int", i},
                                    {"long", lg},
                                    {"long_long", ll},
                                    {"float", f},
                                    {"double", d},
                                    {"bool", b},
                                    {"string", s}};
            });

    app.get("/fixed/{i32}/{u16}/{u64}",
            [](std::int32_t i32, std::uint16_t u16, std::uint64_t u64) {
                return fiasco::json{{"int32", i32}, {"uint16", u16}, {"uint64", u64}};
            });

    app.run(fiasco::server_config{.network = {.port = example_port()}});
}
