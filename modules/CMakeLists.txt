// Parked C++20 module sources. Not referenced by the build while modules are
// disabled.
add_library(fiasco_modules)
target_sources(fiasco_modules
  PUBLIC FILE_SET CXX_MODULES FILES
    fiasco.cppm
    fiasco-common.cppm
    fiasco-logging.cppm
    fiasco-response.cppm
    fiasco-server.cppm
    fiasco-router.cppm
)
target_link_libraries(fiasco_modules PUBLIC fiasco)
