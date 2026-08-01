# Steradian

A from-scratch CPU renderer in C++.

Named for the SI unit of solid angle — integrating incoming radiance over the hemisphere
above a surface is exactly what it does.

It is a **Monte Carlo path tracer**: light is estimated by following paths from the
camera through the scene, so indirect illumination, colour bleeding and soft shadows come
out of the method itself rather than being approximated.

## Roadmap

| Stage | Goal | State |
| --- | --- | --- |
| 0 | Build and run reproducibly; headless PNG output; deterministic seeding | done |
| 1 | Golden-image regression tests, CI, BVH/ray instrumentation | done |
| 2 | Correctness pass: camera basis and real FOV, ray epsilons, OBJ triangulation, textures | done |
| 3 | Performance: flat SAH BVH that actually culls, occlusion queries, thread pool | done |
| 4 | Monte Carlo path tracing: hemisphere sampling, BSDFs, area lights, tone mapping | done |
| 5 | Interactive viewer: progressive accumulation, camera controls, tone mapped display | done |

### How it works

Paths start at the camera and scatter until they escape the scene or are terminated by
Russian roulette. Each vertex carries a throughput term, so light that has bounced off
other surfaces is carried along the path rather than approximated by a constant.

- **Cosine-weighted hemisphere sampling** for diffuse surfaces. The rendering equation
  already contains a cosine factor, so drawing samples proportional to it makes the two
  cancel: a diffuse bounce reduces to multiplying by albedo, with no variance from the
  cosine at all.
- **Direct light sampling** at every diffuse and glossy vertex, aimed at the cone each
  spherical emitter subtends. Waiting for a scattered ray to land on a small bright light
  by chance is what makes naive path tracers so noisy.
- **Multiple importance sampling** combining the two strategies above by the balance
  heuristic. Light sampling is excellent for a small distant source and poor for one that
  fills the sky; scattering is the reverse, and for a glossy surface far better still.
  Weighting them takes the better of the two everywhere without deciding in advance which
  case a scene is in. Measured on the glossy test scene: **1.80x less noise, equivalent to
  3.2x the sample count.**
- **GGX microfacet conductors**, sampled by visible normals, which gives rough metal a
  real probability density. That density is what lets it take part in the weighting above
  at all.
- **Russian roulette** past the third bounce, terminating low-contribution paths at random
  and scaling survivors up to compensate, which costs nothing in bias.
- **Stratified pixel samples**, one per cell of a jittered grid rather than independent
  draws that clump together by chance.
- **ACES tone mapping** then sRGB encode on output. A path tracer produces unbounded
  radiance; clamping at 1.0 turns every highlight into flat white.

### Materials

| `type` | Meaning |
| --- | --- |
| `diffuse` | Lambertian. `albedo` is the reflectance |
| `metal` | GGX microfacet conductor. `albedo` is the reflectance at normal incidence, `roughness` the width of the highlight |
| `dielectric` | Glass: refracts with Fresnel-weighted reflection, controlled by `ior` |

Any material with a non-zero `emissive` is a light. Scenes written against the older
Blinn-Phong fields (`diffuse`, `specular`, `reflection`, `transparency`) still load and
are mapped onto the closest equivalent.

### Known limitations

- **Direct light sampling covers spherical emitters only.** Emissive geometry of other
  shapes still lights the scene correctly through ordinary path tracing, just with more
  noise.
- **Rough conductors lose energy.** The microfacet model accounts for light scattering
  off the surface once, not for light bouncing between microfacets before leaving, so
  rough metal is darker than it should be. Measured in a uniform environment with a
  fully reflective conductor:

  | Roughness | 0.05 | 0.2 | 0.4 | 0.6 | 0.8 | 1.0 |
  | --- | --- | --- | --- | --- | --- | --- |
  | Energy reflected | 1.00 | 1.00 | 0.96 | 0.82 | 0.56 | 0.32 |

  Negligible below about 0.4 and severe above it. The fix is a multiple-scattering
  compensation term.
