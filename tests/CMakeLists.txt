# === Catch2 is fetched by the root CMakeLists.txt ===
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# ---- json_test ----
add_executable(fiasco_test_json json_test.cpp)
target_link_libraries(fiasco_test_json PRIVATE Catch2::Catch2WithMain fiasco_types)
catch_discover_tests(fiasco_test_json)

# ---- response_test ----
add_executable(fiasco_test_response response_test.cpp)
target_link_libraries(fiasco_test_response PRIVATE Catch2::Catch2WithMain fiasco_http)
catch_discover_tests(fiasco_test_response)

# ---- request_test ----
add_executable(fiasco_test_request request_test.cpp)
target_link_libraries(fiasco_test_request PRIVATE Catch2::Catch2WithMain fiasco_http)
catch_discover_tests(fiasco_test_request)

# ---- parser_test ----
add_executable(fiasco_test_parser parser_test.cpp)
target_link_libraries(fiasco_test_parser PRIVATE Catch2::Catch2WithMain fiasco_http)
catch_discover_tests(fiasco_test_parser)

# ---- pathing_test ----
add_executable(fiasco_test_pathing pathing_test.cpp)
target_link_libraries(fiasco_test_pathing PRIVATE Catch2::Catch2WithMain fiasco_routing)
catch_discover_tests(fiasco_test_pathing)

# ---- route_table_test ----
add_executable(fiasco_test_route_table route_table_test.cpp)
target_link_libraries(fiasco_test_route_table PRIVATE Catch2::Catch2WithMain fiasco_routing)
catch_discover_tests(fiasco_test_route_table)

# ---- router_test ----
add_executable(fiasco_test_router router_test.cpp)
target_link_libraries(fiasco_test_router PRIVATE Catch2::Catch2WithMain fiasco_routing)
catch_discover_tests(fiasco_test_router)

# ---- function_traits_test ----
add_executable(fiasco_test_function_traits function_traits_test.cpp)
target_link_libraries(fiasco_test_function_traits PRIVATE Catch2::Catch2WithMain fiasco_routing)
catch_discover_tests(fiasco_test_function_traits)

# ---- meta_reflect_test ----
add_executable(fiasco_test_meta_reflect meta_reflect_test.cpp)
target_link_libraries(fiasco_test_meta_reflect PRIVATE Catch2::Catch2WithMain fiasco_meta)
catch_discover_tests(fiasco_test_meta_reflect)

# ---- model_reflect_test ----
add_executable(fiasco_test_model_reflect model_reflect_test.cpp)
target_link_libraries(fiasco_test_model_reflect PRIVATE Catch2::Catch2WithMain fiasco_types)
catch_discover_tests(fiasco_test_model_reflect)

# ---- model_serialization_test ----
add_executable(fiasco_test_model_serialization model_serialization_test.cpp)
target_link_libraries(fiasco_test_model_serialization PRIVATE Catch2::Catch2WithMain fiasco_types)
catch_discover_tests(fiasco_test_model_serialization)

# ---- field_annotation_test ----
add_executable(fiasco_test_field_annotation field_annotation_test.cpp)
target_link_libraries(fiasco_test_field_annotation PRIVATE Catch2::Catch2WithMain fiasco_types)
catch_discover_tests(fiasco_test_field_annotation)

# ---- server_test ----
add_executable(fiasco_test_server server_test.cpp)
target_link_libraries(fiasco_test_server PRIVATE Catch2::Catch2WithMain fiasco)
catch_discover_tests(fiasco_test_server)

# ---- http_boundary_test ----
add_executable(fiasco_test_http_boundary http_boundary_test.cpp)
target_link_libraries(fiasco_test_http_boundary PRIVATE Catch2::Catch2WithMain fiasco)
catch_discover_tests(fiasco_test_http_boundary)

# ---- integration_test ----
add_executable(fiasco_test_integration integration_test.cpp)
target_link_libraries(fiasco_test_integration PRIVATE Catch2::Catch2WithMain fiasco)
catch_discover_tests(fiasco_test_integration)

# ---- logging_test ----
add_executable(fiasco_test_logging logging_test.cpp)
target_link_libraries(fiasco_test_logging PRIVATE Catch2::Catch2WithMain fiasco_logging)
target_link_libraries(fiasco_test_logging PRIVATE asio)
catch_discover_tests(fiasco_test_logging)

# ---- logging_integration_test ----
add_executable(fiasco_test_logging_integration logging_integration_test.cpp)
target_link_libraries(fiasco_test_logging_integration PRIVATE Catch2::Catch2WithMain fiasco)
catch_discover_tests(fiasco_test_logging_integration)

# ---- error_handling_test ----
add_executable(fiasco_test_error_handling error_handling_test.cpp)
target_link_libraries(fiasco_test_error_handling PRIVATE Catch2::Catch2WithMain fiasco)
catch_discover_tests(fiasco_test_error_handling)
