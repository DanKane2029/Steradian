# Future work

What is left, why each thing is worth doing, and — where it is known — what it would be
worth. Items are ordered by how much evidence stands behind them rather than by how
interesting they are, because the two orders differ and the second one is a trap.

Nothing here is a commitment. Several entries end in "measure first", and that is the
point: the largest surprises in this project so far have come from acting on a plausible
prediction instead of a measurement, in both directions.

---

## Backed by a measurement

### CUDA/OpenGL interop for the viewer

The GPU is no longer the limiting factor in the interactive viewer. At one sample per
frame the render takes **19 ms** at 1000x800, and the display path — tone mapping 800,000
pixels on the CPU and uploading them — is most of what remains of the frame.

Interop would let the accumulation buffer be displayed without the round trip through host
memory. Everything else in this file is speculative next to it.

Worth knowing before starting: the round trip itself is not obviously the expensive part.
Three copies of 9.6 MB is a few milliseconds over PCIe, so the tone map, which is a serial
pass over 800,000 pixels on one core, is the more likely culprit. Measure the two
separately first. If it is the tone map, moving *that* to the device is a smaller change
than interop and may be the whole win.

### The CPU's traversal breadth

The largest known unclaimed win on the CPU side, and untouched.

The dragon costs **1,001 bounding box visits per ray** against 136 primitive tests, at
400x320 and 64 samples. That ratio says the hierarchy is being swept broadly rather than
descended once. The tree itself is well formed — 202,523 primitives in 238,397 nodes at
depth 21 — so this is a traversal or build-quality question rather than a defect.

Both figures depend on where the camera is, so quote the conditions with them: an earlier
measurement of 658 and 87.5 was taken before the models were reoriented, and is not
comparable.

RT cores make it moot on the GPU path, which is exactly why it is still here. It would
make the oracle faster, and the oracle is what every GPU test is checked against.

### Vertex normals for models that supply none

168 dragon vertices (0.2%) still carry no normal and fall back to flat shading, because the
source file does not provide one for them. Generating smooth normals at load — the
area-weighted or angle-weighted average of adjacent face normals — would close it.

Small, but the reason it matters is not. Every mesh in this project rendered flat-shaded
for its entire history because of a one-line bug in `.obj` parsing, and it survived that
long partly because the artifact was mistaken for noise. Removing the last of the fallback
removes the last place that failure can hide.

---

## Plausible, but measure first

### Adaptive sampling on the device

Implemented on the CPU, and the GPU says plainly that it is ignoring the flag rather than
quietly rendering something else. It is per pixel and independent, so it ports naturally,
and in principle its win should be *larger* on a device, where uneven work costs more
because of divergence.

Against that: at 605-915x the pressure is off, and the CPU measurements showed adaptive
sampling to be worth about 13% at equal quality on a scene with easy regions and nothing at
all on one without. Revisit after interop, against a viewer that is actually
device-limited.

### Wavefront path tracing

Only if divergence is measured to be the limit. A megakernel was the right starting point
and may well be the right ending point at this scene complexity. No measurement currently
suggests otherwise, and "GPU path tracers are usually wavefront" is not one.

### The denoiser on the device

The a-trous filter runs on the host over the read-back image, which makes it part of the
display cost above. Moving it would help the viewer, and would matter more after interop
rather than before.

Any replacement — including the OptiX AI denoiser — has to clear the bar the existing one
was tuned against: measured against a converged reference, not judged by whether it looks
smoother.

---

## Small and known

- **Two compiler warnings**, both in `src/Utils/ObjModel.cpp`: an unused `lineCount`, and a
  signed/unsigned comparison in the face-index loop. Neither is a bug today.
- **Three files are CRLF** where the rest of the project is LF: `src/RayTracer/Ray.h`,
  `src/Utils/ObjModel.cpp`, `src/Utils/ObjModel.h`. Harmless until someone edits one with a
  tool that normalises, at which point the diff becomes the whole file and buries the real
  change. This has already happened once.
- **A second backend for cross-vendor GPUs.** Vulkan ray tracing reaches the same hardware
  and is not tied to NVIDIA. Rejected deliberately when OptiX was chosen — instance,
  device, queue, descriptor, pipeline, allocator and binding-table boilerplate before the
  first ray, against an OptiX path already proven — and worth reconsidering only if
  cross-vendor support becomes a goal rather than a nicety.

---

## Deliberately not planned

- **Replacing the CPU renderer.** It is the oracle. Every GPU correctness claim is checked
  against it, and a project with one backend cannot make those checks at all.
- **Multi-GPU, or geometry that does not fit in memory.** The dragon is about 10 MB packed
  against 7.7 GiB of device memory. There is no problem here to solve.
- **Byte-identical CPU and GPU output.** Not achievable — fused multiply-add contraction,
  different transcendental implementations and different summation orders all differ — and
  attempting it produces a suite that fails constantly and gets loosened until it means
  nothing. The furnace test, GPU self-determinism and cross-backend statistical agreement
  are what replace it.
