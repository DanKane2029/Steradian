cmake_minimum_required(VERSION 3.0)

project(Path_Tracer)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

#Classes
set(WINDOW "src/Window.h" "src/Window.cpp")
set(PIXEL_BUFFER "src/PixelBuffer.h" "src/PixelBuffer.cpp")

set(SOURCES "src/Main.cpp" ${WINDOW} ${PIXEL_BUFFER})

add_executable(path_tracer ${SOURCES})
target_link_libraries(path_tracer ${CONAN_LIBS})