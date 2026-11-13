// The smallest useful Fiasco server.
//
//   cmake --build build --target hello
//   ./build/examples/hello
//   curl localhost:8080/

#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::get;

namespace api {

[[=get("/")]]
auto hello() -> std::string {
    return "Hello, World!";
}

}  // namespace api

int main() {
    fiasco::app app;
    app.register_api<^^api>();
    app.run();
}
