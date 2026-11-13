// Static file mounts.
//
//   cmake --build build --target static_files
//   ./build/examples/static_files
//
//   curl localhost:8080/static/
//   curl localhost:8080/static/style.css
//   curl -I localhost:8080/static/style.css

#include <fiasco/fiasco.hpp>

#include <string>

using fiasco::get;

namespace routes {

[[=get("/")]]
auto root() -> std::string {
    return "try /static/";
}

}  // namespace routes

int main() {
    fiasco::app app;
    app.register_api<^^routes>();
    app.mount_static("/static", "examples/static/public", {.html = true});
    app.run();
}
