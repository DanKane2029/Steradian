set(LIBRARY_NAME WINDOW)

set(HEADERS PixelBuffer.h Window.h)
set(SOURCES PixelBuffer.cpp Window.cpp)

add_library(${LIBRARY_NAME} STATIC ${SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src/${LIBRARY_NAME} ${CMAKE_SOURCE_DIR}/src)
find_package(OpenGL REQUIRED)
target_link_libraries(${LIBRARY_NAME} glfw OpenGL::GL)

target_link_libraries(${CMAKE_PROJECT_NAME} ${LIBRARY_NAME})
