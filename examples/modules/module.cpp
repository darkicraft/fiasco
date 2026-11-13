// C++20 module import example (parked; not built while modules are disabled).
//
// The module surface is retained under modules/ but is currently broken:
// fiasco.logging references a removed logger header, and the exported aliases
// do not cover the current application API.
//
//   cmake -S . -B build-modules -G Ninja
//   cmake --build build-modules --target module
//   ./build-modules/examples/module
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
