#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>

#include "fiasco/fiasco.hpp"
#include "support/http_client.hpp"
#include "support/test_routes.hpp"
#include "support/test_server.hpp"

namespace {

struct file_fixture {
    std::filesystem::path path;

    explicit file_fixture(std::string_view contents)
        : path(std::filesystem::temp_directory_path() /
               ("fiasco-file-response-" + std::to_string(std::random_device{}()) + ".txt")) {
        std::ofstream file(path, std::ios::binary);
        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    ~file_fixture() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
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

TEST_CASE("response::file records file metadata and infers its media type", "[file_response]") {
    file_fixture fixture{"hello from a file"};

    const auto res = fiasco::response::file(fixture.path);
    CHECK(res.status_code == 200);
    CHECK(res.body.empty());
    CHECK(res.file_source != nullptr);
    CHECK(res.file_source->size == 17);
    CHECK(res.headers.at("Content-Type") == "text/plain; charset=utf-8");
    CHECK_FALSE(res.headers.contains("Content-Disposition"));
}

TEST_CASE("response::file sets Content-Disposition for downloads", "[file_response]") {
    file_fixture fixture{"report"};

    const auto res = fiasco::response::file(fixture.path, {.filename = "report.txt"});
    CHECK(res.headers.at("Content-Disposition") == "attachment; filename=\"report.txt\"");
}

TEST_CASE("response::file supports inline and media type overrides", "[file_response]") {
    file_fixture fixture{"report"};

    const auto res = fiasco::response::file(fixture.path,
                                            {.filename = "report.txt",
                                             .media_type = "application/custom",
                                             .content_disposition_type = "inline"});
    CHECK(res.headers.at("Content-Type") == "application/custom");
    CHECK(res.headers.at("Content-Disposition") == "inline; filename=\"report.txt\"");
}

TEST_CASE("response::file escapes quoted filenames", "[file_response]") {
    file_fixture fixture{"report"};

    const auto res = fiasco::response::file(fixture.path, {.filename = "a\"b.txt"});
    CHECK(res.headers.at("Content-Disposition") == "attachment; filename=\"a\\\"b.txt\"");
}

TEST_CASE("response::file rejects missing files", "[file_response]") {
    REQUIRE_THROWS_AS(
        fiasco::response::file(std::filesystem::temp_directory_path() / "missing-fiasco-file"),
        std::runtime_error);
}

TEST_CASE("response::file works through the application seam", "[file_response]") {
    file_fixture fixture{"download me"};

    fiasco::app app;
    fiasco_test::get(app, "/download", [path = fixture.path] {
        return fiasco::response::file(path, {.filename = "download.txt"});
    });

    fiasco::test_client client{app};
    const auto res = client.handle(make_request("GET", "/download"));

    REQUIRE(res.status_code == 200);
    CHECK(res.body == "download me");
    CHECK(res.headers.at("Content-Disposition") == "attachment; filename=\"download.txt\"");
    CHECK(res.headers.at("Accept-Ranges") == "bytes");
    CHECK(res.headers.at("Content-Length") == "11");
    CHECK(res.headers.contains("ETag"));
    CHECK(res.headers.contains("Last-Modified"));
}

TEST_CASE("response::file supports byte ranges", "[file_response][range]") {
    file_fixture fixture{"0123456789"};

    fiasco::app app;
    fiasco_test::get(app, "/file", [path = fixture.path] { return fiasco::response::file(path); });

    fiasco::test_client client{app};
    auto req = make_request("GET", "/file");
    req.headers["Range"] = "bytes=2-5";
    const auto res = client.handle(std::move(req));

    REQUIRE(res.status_code == 206);
    CHECK(res.body == "2345");
    CHECK(res.headers.at("Content-Range") == "bytes 2-5/10");
    CHECK(res.headers.at("Content-Length") == "4");
}

TEST_CASE("response::file supports suffix and open-ended ranges", "[file_response][range]") {
    file_fixture fixture{"0123456789"};

    fiasco::app app;
    fiasco_test::get(app, "/file", [path = fixture.path] { return fiasco::response::file(path); });

    fiasco::test_client client{app};
    {
        auto req = make_request("GET", "/file");
        req.headers["Range"] = "bytes=-3";
        const auto res = client.handle(std::move(req));
        REQUIRE(res.status_code == 206);
        CHECK(res.body == "789");
        CHECK(res.headers.at("Content-Range") == "bytes 7-9/10");
    }
    {
        auto req = make_request("GET", "/file");
        req.headers["Range"] = "bytes=7-";
        const auto res = client.handle(std::move(req));
        REQUIRE(res.status_code == 206);
        CHECK(res.body == "789");
        CHECK(res.headers.at("Content-Range") == "bytes 7-9/10");
    }
}

TEST_CASE("response::file rejects unsatisfiable ranges", "[file_response][range]") {
    file_fixture fixture{"0123456789"};

    fiasco::app app;
    fiasco_test::get(app, "/file", [path = fixture.path] { return fiasco::response::file(path); });

    fiasco::test_client client{app};
    auto req = make_request("GET", "/file");
    req.headers["Range"] = "bytes=20-";
    const auto res = client.handle(std::move(req));

    REQUIRE(res.status_code == 416);
    CHECK(res.body.empty());
    CHECK(res.headers.at("Content-Range") == "bytes */10");
    CHECK(res.headers.at("Content-Length") == "0");
}

TEST_CASE("response::file ignores a range when If-Range does not match", "[file_response][range]") {
    file_fixture fixture{"0123456789"};

    fiasco::app app;
    fiasco_test::get(app, "/file", [path = fixture.path] { return fiasco::response::file(path); });

    fiasco::test_client client{app};
    auto req = make_request("GET", "/file");
    req.headers["Range"] = "bytes=2-5";
    req.headers["If-Range"] = "\"not-the-etag\"";
    const auto res = client.handle(std::move(req));

    REQUIRE(res.status_code == 200);
    CHECK(res.body == "0123456789");
    CHECK(res.headers.at("Content-Length") == "10");
}

TEST_CASE("server streams file ranges", "[file_response][integration]") {
    file_fixture fixture{"0123456789"};

    fiasco_test::test_server server;
    fiasco_test::get(
        server.app(), "/file", [path = fixture.path] { return fiasco::response::file(path); });
    server.start();

    const auto raw = fiasco_test::send_request(server.port(),
                                               "GET /file HTTP/1.1\r\n"
                                               "Host: localhost\r\n"
                                               "Range: bytes=2-5\r\n"
                                               "Connection: close\r\n"
                                               "\r\n");

    CHECK(fiasco_test::extract_status(raw) == 206);
    CHECK(fiasco_test::extract_body(raw) == "2345");
    CHECK(raw.find("Content-Range: bytes 2-5/10\r\n") != std::string::npos);
    CHECK(raw.find("Content-Length: 4\r\n") != std::string::npos);
}

TEST_CASE("response::file supports HEAD and conditional requests", "[file_response]") {
    file_fixture fixture{"0123456789"};

    fiasco::app app;
    app.mount_static("/files", fixture.path.parent_path());

    const std::string url = "/files/" + fixture.path.filename().string();
    fiasco::test_client client{app};
    const auto head = client.handle(make_request("HEAD", url));
    REQUIRE(head.status_code == 200);
    CHECK(head.body.empty());
    CHECK(head.headers.at("Content-Length") == "10");

    const auto full = client.handle(make_request("GET", url));
    REQUIRE(full.status_code == 200);
    const auto etag = full.headers.at("ETag");

    auto conditional = make_request("GET", url);
    conditional.headers["If-None-Match"] = etag;
    const auto not_modified = client.handle(std::move(conditional));
    CHECK(not_modified.status_code == 304);
    CHECK(not_modified.body.empty());
}
