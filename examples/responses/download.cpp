// File responses and download disposition.
//
//   cmake --build build --target response_file
//   ./build/examples/responses/download
//
//   curl -i localhost:8080/inline
//   curl -OJ localhost:8080/download

#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::get;
using fiasco::response;

namespace routes {

[[=get("/inline")]]
auto inline_file() -> response {
    return response::file("examples/responses/download.txt");
}

[[=get("/download")]]
auto download() -> response {
    return response::file("examples/responses/download.txt", {.filename = "fiasco-download.txt"});
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.run();
}
