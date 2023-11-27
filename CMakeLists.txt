cmake_minimum_required(VERSION 3.22)

project(path_tracer)

add_executable(${CMAKE_PROJECT_NAME} src/main.cpp)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)

add_subdirectory(third_party/glfw)

add_subdirectory(src)