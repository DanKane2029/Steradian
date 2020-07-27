cmake_minimum_required(VERSION 3.0)

project(Path_Tracer)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(path_tracer "src/main.cpp")
target_link_libraries(path_tracer ${CONAN_LIBS})