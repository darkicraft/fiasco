#pragma once

/// @file
/// @brief Application-level API metadata and documentation options.

#include <string>
#include <vector>

#include "fiasco/types/json.hpp"

namespace fiasco {

/// Contact information shown in the OpenAPI document.
struct contact_info {
    /// Contact name.
    std::string name;
    /// Contact URL.
    std::string url;
    /// Contact email address.
    std::string email;
};

/// License information shown in the OpenAPI document.
struct license_info {
    /// License name, for example `MIT`.
    std::string name;
    /// License URL.
    std::string url;
};

/// API metadata for the application. Kept separate from `server_config`:
/// this is the document, not the transport.
struct app_options {
    /// Application title shown in the OpenAPI document and docs pages.
    std::string title = "Fiasco";
    /// Application version shown in the OpenAPI document.
    std::string version = "0.1.0";
    /// Long description used by the API document.
    std::string description;
    /// Short summary used by the API document.
    std::string summary;
    /// Terms-of-service URL or text.
    std::string terms_of_service;
    /// Contact information for the API document.
    contact_info contact{};
    /// License information for the API document.
    license_info license{};
    /// Path of the generated OpenAPI document; empty disables the route.
    std::string openapi_url = "/openapi.json";
    /// Path of the Swagger UI page; empty disables it.
    std::string docs_url = "/docs";
    /// Path of the ReDoc page; empty disables it.
    std::string redoc_url = "/redoc";
    /// Optional tag metadata included in the OpenAPI document.
    std::vector<std::string> openapi_tags{};

    /// Swagger UI JavaScript asset URL.
    std::string swagger_ui_js_url =
        "https://cdn.jsdelivr.net/npm/swagger-ui-dist@5/swagger-ui-bundle.js";
    /// Swagger UI stylesheet asset URL.
    std::string swagger_ui_css_url =
        "https://cdn.jsdelivr.net/npm/swagger-ui-dist@5/swagger-ui.css";
    /// Optional Swagger UI favicon URL; empty uses the browser default.
    std::string swagger_ui_favicon_url;
    /// ReDoc JavaScript asset URL.
    std::string redoc_js_url = "https://cdn.jsdelivr.net/npm/redoc@2/bundles/redoc.standalone.js";
    /// Optional ReDoc favicon URL; empty uses the browser default.
    std::string redoc_favicon_url;
    /// Load Google Fonts in the ReDoc page.
    bool redoc_with_google_fonts = true;

    /// Additional Swagger UI config, merged over FastAPI's defaults.
    detail::json_type swagger_ui_parameters = detail::json_type::object();
    /// Optional Swagger UI OAuth2 initialization config.
    detail::json_type swagger_ui_init_oauth = detail::json_type::object();

    /// Whether requests without a trailing slash may redirect to the matching
    /// route with one. Application-level behavior, like FastAPI's argument.
    bool redirect_slashes = true;
};

}  // namespace fiasco
