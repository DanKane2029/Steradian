#!/usr/bin/env bash
# Formats and lints the project's own sources.
#
# Runs from anywhere: paths are resolved relative to this script, not to the caller's
# working directory. Vendored third-party headers under src/Utils (stb_image*.h) are
# excluded so they are not reformatted or auto-fixed.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$REPO_ROOT/build}"

mapfile -t FILES < <(
    find "$REPO_ROOT/src" \( -name '*.h' -o -name '*.cpp' -o -name '*.cu' \) \
        -not -name 'stb_image.h' \
        -not -name 'stb_image_write.h' \
        | sort
)

if [ ${#FILES[@]} -eq 0 ]; then
    echo "No sources found under $REPO_ROOT/src" >&2
    exit 1
fi

echo "Formatting ${#FILES[@]} files..."
clang-format -i "${FILES[@]}"

# clang-tidy needs a compilation database, which CMake writes into the build directory.
if [ -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "Linting with clang-tidy (build dir: $BUILD_DIR)..."
    clang-tidy -p="$BUILD_DIR" --fix-errors --fix-notes "${FILES[@]}"
else
    echo "Skipping clang-tidy: no compile_commands.json in $BUILD_DIR" >&2
    echo "Configure first, e.g. cmake -B build" >&2
fi
