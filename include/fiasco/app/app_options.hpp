#pragma once

#include <string>
#include <vector>

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
};

}  // namespace fiasco
