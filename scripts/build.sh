#!/usr/bin/env bash
# Configures and builds the project.
#
# Exists mainly so the viewer's dependency paths do not have to be remembered. When the
# OpenGL and X11 development files were installed without root, CMake has to be pointed at
# them explicitly, and the two arguments that do it are long and easy to leave out. This
# script looks for that sysroot and adds them when it exists.
#
#   scripts/build.sh                 configure and build
#   scripts/build.sh --test          ...and run the test suite
#   scripts/build.sh --clean --test  ...from scratch
#   scripts/build.sh --debug         a debug build, in build-debug
#   scripts/build.sh --no-viewer     headless only, in build-headless
#   scripts/build.sh --gpu           with GPU support, in build-gpu
#
# Runs from anywhere: paths are resolved relative to this script.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_TYPE="Release"
BUILD_DIR=""
RUN_TESTS=0
CLEAN=0
VIEWER=1
GPU=0
JOBS="$(nproc 2>/dev/null || echo 4)"

while [ $# -gt 0 ]; do
    case "$1" in
        --clean)      CLEAN=1 ;;
        --test|--tests) RUN_TESTS=1 ;;
        --debug)      BUILD_TYPE="Debug" ;;
        --release)    BUILD_TYPE="Release" ;;
        --no-viewer)  VIEWER=0 ;;
        --gpu)        GPU=1 ;;
        --jobs)       shift; JOBS="$1" ;;
        --dir)        shift; BUILD_DIR="$1" ;;
        -h|--help)    sed -n '2,16p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'; exit 0 ;;
        *)            echo "Unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

# Separate directories per configuration, so switching between them does not force a
# full rebuild each time.
if [ -z "$BUILD_DIR" ]; then
    # Composed from every option that affects the result, not overwritten by each in turn.
    # Letting one option win would put, say, a debug headless build in the same directory
    # as a release headless one, so switching between them would silently replace the
    # other and force a full rebuild every time.
    BUILD_DIR="build"
    [ "$BUILD_TYPE" = "Debug" ] && BUILD_DIR="${BUILD_DIR}-debug"
    [ "$VIEWER" -eq 0 ] && BUILD_DIR="${BUILD_DIR}-headless"
    [ "$GPU" -eq 1 ] && BUILD_DIR="${BUILD_DIR}-gpu"
fi

# The renderer will not build at all without the JSON headers, and a clone made without
# --recurse-submodules is a common way to arrive here.
if [ ! -f third_party/json/include/nlohmann/json.hpp ]; then
    echo "Fetching submodules..."
    git submodule update --init third_party/json
fi

if [ "$VIEWER" -eq 1 ] && [ ! -f third_party/glfw/CMakeLists.txt ]; then
    echo "Fetching GLFW for the viewer..."
    git submodule update --init third_party/glfw
fi

CMAKE_ARGS=(-B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE")

if [ "$VIEWER" -eq 0 ]; then
    CMAKE_ARGS+=(-DPT_BUILD_VIEWER=OFF)
else
    # Development files installed by scripts/setup-deps.sh rather than by a package
    # manager live here, and CMake will not find them without being told.
    SYSROOT="${PT_SYSROOT:-$HOME/.local/sysroot}"
    if [ -d "$SYSROOT/usr/include/GL" ]; then
        echo "Using OpenGL and X11 headers from $SYSROOT"
        CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$SYSROOT/usr")
        CMAKE_ARGS+=(-DCMAKE_LIBRARY_PATH="$SYSROOT/usr/lib/x86_64-linux-gnu")
    fi
fi

if [ "$GPU" -eq 1 ]; then
    # The GPU SDK is fetched rather than tracked, so a first --gpu build on a fresh clone
    # would otherwise configure, warn, and quietly produce a binary without the feature
    # that was just asked for.
    if [ ! -f third_party/gpu/include/cuda.h ]; then
        echo "Fetching the CUDA and OptiX headers..."
        scripts/setup-gpu-deps.sh >/dev/null
    fi

    CMAKE_ARGS+=(-DPT_ENABLE_GPU=ON)
fi

if [ "$CLEAN" -eq 1 ]; then
    echo "Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

echo "Configuring $BUILD_TYPE in $BUILD_DIR"
cmake "${CMAKE_ARGS[@]}" | grep -E "^-- steradian:|^CMake Warning" || true

echo "Building with $JOBS jobs"
cmake --build "$BUILD_DIR" -j"$JOBS"

if [ "$RUN_TESTS" -eq 1 ]; then
    echo
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

echo
echo "Built: $BUILD_DIR/src/steradian"
echo
echo "  $BUILD_DIR/src/steradian --config res/configs/test_config.json \\"
echo "      --scene res/scenes/cornell_box.json --out render.png --samples 128"