- **Dielectrics are smooth only.** Roughness applies to conductors; glass is perfectly
  clear.

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
                        --scene  res/scenes/cornell_box.json

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
| `--denoise [n]` | Filter noise using surface colour and normals as a guide (default 5 passes). Applies to both saved images and the viewer |
| `--denoise-strength <v>` | How much radiance difference the filter blurs across (default 0.10) |
| `--headless` | Render once and exit without opening a window |

A headless render is **deterministic**: the same `--seed` and `--samples` produce a
byte-identical PNG regardless of `--threads`. That is what makes golden-image regression
testing possible.

Asset paths inside a scene file are resolved relative to that scene file, so scenes and
models can be moved or checked out anywhere.

### Viewer

The window refines its image progressively: every frame adds one more sample per pixel
and the buffer holds the running average, so leaving the camera still lets the picture
converge. Moving discards the accumulation, because those samples describe light arriving
at a viewpoint that no longer exists.

| Input | Action |
| --- | --- |
| `W` `A` `S` `D` | Move forward, left, back, right |
| `Q` `E` | Move down, up |
| Hold `Shift` | Move four times faster |
| Left-drag | Look around |
| `[` `]` | Decrease / increase movement speed |
| `R` | Return to the camera the scene file specified |
| `Esc` | Quit |

The title bar shows accumulated samples per pixel, frame rate and movement speed. The
viewer applies the same ACES tone mapping and sRGB encoding as the file writer, so what
is on screen matches what `--out` produces.

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

### Denoising

A path traced image is noisy because each pixel averages a limited number of random light
paths. The noise is entirely in the *lighting*: which surface is visible at a pixel, and
which way it faces, are known almost exactly from a single sample.

`--denoise` exploits that. The renderer also produces an albedo and a normal buffer, which
are nearly noise-free, and the filter then blurs the lighting hard while refusing to cross
edges those guides reveal. Two pixels on the same flat wall are averaged freely; two
spanning a wall-to-floor boundary are not.

```sh
steradian --config res/configs/test_config.json \
          --scene res/scenes/cornell_box.json \
          --out render.png --samples 64 --denoise
```

Five passes over a 400x400 image take about 0.4 s.

It applies to the viewer as well, where the filtered image is refreshed while the camera
is still and skipped while it is moving, since a view about to be replaced does not
benefit from being cleaned up. The refresh interval is derived from how long the previous
run took, so the filter stays roughly a quarter of the frame budget whatever the
resolution: a fixed interval works at small sizes and collapses the frame rate at large
ones, where a single pass can take longer than the interval itself.

**Measured**, against a 4000-sample reference at 32 samples per pixel:

| Strength | RMS error | vs. not denoising |
| --- | --- | --- |
| off | 12.44 | — |
| 0.06 | 11.21 | 1.11x closer |
| **0.10** (default) | **10.78** | **1.15x closer** |
| 0.15 | 11.74 | 1.06x closer |
| 0.30 | 17.05 | 0.73x, *further away* |

Two things that table is saying. First, there is an optimum: past it the filter removes
more real shading than noise and the image gets measurably worse, however smooth it looks.
Second, even at the optimum the numerical gain is modest, roughly equivalent to 1.3x the
samples, while the *visible* improvement is far larger than that. Noise is much more
objectionable to the eye than a slight loss of detail, which is exactly the trade being
made, and it is why denoising is judged by eye in practice rather than by error metrics.

It is an approximation, and worth knowing where it costs you:

- **Reflections blur.** The guides describe the surface itself, not what it reflects, so
  the filter cannot tell that detail in a mirror has its own structure. Production
  denoisers keep a separate specular buffer; this one does not.
- **Fine lighting detail softens** -- small caustics, tight contact shadows.
- **Nothing verifies it.** Every other part of the renderer can be checked against a
  ground truth; changing the image is this feature's entire purpose, so the furnace test
  has nothing to say about it.

Best used on an image that is already reasonably converged, rather than as a substitute
for sampling.

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
- **A white furnace test.** A surface that reflects all light, placed in an environment of
  uniform radiance, must render as exactly that environment and vanish into it. Nearly
  every integrator mistake — a dropped cosine, a density applied the wrong way round, a
  bounce that loses throughput — makes the sphere visible, so this one test covers a great
  deal of ground.

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
