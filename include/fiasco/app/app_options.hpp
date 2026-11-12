#pragma once

#include <string>
#include <vector>

#include "fiasco/internal/types/json.hpp"

namespace fiasco {

struct contact_info {
    std::string name;
    std::string url;
    std::string email;
};

struct license_info {
    std::string name;
    std::string url;
};

/// API metadata for the application. Kept separate from `server_config`:
/// this is the document, not the transport.
struct app_options {
    std::string title = "Fiasco";
    std::string version = "0.1.0";
    std::string description;
    std::string summary;
    std::string terms_of_service;
    contact_info contact{};
    license_info license{};
    std::string openapi_url = "/openapi.json";
    std::string docs_url = "/docs";
    std::string redoc_url = "/redoc";
    std::vector<std::string> openapi_tags{};

    // Swagger UI / ReDoc CDN assets. Set these to self-hosted URLs to avoid
    // the CDN.
    std::string swagger_ui_js_url =
        "https://cdn.jsdelivr.net/npm/swagger-ui-dist@5/swagger-ui-bundle.js";
    std::string swagger_ui_css_url =
        "https://cdn.jsdelivr.net/npm/swagger-ui-dist@5/swagger-ui.css";
    std::string swagger_ui_favicon_url;
    std::string redoc_js_url =
        "https://cdn.jsdelivr.net/npm/redoc@2/bundles/redoc.standalone.js";
    std::string redoc_favicon_url;
    bool redoc_with_google_fonts = true;

    /// Additional Swagger UI config, merged over FastAPI's defaults.
    detail::json_type swagger_ui_parameters = detail::json_type::object();
    /// Optional Swagger UI OAuth2 initialization config.
    detail::json_type swagger_ui_init_oauth = detail::json_type::object();
};

}  // namespace fiasco
