#!/usr/bin/env bash
# Formats and lints the project's own sources.
#
# Runs from anywhere: paths are resolved relative to this script, not to the caller's
# working directory. Vendored third-party headers under src/Utils (stb_image*.h) are
# excluded so they are not reformatted or auto-fixed.
#
#   scripts/format.sh              format in place, and lint if a build directory exists
#   scripts/format.sh --check      report what is unformatted and change nothing
#
# The --check mode exists so that continuous integration and this script cannot disagree
# about which files are covered. They did: this walked src/ while CI walked src/ and
# tests/, so formatting everything locally, committing, and then failing on a test file
# was a perfectly ordinary thing to do.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CHECK=0
BUILD_DIR="$REPO_ROOT/build"

while [ $# -gt 0 ]; do
    case "$1" in
        --check)   CHECK=1 ;;
        -h|--help) sed -n '2,14p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'; exit 0 ;;
        *)         BUILD_DIR="$1" ;;
    esac
    shift
done

if ! command -v clang-format >/dev/null; then
    echo "error: clang-format is not installed, or is not on PATH." >&2
    echo "       Debian/Ubuntu: sudo apt install clang-format" >&2
    exit 1
fi

# Both directories, matching what CI checks. src/ alone is not enough: the tests are as
# much this project's code as the renderer is, and they are where the mismatch bit.
mapfile -t FILES < <(
    find "$REPO_ROOT/src" "$REPO_ROOT/tests" \( -name '*.h' -o -name '*.cpp' -o -name '*.cu' \) \
        -not -name 'stb_image.h' \
        -not -name 'stb_image_write.h' \
        | sort
)

if [ ${#FILES[@]} -eq 0 ]; then
    echo "No sources found under $REPO_ROOT/src or $REPO_ROOT/tests" >&2
    exit 1
fi

if [ "$CHECK" -eq 1 ]; then
    echo "Checking ${#FILES[@]} files with $(clang-format --version)"
    clang-format --dry-run --Werror "${FILES[@]}"
    echo "All formatted."
    exit 0
fi

echo "Formatting ${#FILES[@]} files with $(clang-format --version)"
clang-format -i "${FILES[@]}"

# clang-tidy needs a compilation database, which CMake writes into the build directory.
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "Skipping clang-tidy: no compile_commands.json in $BUILD_DIR" >&2
    echo "Configure first, e.g. cmake -B build" >&2
    exit 0
fi

if ! command -v clang-tidy >/dev/null; then
    echo "Skipping clang-tidy: it is not installed, or is not on PATH." >&2
    exit 0
fi

echo "Linting with clang-tidy (build dir: $BUILD_DIR)..."
clang-tidy -p="$BUILD_DIR" --fix-errors --fix-notes "${FILES[@]}"
