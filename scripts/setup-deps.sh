#!/usr/bin/env bash
# Installs the OpenGL and X11 development files the interactive viewer needs, without
# requiring root.
#
# The normal way to get these is:
#
#     sudo apt install libgl1-mesa-dev xorg-dev
#
# Prefer that if you have sudo. This script exists for machines where you do not: it
# downloads the same -dev packages with `apt-get download` (which needs no privileges),
# unpacks them into a local sysroot, and repoints the development symlinks at the
# runtime libraries that are already installed system-wide.
#
# Usage:
#     scripts/setup-deps.sh [sysroot-dir]      # default: ~/.local/sysroot
#
# Then configure with the printed -DCMAKE_PREFIX_PATH argument.
set -euo pipefail

SYSROOT="${1:-$HOME/.local/sysroot}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

SYSLIB=/usr/lib/x86_64-linux-gnu

PKGS=(
    # OpenGL
    libgl-dev libglx-dev libopengl-dev libglvnd-dev libgl1-mesa-dev mesa-common-dev
    # core X11
    libx11-dev libxau-dev libxdmcp-dev x11proto-dev xtrans-dev
    # X11 extensions GLFW links against
    libxrandr-dev libxrender-dev libxext-dev libxfixes-dev
    libxinerama-dev libxcursor-dev libxi-dev
)

echo "Downloading ${#PKGS[@]} development packages..."
(cd "$WORK" && apt-get download "${PKGS[@]}" >/dev/null)

echo "Unpacking into $SYSROOT ..."
rm -rf "$SYSROOT"
mkdir -p "$SYSROOT"
for deb in "$WORK"/*.deb; do
    dpkg -x "$deb" "$SYSROOT"
done

# A -dev package ships libFoo.so as a relative symlink to the versioned runtime file,
# which lives in the system library directory rather than in this sysroot. Repoint each
# one at the real file so the linker can follow it.
echo "Repointing development symlinks at $SYSLIB ..."
while IFS= read -r link; do
    [ -e "$link" ] && continue

    target=$(readlink "$link")
    if [ -e "$SYSLIB/$target" ]; then
        ln -sf "$SYSLIB/$target" "$link"
    else
        # Fall back to the soname when the fully versioned file is not installed.
        base=$(basename "$link" .so)
        soname=$(ls "$SYSLIB/$base".so.[0-9] 2>/dev/null | head -1)
        if [ -n "$soname" ]; then
            ln -sf "$soname" "$link"
        else
            echo "  warning: could not resolve $(basename "$link") -> $target" >&2
        fi
    fi
done < <(find "$SYSROOT" -name '*.so' -type l)

dangling=$(find "$SYSROOT" -name '*.so' -type l ! -exec test -e {} \; -print | wc -l)
if [ "$dangling" -ne 0 ]; then
    echo "warning: $dangling development symlink(s) still unresolved" >&2
fi

cat <<EOF

Done. Configure the build with:

    cmake -B build -DCMAKE_BUILD_TYPE=Release \\
        -DCMAKE_PREFIX_PATH="$SYSROOT/usr" \\
        -DCMAKE_LIBRARY_PATH="$SYSROOT/usr/lib/x86_64-linux-gnu"

EOF
