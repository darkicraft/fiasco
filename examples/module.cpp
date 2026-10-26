// GCC's module implementation requires the importer to make standard headers
// used by the library's templates visible; fiasco's reflection traits use
// std::vector (pulled in by <meta>).
#include <vector>

import fiasco.server;

int main() {
    fiasco::server app;
    app.get("/", []() { return fiasco::json{{"msg", "root"}}; });
    app.run();
}