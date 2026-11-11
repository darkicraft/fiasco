add_executable(fiasco_json_bind json_bind.cpp)
target_link_libraries(fiasco_json_bind PRIVATE fiasco)

add_executable(raw_json_bind raw_json.cpp)
target_link_libraries(raw_json_bind PRIVATE nlohmann_json::nlohmann_json asio)
