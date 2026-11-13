// Path parameters are bound by name, not position.
//
//   cmake --build build --target path_params
//   ./build/examples/path_params
//
//   curl 'localhost:8080/types/7/9000000000/1234567890123/0.25/9.5/true/hello'
//   curl 'localhost:8080/fixed/127/65535/2147483647/9000000000'
//   curl 'localhost:8080/types/not-an-int/1/2/0.5/1.5/false/x'   # 422
//
// The parameter order is intentionally scrambled: names, not positions,
// decide the binding.

#include <fiasco/fiasco.hpp>

#include <cstdint>
#include <string>

using fiasco::get;
using fiasco::json;

namespace routes {

[[=get("/types/{i}/{lg}/{ll}/{f}/{d}/{b}/{s}")]]
auto all_types(
    std::string s, bool b, double d, float f, long long ll, long lg, int i) -> json {
    return json{{"int", i},
                {"long", lg},
                {"long_long", ll},
                {"float", f},
                {"double", d},
                {"bool", b},
                {"string", s}};
}

[[=get("/fixed/{i32}/{u16}/{u64}")]]
auto fixed_types(std::int32_t i32,
                                                       std::uint16_t u16,
                                                       std::uint64_t u64) -> json {
    return json{{"int32", i32}, {"uint16", u16}, {"uint64", u64}};
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
