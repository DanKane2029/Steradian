cmake_minimum_required(VERSION 3.22)

project(steradian LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Renderer speed depends almost entirely on inlining the small by-value Vec3 operators,
# so an unoptimized build is not merely slower, it is unusable. Default to an optimized
# build when the user has not chosen one.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 Debug Release RelWithDebInfo MinSizeRel)
endif()

# The interactive viewer needs GLFW plus OpenGL/X11 development headers. The renderer
# core deliberately does not, so it can be built and tested headless (and in CI).
option(PT_BUILD_VIEWER "Build the interactive GLFW viewer" ON)

# Off by default: -march=native changes floating point results between machines, which
# would make golden-image comparisons non-portable. Opt in for benchmarking.
option(PT_NATIVE_ARCH "Optimize for the host CPU (-march=native)" OFF)

# Ray, node-visit and primitive-test counters. These are what make acceleration
# structure work measurable, and cost one thread-local increment each.
option(PT_ENABLE_STATS "Collect render statistics" ON)

option(PT_BUILD_TESTS "Build the golden-image regression tests" ON)

# Common warning flags, applied privately so third-party code is unaffected.
add_library(pt_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pt_warnings INTERFACE -Wall -Wextra)
    if(PT_NATIVE_ARCH)
        target_compile_options(pt_warnings INTERFACE -march=native)
    endif()
endif()

add_subdirectory(third_party/json)

# Decide whether the viewer can actually be built here rather than failing the whole
# configure. A missing GLFW submodule or missing OpenGL headers downgrades to a
# headless-only build instead of a hard error.
set(PT_VIEWER_ENABLED OFF)
if(PT_BUILD_VIEWER)
    find_package(OpenGL)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/CMakeLists.txt")
        message(WARNING
            "PT_BUILD_VIEWER is ON but third_party/glfw is empty. "
            "Run: git submodule update --init third_party/glfw. "
            "Building headless-only.")
    elseif(NOT OpenGL_FOUND)
        message(WARNING
            "PT_BUILD_VIEWER is ON but OpenGL development files were not found. "
            "Install them (e.g. libgl1-mesa-dev, xorg-dev) to build the viewer. "
            "Building headless-only.")
    else()
        set(PT_VIEWER_ENABLED ON)
        add_subdirectory(third_party/glfw)
    endif()
endif()

add_subdirectory(src)

if(PT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "steradian: build type  = ${CMAKE_BUILD_TYPE}")
message(STATUS "steradian: viewer      = ${PT_VIEWER_ENABLED}")
message(STATUS "steradian: native arch = ${PT_NATIVE_ARCH}")
message(STATUS "steradian: stats       = ${PT_ENABLE_STATS}")
message(STATUS "steradian: tests       = ${PT_BUILD_TESTS}")
