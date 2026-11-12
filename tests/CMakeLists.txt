# === Catch2 is fetched by the root CMakeLists.txt ===
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# ---- contract_test ----
add_executable(fiasco_test_contract contract_test.cpp)
target_link_libraries(fiasco_test_contract PRIVATE Catch2::Catch2WithMain fiasco_contract)
catch_discover_tests(fiasco_test_contract)

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

# ---- cors_test ----
add_executable(fiasco_test_cors cors_test.cpp)
target_link_libraries(fiasco_test_cors PRIVATE Catch2::Catch2WithMain fiasco)
catch_discover_tests(fiasco_test_cors)

# ---- middleware_test ----
add_executable(fiasco_test_middleware middleware_test.cpp)
target_link_libraries(fiasco_test_middleware PRIVATE Catch2::Catch2WithMain fiasco)
catch_discover_tests(fiasco_test_middleware)

# ---- async_test ----
add_executable(fiasco_test_async async_test.cpp)
target_link_libraries(fiasco_test_async PRIVATE Catch2::Catch2WithMain fiasco)
catch_discover_tests(fiasco_test_async)

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

# ---- error_handling_test ----
add_executable(fiasco_test_error_handling error_handling_test.cpp)
target_link_libraries(fiasco_test_error_handling PRIVATE Catch2::Catch2WithMain fiasco)
catch_discover_tests(fiasco_test_error_handling)

# ---- test_client_test ----
add_executable(fiasco_test_test_client test_client_test.cpp)
target_link_libraries(fiasco_test_test_client PRIVATE Catch2::Catch2WithMain fiasco_app)
catch_discover_tests(fiasco_test_test_client)

# ---- annotation_route_test ----
add_executable(fiasco_test_annotation_routes annotation_route_test.cpp)
target_link_libraries(fiasco_test_annotation_routes PRIVATE Catch2::Catch2WithMain fiasco_app)
catch_discover_tests(fiasco_test_annotation_routes)

# ---- descriptor_test ----
add_executable(fiasco_test_descriptor descriptor_test.cpp)
target_link_libraries(fiasco_test_descriptor PRIVATE Catch2::Catch2WithMain fiasco_app)
catch_discover_tests(fiasco_test_descriptor)

# ---- openapi_test ----
add_executable(fiasco_test_openapi openapi_test.cpp)
target_link_libraries(fiasco_test_openapi PRIVATE Catch2::Catch2WithMain fiasco_app)
catch_discover_tests(fiasco_test_openapi)

# ---- routing_behavior_test ----
add_executable(fiasco_test_routing_behavior routing_behavior_test.cpp)
target_link_libraries(fiasco_test_routing_behavior PRIVATE Catch2::Catch2WithMain fiasco_app)
catch_discover_tests(fiasco_test_routing_behavior)

# ---- json_schema_test ----
add_executable(fiasco_test_json_schema json_schema_test.cpp)
target_link_libraries(fiasco_test_json_schema PRIVATE Catch2::Catch2WithMain fiasco_app)
catch_discover_tests(fiasco_test_json_schema)
