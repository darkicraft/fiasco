#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>

#include "fiasco/fiasco.hpp"

namespace {

struct static_fixture {
    std::filesystem::path dir;

    static_fixture()
        : dir(std::filesystem::temp_directory_path() /
              ("fiasco-static-test-" + std::to_string(std::random_device{}()))) {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir / "nested");
        write("index.html", "<h1>home</h1>");
        write("style.css", "body{}");
        write("nested/data.json", R"({"ok":true})");
        write("plain.txt", "hello");
    }

    ~static_fixture() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    void write(const std::filesystem::path& relative, std::string_view body) {
        std::ofstream file(dir / relative, std::ios::binary);
        file.write(body.data(), static_cast<std::streamsize>(body.size()));
    }
};

fiasco::request make_request(std::string_view method, std::string url) {
    fiasco::request req;
    req.method = fiasco::detail::string_to_method(std::string(method));
    req.url = std::move(url);
    req.reanchor_url_views();
    return req;
}

}  // namespace

TEST_CASE("static mounts serve files with content types", "[static]") {
    static_fixture fixture;
    fiasco::app app;
    app.mount_static("/assets", fixture.dir, {.html = true});

    fiasco::test_client client{app};

    const auto css = client.handle(make_request("GET", "/assets/style.css"));
    REQUIRE(css.status_code == 200);
    CHECK(css.headers.at("Content-Type") == "text/css; charset=utf-8");
    CHECK(css.body == "body{}");

    const auto json = client.handle(make_request("GET", "/assets/nested/data.json"));
    REQUIRE(json.status_code == 200);
    CHECK(json.headers.at("Content-Type") == "application/json");
    CHECK(json.body == R"({"ok":true})");

    const auto text = client.handle(make_request("GET", "/assets/plain.txt"));
    REQUIRE(text.status_code == 200);
    CHECK(text.headers.at("Content-Type") == "text/plain; charset=utf-8");
    CHECK(text.body == "hello");
}

TEST_CASE("static mounts serve index.html for directories", "[static]") {
    static_fixture fixture;
    fiasco::app app;
    app.mount_static("/assets", fixture.dir, {.html = true});

    fiasco::test_client client{app};

    for (const auto* path : {"/assets", "/assets/"}) {
        const auto res = client.handle(make_request("GET", path));
        REQUIRE(res.status_code == 200);
        CHECK(res.headers.at("Content-Type") == "text/html; charset=utf-8");
        CHECK(res.body == "<h1>home</h1>");
    }
}

TEST_CASE("static mounts require html mode for directories", "[static]") {
    static_fixture fixture;
    fiasco::app app;
    app.mount_static("/files", fixture.dir);

    fiasco::test_client client{app};
    CHECK(client.handle(make_request("GET", "/files/")).status_code == 404);
    CHECK(client.handle(make_request("GET", "/files/style.css")).status_code == 200);
}

TEST_CASE("static mounts support HEAD", "[static]") {
    static_fixture fixture;
    fiasco::app app;
    app.mount_static("/assets", fixture.dir, {.html = true});

    fiasco::test_client client{app};
    const auto res = client.handle(make_request("HEAD", "/assets/style.css"));

    REQUIRE(res.status_code == 200);
    CHECK(res.body.empty());
    CHECK(res.headers.at("Content-Type") == "text/css; charset=utf-8");
    CHECK(res.headers.at("Content-Length") ==
          std::to_string(std::filesystem::file_size(fixture.dir / "style.css")));
}

TEST_CASE("static mounts reject missing files and traversal", "[static]") {
    static_fixture fixture;
    const auto secret = fixture.dir.parent_path() / "fiasco-static-secret.txt";
    {
        std::ofstream file(secret);
        file << "secret";
    }

    fiasco::app app;
    app.mount_static("/assets", fixture.dir, {.html = true});
    fiasco::test_client client{app};

    CHECK(client.handle(make_request("GET", "/assets/missing.txt")).status_code == 404);
    CHECK(client.handle(make_request("GET", "/assets/../fiasco-static-secret.txt")).status_code ==
          404);
    CHECK(
        client.handle(make_request("GET", "/assets/%2e%2e/fiasco-static-secret.txt")).status_code ==
        404);

    std::error_code ec;
    std::filesystem::remove(secret, ec);
}

TEST_CASE("static mounts reject unsupported methods", "[static]") {
    static_fixture fixture;
    fiasco::app app;
    app.mount_static("/assets", fixture.dir, {.html = true});

    fiasco::test_client client{app};
    const auto res = client.handle(make_request("POST", "/assets/style.css"));

    REQUIRE(res.status_code == 405);
    CHECK(res.headers.at("Allow") == "GET, HEAD");
}
