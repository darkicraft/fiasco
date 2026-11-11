// C++20 module import example (requires FIASCO_BUILD_MODULES=ON and Ninja).
//
//   cmake -S . -B build-modules -G Ninja -DFIASCO_BUILD_MODULES=ON
//   cmake --build build-modules --target module
//   FIASCO_PORT=8080 ./build-modules/examples/module
//   curl localhost:8080/
//
// GCC's module implementation requires the importer to make standard headers
// used by the library's templates visible; fiasco's reflection traits use
// std::vector (pulled in by <meta>).
#include <vector>

import fiasco.server;

int main() {
    fiasco::app app;
    app.get("/", []() { return fiasco::json{{"msg", "root"}}; });
    app.run();
}
