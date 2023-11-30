cmake_minimum_required(VERSION 3.22)

project(path_tracer)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${CMAKE_PROJECT_NAME} src/main.cpp)

add_subdirectory(third_party/glfw)
add_subdirectory(third_party/json)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src ${CMAKE_SOURCE_DIR}/third_party/json/include)

add_subdirectory(src)