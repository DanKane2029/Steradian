# Steradian

A from-scratch CPU renderer in C++.

Named for the SI unit of solid angle — integrating incoming radiance over the hemisphere
above a surface is the thing the renderer is being built to do.

> **Status: not yet a path tracer.** Today the renderer is **Whitted-style**: Blinn-Phong
> direct lighting, perfect-mirror reflections and stochastic soft shadows. There is no
> Monte Carlo path integral, no BRDF importance sampling and no global illumination. The
> roadmap below tracks the work to get there, and this note will change when it lands.

## Roadmap

| Stage | Goal | State |
| --- | --- | --- |
| 0 | Build and run reproducibly; headless PNG output; deterministic seeding | done |
| 1 | Golden-image regression tests, CI, BVH/ray instrumentation | done |
| 2 | Correctness pass: camera basis and real FOV, ray epsilons, OBJ triangulation, textures | done |
| 3 | Performance: flat SAH BVH that actually culls, occlusion queries, thread pool | done |
| 4 | Replace the integrator with Monte Carlo path tracing: hemisphere sampling, BSDFs, area lights, tonemapping | planned |

### Known issues being worked through

These are real and measured, not speculative:

- **Shading is Blinn-Phong, not a BSDF.** No energy conservation, no importance sampling, and
  ambient light is added as a flat constant rather than being derived from the scene.
- **The `transparency` and `refraction` material fields are parsed and ignored.** Dielectrics
  come with the integrator work in Stage 4.
- **No tone mapping.** Linear radiance is clamped at 1.0 on output, so highlights hard-clip.

## Building

Requires CMake 3.22+ and a C++20 compiler.

```sh
git submodule update --init
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The build defaults to `RelWithDebInfo` if no build type is given. The renderer leans heavily
on inlining small vector operations, so an unoptimized build is dramatically slower — prefer
`Release` for anything but debugging.

### Optional viewer

The interactive GLFW viewer needs OpenGL and X11 development headers. If they are not
present, CMake prints a warning and builds a **headless-only** binary instead of failing.

```sh
sudo apt install libgl1-mesa-dev xorg-dev
```

Without sudo, `scripts/setup-deps.sh` fetches the same `-dev` packages with
`apt-get download` (no privileges needed), unpacks them into `~/.local/sysroot`, and
points the development symlinks at the runtime libraries already installed system-wide:

```sh
scripts/setup-deps.sh
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$HOME/.local/sysroot/usr" \
    -DCMAKE_LIBRARY_PATH="$HOME/.local/sysroot/usr/lib/x86_64-linux-gnu"
```

To skip the viewer entirely:

```sh
cmake -B build -DPT_BUILD_VIEWER=OFF
```

| Option | Default | Meaning |
| --- | --- | --- |
| `PT_BUILD_VIEWER` | `ON` | Build the interactive GLFW viewer |
| `PT_BUILD_TESTS` | `ON` | Build the golden-image regression tests |
| `PT_ENABLE_STATS` | `ON` | Collect ray/node/primitive counters |
| `PT_NATIVE_ARCH` | `OFF` | Compile with `-march=native`. Off by default because it changes floating point results between machines, which would make golden images non-portable |

## Running

```sh
# interactive viewer
./build/src/steradian --config res/configs/test_config.json \
                        --scene  res/scenes/two_spheres.json

# headless render to a PNG
./build/src/steradian --config res/configs/test_config.json \
                        --scene  res/scenes/two_spheres.json \
                        --out    render.png --samples 64 --seed 1
