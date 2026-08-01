#!/usr/bin/env bash
# Rebuilds the Stanford models in res/models from the originals.
#
# The committed bunny.obj and dragon.obj were produced by this script, so it is not needed
# to use the project. It exists so the conversion is reproducible rather than something
# that happened once on someone's machine, and so the full-resolution dragon can be
# fetched by anyone who wants a heavier test than the committed one.
#
#   scripts/fetch-stanford-models.sh            # bunny + dragon, as committed
#   scripts/fetch-stanford-models.sh --full     # ...and the 871,414 triangle dragon
#
# See res/models/ATTRIBUTION.md for terms of use.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODELS="$REPO_ROOT/res/models"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

WANT_FULL=0
[ "${1:-}" = "--full" ] && WANT_FULL=1

BASE="http://graphics.stanford.edu/pub/3Dscanrep"

echo "Downloading from the Stanford 3D Scanning Repository..."
curl -# -L -o "$WORK/bunny.tar.gz" "$BASE/bunny.tar.gz"
curl -# -L -o "$WORK/dragon.tar.gz" "$BASE/dragon/dragon_recon.tar.gz"

tar xzf "$WORK/bunny.tar.gz" -C "$WORK"
tar xzf "$WORK/dragon.tar.gz" -C "$WORK"

cat > "$WORK/ply2obj.py" <<'PYTHON'
import sys, math, os

def convert(src, dst, name):
    with open(src) as f:
        counts = {}
        props = []
        current = None
        while True:
            line = f.readline()
            if not line:
                break
            t = line.split()
            if not t:
                continue
            if t[0] == 'element':
                current = t[1]
                counts[current] = int(t[2])
            elif t[0] == 'property' and current == 'vertex':
                props.append(t[-1])
            elif t[0] == 'end_header':
                break

        ix, iy, iz = props.index('x'), props.index('y'), props.index('z')

        verts = []
        for _ in range(counts.get('vertex', 0)):
            t = f.readline().split()
            verts.append((float(t[ix]), float(t[iy]), float(t[iz])))

        faces = []
        for _ in range(counts.get('face', 0)):
            t = f.readline().split()
            idx = [int(x) for x in t[1:1 + int(t[0])]]
            # Fan-triangulate anything with more than three corners.
            for k in range(1, len(idx) - 1):
                faces.append((idx[0], idx[k], idx[k + 1]))

    # Area-weighted vertex normals. The cross product's length is proportional to the
    # triangle's area, so broad faces influence a shared vertex more than slivers do,
    # which is what keeps the shading smooth across an irregular scan.
    normals = [[0.0, 0.0, 0.0] for _ in verts]
    for a, b, c in faces:
        ax, ay, az = verts[a]
        bx, by, bz = verts[b]
        cx, cy, cz = verts[c]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx - ax, cy - ay, cz - az
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        for i in (a, b, c):
            normals[i][0] += nx
            normals[i][1] += ny
            normals[i][2] += nz

    for n in normals:
        length = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2) or 1.0
        n[0] /= length
        n[1] /= length
        n[2] /= length

    with open(dst, 'w') as o:
        o.write(f"# {name}\n")
        o.write("# Stanford 3D Scanning Repository, http://graphics.stanford.edu/data/3Dscanrep/\n")
        o.write(f"# {len(verts)} vertices, {len(faces)} triangles\n")
        o.write("# Converted from PLY; vertex normals computed during conversion.\n")
        for v in verts:
            o.write("v %.6f %.6f %.6f\n" % v)
        for n in normals:
            o.write("vn %.5f %.5f %.5f\n" % tuple(n))
        for a, b, c in faces:
            o.write(f"f {a+1}//{a+1} {b+1}//{b+1} {c+1}//{c+1}\n")

    print(f"  {os.path.basename(dst):22s} {len(verts):>7} vertices {len(faces):>8} triangles"
          f"  {os.path.getsize(dst)/1048576:5.1f} MB")

convert(sys.argv[1], sys.argv[2], sys.argv[3])
PYTHON

echo "Converting..."
python3 "$WORK/ply2obj.py" "$WORK/bunny/reconstruction/bun_zipper.ply" \
    "$MODELS/bunny.obj" "Stanford Bunny"
python3 "$WORK/ply2obj.py" "$WORK/dragon_recon/dragon_vrip_res2.ply" \
    "$MODELS/dragon.obj" "Stanford Dragon"

if [ "$WANT_FULL" -eq 1 ]; then
    python3 "$WORK/ply2obj.py" "$WORK/dragon_recon/dragon_vrip.ply" \
        "$MODELS/dragon_full.obj" "Stanford Dragon (full resolution)"
    echo
    echo "dragon_full.obj is deliberately not committed: it is far larger than the rest of"
    echo "the repository put together. Point a scene at it directly to use it."
fi
