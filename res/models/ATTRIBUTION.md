# Model attribution

## Stanford 3D Scanning Repository

`bunny.obj` and `dragon.obj` come from the [Stanford 3D Scanning
Repository](http://graphics.stanford.edu/data/3Dscanrep/), scanned at Stanford University.

- **Stanford Bunny** — scanned 1993–1994 by Greg Turk and Marc Levoy.
- **Stanford Dragon** — scanned 1996 by the Stanford Computer Graphics Laboratory.

Stanford asks that the repository be acknowledged wherever these models are used, and that
they not be redistributed for commercial purposes. They are included here for testing and
demonstration.

### What was changed

Both were published as PLY. They were converted to OBJ so this renderer's loader can read
them, and per-vertex normals were computed during that conversion, since the originals
carry none and the meshes would otherwise be flat shaded.

The dragon is the `res2` decimation, 202,520 triangles rather than the full 871,414. The
full model is a 33 MB file, which would weigh more than everything else in this repository
combined; the reduced version is visually indistinguishable at the resolutions used here
while still being by a wide margin the heaviest thing the acceleration structure is asked
to organize.

| File | Triangles | Source |
| --- | --- | --- |
| `bunny.obj` | 69,451 | `bunny/reconstruction/bun_zipper.ply` |
| `dragon.obj` | 202,520 | `dragon_recon/dragon_vrip_res2.ply` |

`scripts/fetch-stanford-models.sh` reproduces both from the originals, and can fetch the
full-resolution dragon for anyone who wants a heavier test.

## Other models

The remaining files in this directory predate this work and arrived with the project; their
provenance is not recorded.
