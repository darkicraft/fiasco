# Tests are grouped by the layer or feature they exercise. Keep support helpers
# shared under tests/support rather than duplicating fixtures per directory.

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

function(fiasco_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain ${ARGN})
    catch_discover_tests(${name})
endfunction()

# ---- contract ----
fiasco_add_test(fiasco_test_contract contract/contract_test.cpp fiasco_contract)

# ---- types and meta ----
fiasco_add_test(fiasco_test_json types/json_test.cpp fiasco_types)
fiasco_add_test(fiasco_test_json_schema types/json_schema_test.cpp fiasco_app)
fiasco_add_test(fiasco_test_model_reflect types/model_reflect_test.cpp fiasco_types)
fiasco_add_test(fiasco_test_model_serialization types/model_serialization_test.cpp fiasco_types)
fiasco_add_test(fiasco_test_field_annotation types/field_annotation_test.cpp fiasco_types)
fiasco_add_test(fiasco_test_meta_reflect meta/meta_reflect_test.cpp fiasco_meta)

# ---- http and transport ----
fiasco_add_test(fiasco_test_response http/response_test.cpp fiasco_http)
fiasco_add_test(fiasco_test_request http/request_test.cpp fiasco_http)
fiasco_add_test(fiasco_test_parser http/parser_test.cpp fiasco_http)
fiasco_add_test(fiasco_test_async http/async_test.cpp fiasco)
fiasco_add_test(fiasco_test_server http/server_test.cpp fiasco)
fiasco_add_test(fiasco_test_http_boundary http/http_boundary_test.cpp fiasco)

# ---- routing ----
fiasco_add_test(fiasco_test_pathing routing/pathing_test.cpp fiasco_routing)
fiasco_add_test(fiasco_test_route_table routing/route_table_test.cpp fiasco_routing)
fiasco_add_test(fiasco_test_router routing/router_test.cpp fiasco_routing)
fiasco_add_test(fiasco_test_function_traits routing/function_traits_test.cpp fiasco_routing)
fiasco_add_test(fiasco_test_descriptor routing/descriptor_test.cpp fiasco_app)
fiasco_add_test(fiasco_test_annotation_routes routing/annotation_route_test.cpp fiasco_app)
fiasco_add_test(fiasco_test_routing_behavior routing/routing_behavior_test.cpp fiasco_app)

# ---- application ----
fiasco_add_test(fiasco_test_cors app/cors_test.cpp fiasco)
fiasco_add_test(fiasco_test_middleware app/middleware_test.cpp fiasco)
fiasco_add_test(fiasco_test_error_handling app/error_handling_test.cpp fiasco)
fiasco_add_test(fiasco_test_test_client app/test_client_test.cpp fiasco_app)
fiasco_add_test(fiasco_test_openapi app/openapi_test.cpp fiasco_app)
fiasco_add_test(fiasco_test_docs app/docs_test.cpp fiasco_app)
fiasco_add_test(fiasco_test_cookies app/cookie_test.cpp fiasco_app)
fiasco_add_test(fiasco_test_static_files app/static_files_test.cpp fiasco_app)
fiasco_add_test(fiasco_test_file_response app/file_response_test.cpp fiasco)
fiasco_add_test(fiasco_test_stream app/stream_test.cpp fiasco)
fiasco_add_test(fiasco_test_stream_helpers app/stream_helpers_test.cpp fiasco)

# ---- integration ----
fiasco_add_test(fiasco_test_integration integration/integration_test.cpp fiasco)
