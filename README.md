# Path_Tracer

A from-scratch CPU renderer in C++.

> **Status:** despite the project name, the renderer currently implements a **Whitted-style
> ray tracer** with a Blinn-Phong direct lighting model, mirror reflections and stochastic
> soft shadows. There is no Monte Carlo path integral and no global illumination yet —
> converting the integrator is planned work, and this note will be updated when it lands.

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

The interactive GLFW viewer needs OpenGL and X11 development headers
(`libgl1-mesa-dev`, `xorg-dev` on Debian/Ubuntu). If they are not present, CMake prints a
warning and builds a **headless-only** binary instead of failing. To skip the viewer
explicitly:

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
./build/src/path_tracer --config res/configs/test_config.json \
                        --scene  res/scenes/two_spheres.json

# headless render to a PNG
./build/src/path_tracer --config res/configs/test_config.json \
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
