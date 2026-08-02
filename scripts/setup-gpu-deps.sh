#!/usr/bin/env bash
# Fetches the headers and libraries the GPU backend builds against. Needs no root, and no
# CUDA toolkit install.
#
# Almost everything the GPU path needs is already on a machine with an NVIDIA driver:
# libcuda.so.1 (the driver API) and libnvoptix.so.1 (the OptiX implementation, 49 MB) both
# ship with the display driver rather than with any SDK. What is missing is only the
# headers to compile against, plus NVRTC to turn device source into PTX at runtime.
#
# Those come from two places, both public and neither requiring a developer login:
#
#   - NVIDIA's CUDA redistributable archives, which is where cuda.h, nvrtc.h and the
#     NVRTC libraries live. Two components, about 31 MB. The Ubuntu package carrying the
#     same headers, nvidia-cuda-dev, is a 668 MB download that unpacks to 2.4 GB, and
#     these archives are also distribution independent.
#   - The OptiX headers, published on GitHub. The implementation is already installed, so
#     these are all that is needed.
#
# Deliberately not fetched: nvcc. Device code is compiled at runtime by NVRTC, which is
# how OptiX is conventionally driven anyway, and which sidesteps a real problem here --
# the CUDA 12.0 nvcc supports GCC up to 12, and this project builds with GCC 13.
#
# Usage:
#     scripts/setup-gpu-deps.sh [dest-dir]      # default: <repo>/third_party/gpu
#
# Then configure with -DPT_ENABLE_GPU=ON, which looks in the default location on its own.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${1:-$REPO_ROOT/third_party/gpu}"

# Pinned deliberately. These are the versions the feasibility work was done against: NVRTC
# 12.0 compiling for compute capability 8.6, loaded by a 13.x driver, which works because
# PTX is forward compatible. Moving them is fine, but should be a decision rather than a
# side effect of running this on a different day.
CUDA_CUDART_VERSION="12.0.146"
CUDA_NVRTC_VERSION="12.0.140"
OPTIX_TAG="v9.1.0"

REDIST="https://developer.download.nvidia.com/compute/cuda/redist"
OPTIX_ARCHIVE="https://github.com/NVIDIA/optix-dev/archive/refs/tags/${OPTIX_TAG}.tar.gz"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

for tool in curl tar; do
    command -v "$tool" >/dev/null || { echo "error: $tool is required" >&2; exit 1; }
done

# Fetches one CUDA redistributable component and unpacks it into $WORK.
fetch_cuda_component() {
    local name="$1" version="$2"
    local archive="${name}-linux-x86_64-${version}-archive"

    # Progress goes to stderr because this function's stdout is the unpacked path.
    echo "  ${name} ${version}" >&2
    curl -fsSL --retry 3 -o "$WORK/${name}.tar.xz" \
        "${REDIST}/${name}/linux-x86_64/${archive}.tar.xz"
    tar -xf "$WORK/${name}.tar.xz" -C "$WORK"

    echo "$WORK/$archive"
}

echo "Fetching CUDA headers and NVRTC..."
CUDART_DIR="$(fetch_cuda_component cuda_cudart "$CUDA_CUDART_VERSION")"
NVRTC_DIR="$(fetch_cuda_component cuda_nvrtc "$CUDA_NVRTC_VERSION")"

echo "Fetching OptiX ${OPTIX_TAG} headers..."
curl -fsSL --retry 3 -o "$WORK/optix.tar.gz" "$OPTIX_ARCHIVE"
tar -xf "$WORK/optix.tar.gz" -C "$WORK"
OPTIX_DIR="$WORK/optix-dev-${OPTIX_TAG#v}"

[ -d "$OPTIX_DIR/include" ] || { echo "error: unexpected OptiX archive layout" >&2; exit 1; }

echo "Installing into $DEST ..."
rm -rf "$DEST"
mkdir -p "$DEST/include" "$DEST/lib/stubs"

# Only what is actually compiled against. The cudart component carries the whole runtime
# API as well, and none of it is used: this project talks to the driver API directly.
cp "$CUDART_DIR/include/cuda.h" "$DEST/include/"
cp "$NVRTC_DIR/include/nvrtc.h" "$DEST/include/"
cp -r "$OPTIX_DIR/include/." "$DEST/include/"

# The stub exists to link against. It exports the driver API entry points and nothing
# else, and carries the same soname as the real library, so the loader resolves calls
# against the installed driver at run time rather than against this file.
cp "$CUDART_DIR/lib/stubs/libcuda.so" "$DEST/lib/stubs/"

# NVRTC, unlike the driver, is not installed system-wide, so the real libraries are
# needed and not just a stub. The builtins library is loaded by NVRTC itself at compile
# time and is easy to forget until a compilation fails with no obvious cause.
cp -P "$NVRTC_DIR/lib"/libnvrtc.so* "$DEST/lib/"
cp -P "$NVRTC_DIR/lib"/libnvrtc-builtins.so* "$DEST/lib/"

cp "$CUDART_DIR/LICENSE" "$DEST/LICENSE.cuda"
cp "$OPTIX_DIR/LICENSE.txt" "$DEST/LICENSE.optix"

cat > "$DEST/VERSIONS" <<EOF
cuda_cudart  $CUDA_CUDART_VERSION   (cuda.h, libcuda.so link stub)
cuda_nvrtc   $CUDA_NVRTC_VERSION    (nvrtc.h, libnvrtc, libnvrtc-builtins)
optix-dev    $OPTIX_TAG             (headers only; the implementation ships with the driver)

Fetched by scripts/setup-gpu-deps.sh. Not checked in.
EOF

echo "  $(du -sh "$DEST" | cut -f1) installed"

if [ ! -e /usr/lib/x86_64-linux-gnu/libnvoptix.so.1 ] && ! ldconfig -p 2>/dev/null | grep -q libnvoptix; then
    echo
    echo "warning: libnvoptix.so.1 was not found. It ships with the NVIDIA display driver," >&2
    echo "         so the headers are now here but there is nothing to run them against." >&2
fi

cat <<EOF

Done. Configure the build with:

    cmake -B build -DCMAKE_BUILD_TYPE=Release -DPT_ENABLE_GPU=ON

Then check what the device reports:

    ./build/src/steradian --gpu-info

EOF
