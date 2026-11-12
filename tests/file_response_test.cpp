#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "fiasco/fiasco.hpp"

namespace {

struct file_fixture {
    std::filesystem::path path;

    explicit file_fixture(std::string_view contents)
        : path(std::filesystem::temp_directory_path() / "fiasco-file-response.txt") {
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

TEST_CASE("response::file reads a file and infers its media type", "[file_response]") {
    file_fixture fixture{"hello from a file"};

    const auto res = fiasco::response::file(fixture.path);
    CHECK(res.status_code == 200);
    CHECK(res.body == "hello from a file");
    CHECK(res.headers.at("Content-Type") == "text/plain; charset=utf-8");
    CHECK_FALSE(res.headers.contains("Content-Disposition"));
}

TEST_CASE("response::file sets Content-Disposition for downloads", "[file_response]") {
    file_fixture fixture{"report"};

    const auto res = fiasco::response::file(
        fixture.path, {.filename = "report.txt"});
    CHECK(res.headers.at("Content-Disposition") ==
          "attachment; filename=\"report.txt\"");
}

TEST_CASE("response::file supports inline and media type overrides", "[file_response]") {
    file_fixture fixture{"report"};

    const auto res = fiasco::response::file(
        fixture.path,
        {.filename = "report.txt",
         .media_type = "application/custom",
         .content_disposition_type = "inline"});
    CHECK(res.headers.at("Content-Type") == "application/custom");
    CHECK(res.headers.at("Content-Disposition") ==
          "inline; filename=\"report.txt\"");
}

TEST_CASE("response::file escapes quoted filenames", "[file_response]") {
    file_fixture fixture{"report"};

    const auto res = fiasco::response::file(
        fixture.path, {.filename = "a\"b.txt"});
    CHECK(res.headers.at("Content-Disposition") ==
          "attachment; filename=\"a\\\"b.txt\"");
}

TEST_CASE("response::file rejects missing files", "[file_response]") {
    REQUIRE_THROWS_AS(
        fiasco::response::file(std::filesystem::temp_directory_path() / "missing-fiasco-file"),
        std::runtime_error);
}

TEST_CASE("response::file works through the application seam", "[file_response]") {
    file_fixture fixture{"download me"};

    fiasco::app app;
    app.get("/download", [path = fixture.path] {
        return fiasco::response::file(path, {.filename = "download.txt"});
    });

    fiasco::test_client client{app};
    const auto res = client.handle(make_request("GET", "/download"));

    REQUIRE(res.status_code == 200);
    CHECK(res.body == "download me");
    CHECK(res.headers.at("Content-Disposition") ==
          "attachment; filename=\"download.txt\"");
}