```

| Flag | Meaning |
| --- | --- |
| `--config <path>` | Config JSON (window size, thread count, shadow rays, recursion depth) |
| `--scene <path>` | Scene JSON (camera, materials, objects, lights) |

| `--out <path>` | Write a PNG. Implies `--headless` |
| `--samples <n>` | Samples per pixel for a headless render (default 16) |
| `--seed <n>` | Base seed (default 1) |
| `--threads <n>` | Worker threads (default: `numThreads` from the config) |
| `--headless` | Render once and exit without opening a window |

A headless render is **deterministic**: the same `--seed` and `--samples` produce a
byte-identical PNG regardless of `--threads`. That is what makes golden-image regression
testing possible.

Asset paths inside a scene file are resolved relative to that scene file, so scenes and
models can be moved or checked out anywhere.

### Scene camera

```jsonc
"camera": {
  "org":    [0, 0, 2.5],   // position
  "lookAt": [0, 0, 0],     // aim point
  "fov":    60,            // optional, vertical field of view in degrees (default 60)
  "up":     [0, 1, 0]      // optional, reference up direction (default +Y)
}
```

Lights use inverse-square falloff, so `intensity` scales with the square of the distance
to the subject: a light 10 units away needs roughly 100× the intensity of one 1 unit away
to look equally bright.

### Statistics

Builds carry ray, node-visit and primitive-test counters (`-DPT_ENABLE_STATS=OFF` to
remove them). They are what make acceleration-structure work measurable — wall-clock time
cannot tell "the BVH culls well" apart from "the machine was idle", but primitive tests
per ray can:

```
rays traced      50048
node visits      635010 (12.7 per ray)
primitive tests  1662720 (33.2 per ray)
```

Primitive tests per ray is the number that matters for acceleration work: it is
independent of machine speed and load, and it goes down only if traversal is genuinely
rejecting geometry.

### Acceleration structure

The renderer previously used a median-split octree that **never tested a bounding box
during traversal** — the test existed but had no callers. It visited every node and every
primitive on every ray, and because primitives were assigned to children by centroid they
were duplicated across children, so it performed *more* intersection tests than a brute
force loop over the scene.

It is now a binned-SAH BVH, flattened into a contiguous array, traversed iteratively,
descending the nearer child first and shrinking the ray's far bound on each hit. Measured
at 160×120, 1 spp:

| Scene | Primitives | Tests/ray before | Tests/ray after | Time before | Time after |
| --- | ---: | ---: | ---: | ---: | ---: |
| `two_spheres` | 2 | 2 | 2 | 0.015 s | 0.002 s |
| `test_obj_ball` | 960 | 1,236 | **33** | 0.90 s | 0.011 s |
| `test_man_obj` | 48,918 | 52,309 | **63** | 101.4 s | **0.019 s** |

The 48,918-triangle scene got **5,400× faster**, doing **830× fewer** intersection tests.
Note the "before" figure of 52,309 tests per ray against 48,918 primitives: the old
structure tested some primitives more than once.

Output is unchanged by this: the same scene rendered before and after is byte-identical.

## Testing

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The suite runs in about two seconds and covers three things:

- **Five golden-image comparisons**, including a 48,918-triangle mesh.
- **A determinism check** rendering the same scene at 1, 3 and 8 threads, requiring
  byte-identical results.
- **Acceleration structure consistency**: for thousands of random rays per scene, the BVH
  must return exactly what a brute-force scan over every primitive returns. Golden images
  prove the final picture is right; this proves the structure itself is not quietly
  dropping or inventing intersections, which is precisely what the octree it replaced did.

Because renders are deterministic, the image tolerances are deliberately tight (max one
channel level, mean 0.02). Those numbers were picked against a measurement rather than
guessed: a deliberate 2% brightness change moves max by 2–5 and mean by 0.027–0.30, so
anything looser silently passes a real regression.

When a change is *meant* to alter output, refresh the references and **look at them**:

```sh
tests/update_golden.sh build
git diff --stat tests/golden
```

Refreshing references without reviewing them turns the suite from a test into a record of
whatever the code last happened to do.

## Layout

| Path | Contents |
| --- | --- |
| `src/RayTracer/` | Ray generation, intersection dispatch and shading |
| `src/Scene/` | Scene description, primitives, materials, lights, acceleration structure |
| `src/Utils/` | Vector math, RNG, config and OBJ parsing, image output |
| `src/Window/` | Pixel buffer (core) and the optional GLFW viewer |
| `res/` | Sample scenes, configs, models and textures |

`RayTracer`, `Scene` and `Utils` include each other cyclically, so they build as a single
`pt_core` library. Only `Window/Window.cpp` depends on GLFW/OpenGL and is split into the
optional `pt_viewer` target, which is what keeps headless builds possible.
