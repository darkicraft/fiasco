#pragma once

/// @file
/// @brief Internal Swagger UI and ReDoc HTML rendering.
/// @internal

// Swagger UI and ReDoc HTML pages. The assets themselves come from the CDN by
// default; the URLs are configurable for self-hosting.

#include <string>
#include <string_view>

#include "fiasco/app/app_options.hpp"
#include "fiasco/types/json.hpp"

namespace fiasco::detail {

inline std::string html_escape(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&#39;";
            break;
        default:
            out.push_back(c);
        }
    }
    return out;
}

/// JSON that is safe to embed inside a <script> element.
inline std::string html_safe_json(const json_type& value) {
    const std::string raw = value.dump();
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        switch (c) {
        case '<':
            out += "\\u003c";
            break;
        case '>':
            out += "\\u003e";
            break;
        case '&':
            out += "\\u0026";
            break;
        default:
            out.push_back(c);
        }
    }
    return out;
}

inline json_type swagger_ui_config(const fiasco::app_options& options) {
    json_type config = json_type::object();
    config["url"] = options.openapi_url;
    config["dom_id"] = "#swagger-ui";
    config["layout"] = "BaseLayout";
    config["deepLinking"] = true;
    config["showExtensions"] = true;
    config["showCommonExtensions"] = true;

    if (options.swagger_ui_parameters.is_object()) {
        for (const auto& key : options.swagger_ui_parameters.object_keys()) {
            config[key] = options.swagger_ui_parameters[key];
        }
    }
    return config;
}

inline std::string swagger_ui_html(const fiasco::app_options& options) {
    std::string html = R"(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<link type="text/css" rel="stylesheet" href=")";
    html += html_escape(options.swagger_ui_css_url);
    html += R"(">
)";
    if (!options.swagger_ui_favicon_url.empty()) {
        html += R"(<link rel="shortcut icon" href=")";
        html += html_escape(options.swagger_ui_favicon_url);
        html += R"(">
)";
    }
    html += "<title>";
    html += html_escape(options.title + " - Swagger UI");
    html += R"(</title>
</head>
<body>
<div id="swagger-ui"></div>
<script src=")";
    html += html_escape(options.swagger_ui_js_url);
    html += R"("></script>
<script>
const ui = SwaggerUIBundle({
)";

    const json_type config = swagger_ui_config(options);
    for (const auto& key : config.object_keys()) {
        html += "    ";
        html += html_safe_json(json_type(key));
        html += ": ";
        html += html_safe_json(config[key]);
        html += ",\n";
    }

    html += R"(    presets: [
        SwaggerUIBundle.presets.apis,
        SwaggerUIBundle.SwaggerUIStandalonePreset
    ]
});
)";

    if (options.swagger_ui_init_oauth.is_object() && options.swagger_ui_init_oauth.size() > 0) {
        html += "ui.initOAuth(";
        html += html_safe_json(options.swagger_ui_init_oauth);
        html += ");\n";
    }

    html += R"(</script>
</body>
</html>)";
    return html;
}

inline std::string redoc_html(const fiasco::app_options& options) {
    std::string html = R"(<!DOCTYPE html>
<html>
<head>
<title>)";
    html += html_escape(options.title + " - ReDoc");
    html += R"(</title>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1">
)";
    if (options.redoc_with_google_fonts) {
        html +=
            R"(<link href="https://fonts.googleapis.com/css?family=Montserrat:300,400,700|Roboto:300,400,700" rel="stylesheet">
)";
    }
    if (!options.redoc_favicon_url.empty()) {
        html += R"(<link rel="shortcut icon" href=")";
        html += html_escape(options.redoc_favicon_url);
        html += R"(">
)";
    }
    html += R"(<style>
  body {
    margin: 0;
    padding: 0;
  }
</style>
</head>
<body>
<noscript>
    ReDoc requires Javascript to function. Please enable it to browse the documentation.
</noscript>
<redoc spec-url=")";
    html += html_escape(options.openapi_url);
    html += R"("></redoc>
<script src=")";
    html += html_escape(options.redoc_js_url);
    html += R"("></script>
</body>
</html>)";
    return html;
}

}  // namespace fiasco::detail
