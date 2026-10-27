add_executable(main main.cpp)
target_link_libraries(main PRIVATE fiasco)

add_executable(hello hello.cpp)
target_link_libraries(hello PRIVATE fiasco)

add_executable(dependencies dependencies.cpp)
target_link_libraries(dependencies PRIVATE fiasco)

if(FIASCO_BUILD_MODULES)
  add_executable(module module.cpp)
  target_link_libraries(module PRIVATE fiasco_modules)
endif()
