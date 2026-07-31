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
| 1 | Golden-image regression tests, CI, BVH/ray instrumentation | in progress |
| 2 | Correctness pass: camera basis and real FOV, ray epsilons, OBJ triangulation, textures | planned |
| 3 | Performance: flat SAH BVH that actually culls, typed primitive arrays, slim hit record, thread pool | planned |
| 4 | Replace the integrator with Monte Carlo path tracing: hemisphere sampling, BSDFs, area lights, tonemapping | planned |

### Known issues being worked through

These are real and measured, not speculative:

- **The acceleration structure performs no culling.** Traversal descends into every node and
  tests every primitive; the bounding-box test exists but is never called. Render time scales
  *linearly* with primitive count — 960 triangles takes 0.27 s where 24,459 takes 6.23 s at
  the same settings.
- **The OBJ loader drops geometry.** Fan triangulation is off by one, so every n-gon loses its
  last triangle. `man.obj` is entirely quads and loads as 24,459 triangles where 48,918 is
  correct — half the mesh is missing.
- **The camera ignores its own orientation.** Film offsets are applied in world XY, and the
  field of view is derived from the pixel count rather than the scene, so it only behaves
  looking along ±Z and aspect ratio is applied twice.
- **Secondary rays have no epsilon**, producing visible shadow acne.

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
| `PT_NATIVE_ARCH` | `OFF` | Compile with `-march=native`. Off by default because it changes floating point results between machines |

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
