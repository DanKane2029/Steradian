#!/usr/bin/env bash
# Regenerates the golden reference images.
#
# Run this only when a change is *meant* to alter the rendered output, and review the
# resulting image diff in the commit. If the references are refreshed without looking at
# them, the suite stops being a test and becomes a record of whatever the code last did.
#
# Usage: tests/update_golden.sh [build-dir]      # default: build
set -euo pipefail

TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$TESTS_DIR/.." && pwd)"
BUILD_DIR="${1:-$REPO_ROOT/build}"

RENDERER="$BUILD_DIR/src/steradian"
if [ ! -x "$RENDERER" ]; then
    echo "No renderer at $RENDERER -- build first, e.g. cmake --build $BUILD_DIR" >&2
    exit 1
fi

# Keep these in sync with GOLDEN_SCENES in tests/CMakeLists.txt.
SCENES=(single_sphere single_triangle two_spheres test_obj_ball test_man_obj cornell_box)
SAMPLES=64
SEED=1

mkdir -p "$TESTS_DIR/golden"

for scene in "${SCENES[@]}"; do
    echo "Rendering $scene ..."
    "$RENDERER" \
        --config "$TESTS_DIR/test_config.json" \
        --scene "$REPO_ROOT/res/scenes/$scene.json" \
        --out "$TESTS_DIR/golden/$scene.png" \
        --samples "$SAMPLES" \
        --seed "$SEED" \
        --threads 4 \
        > /dev/null
done

echo
echo "Updated $TESTS_DIR/golden. Review the diff before committing:"
echo "    git diff --stat tests/golden"
