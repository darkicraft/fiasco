cmake_minimum_required(VERSION 3.30)
project(fiasco LANGUAGES CXX)

option(FIASCO_BUILD_MODULES "Build C++ module targets" OFF)
option(FIASCO_BUILD_EXAMPLES "Build examples" ON)
option(FIASCO_BUILD_TESTS "Build tests" ON)

# C++26 reflection (std::meta) is required and, for now, only GCC 16+ provides
# the needed `-freflection` support.
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR
        "fiasco requires GCC for C++26 reflection (-freflection); "
        "found '${CMAKE_CXX_COMPILER_ID}'.")
endif()
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.0)
    message(FATAL_ERROR
        "fiasco requires GCC >= 16 for C++26 reflection (-freflection); "
        "found ${CMAKE_CXX_COMPILER_VERSION}.")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Module scanning is only needed for the opt-in module target; leave it off
# otherwise so ordinary builds don't get -fmodules-ts / module dependency scans.
if(FIASCO_BUILD_MODULES)
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
else()
  set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

# == DEPENDENCIES =============================================================

include(FetchContent)

# -- nlohmann/json ----------------------------------------
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SHALLOW    TRUE
)

# -- llhttp -----------------------------------------------
FetchContent_Declare(
    llhttp
    URL https://github.com/nodejs/llhttp/archive/refs/tags/release/v9.4.3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Force llhttp to build only its static library (llhttp_static).
set(LLHTTP_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LLHTTP_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(json llhttp)

if(FIASCO_BUILD_TESTS)
  FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.16.1
      GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(Catch2)
endif()

# Restore defaults after llhttp
unset(LLHTTP_BUILD_SHARED_LIBS CACHE)
unset(LLHTTP_BUILD_STATIC_LIBS CACHE)

# -- asio -------------------------------------------------
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-38-2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
if (WIN32)
  target_link_libraries(asio INTERFACE ws2_32 mswsock)
  target_compile_definitions(asio INTERFACE _WIN32_WINNT=0x0A00)
endif()

# Reflection lives in fiasco's headers, so every fiasco target (and the
# examples/tests/modules below) is compiled with GCC's reflection flag. Added
# after the dependencies so third-party targets are left untouched.
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-freflection>)

# == FIASCO ===================================================================

# -- fiasco_meta ------------------------------------------
# Header-only reflection foundation (see internal/meta/reflect.hpp).
add_library(fiasco_meta INTERFACE)
target_include_directories(fiasco_meta INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# -- fiasco_types -----------------------------------------
add_library(fiasco_types STATIC
    src/fiasco/internal/types/json.cpp
)

target_include_directories(fiasco_types PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco_types PRIVATE
    nlohmann_json::nlohmann_json
)

# -- fiasco_core ------------------------------------------
add_library(fiasco_core STATIC
    src/fiasco/internal/core/tcp_server.cpp
)

target_include_directories(fiasco_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco_core PUBLIC
    asio
)

# -- fiasco_http ------------------------------------------
add_library(fiasco_http STATIC
    src/fiasco/internal/http/parser.cpp
    src/fiasco/internal/http/request.cpp
    src/fiasco/internal/http/response.cpp
)

target_include_directories(fiasco_http PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco_http PUBLIC
    fiasco_types
    llhttp_static
)

# -- fiasco_routing ------------------------------------------
add_library(fiasco_routing STATIC
    src/fiasco/internal/routing/route_table.cpp
)

target_include_directories(fiasco_routing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco_routing PUBLIC
    fiasco_meta
    fiasco_types
    fiasco_http
)

# -- fiasco_logging ----------------------------------------
add_library(fiasco_logging STATIC
    src/fiasco/internal/logging/access_log.cpp
    src/fiasco/internal/logging/log_formatter.cpp
    src/fiasco/internal/logging/logger.cpp
    src/fiasco/internal/logging/log_registry.cpp
)

target_include_directories(fiasco_logging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# -- fiasco -----------------------------------------------
add_library(fiasco INTERFACE)

target_include_directories(fiasco INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco INTERFACE
    fiasco_meta
    fiasco_types
    fiasco_core
    fiasco_http
    fiasco_routing
    fiasco_logging
)

# == SUBDIRECTORIES ===========================================================

if(FIASCO_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

if(FIASCO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(FIASCO_BUILD_MODULES)
  add_subdirectory(modules)
endif()

# == INSTALLATION =============================================================
install(DIRECTORY include/ DESTINATION include)
