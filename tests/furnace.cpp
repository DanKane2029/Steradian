// White furnace test: checks that the integrator conserves energy.
//
// A surface that reflects all the light falling on it, placed in an environment of
// uniform radiance L, must itself appear exactly as bright as that environment. Any
// other answer means energy is being created or destroyed somewhere in the path loop.
//
// This is a sharp test because almost every mistake in a path tracer shows up here. A
// missing cosine term, a probability density applied the wrong way round, a bounce that
// forgets to carry throughput, an epsilon that swallows valid intersections: all of them
// make the sphere darker or brighter than its surroundings, and the sphere becomes
// visible against a background it should vanish into.
//
// The same invariant is checked on both backends, and that is the point of checking it
// this way. It asserts a physical property rather than a stored number, so it needs no
// reference image and cannot be quietly re-baselined; it transfers to a new backend
// without losing any of its force. A GPU path loop that drops a cosine fails this exactly
// as loudly as a CPU one would.
//
// Usage: furnace <scene.json> [samplesPerPixel] [--device gpu]

#include "RayTracer/Integrator.h"
#include "Scene/Scene.h"
#include "Utils/Random.h"

#ifdef PT_HAVE_GPU
#include "Gpu/Tracer.h"
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{

#ifdef PT_HAVE_GPU

/**
 * \brief The furnace measurement, taken on the GPU.
 *
 * Renders a small image and averages the middle of it. The sphere subtends about 39
 * degrees of a 45 degree view, so the central third is comfortably inside its silhouette
 * and every path averaged there had to interact with the surface before escaping.
 *
 * \returns The mean radiance, or a negative value if the render failed.
 */
auto measureOnGpu(const Scene &scene, int samples) -> float
{
    std::string error;
    const std::unique_ptr<Gpu::Tracer> tracer = Gpu::Tracer::create(scene, error);

    if (tracer == nullptr)
    {
        std::fprintf(stderr, "could not start the GPU backend: %s\n", error.c_str());
        return -1.0f;
    }

    constexpr int size = 48;

    std::vector<Vec3> colour;
    std::vector<Vec3> albedo;
    std::vector<Vec3> normal;

    if (!tracer->render(scene.getCamera(), size, size, static_cast<unsigned int>(samples), 99, 12, true, colour, albedo,
                        normal))
    {
        std::fprintf(stderr, "the GPU render failed\n");
        return -1.0f;
    }

    const int low = size / 3;
    const int high = size - low;

    double total = 0.0;
    int counted = 0;

    for (int y = low; y < high; y++)
    {
        for (int x = low; x < high; x++)
        {
            total += colour[(static_cast<size_t>(y) * size) + static_cast<size_t>(x)].x;
            counted++;
        }
    }

    return static_cast<float>(total / counted);
}

#endif

} // namespace

auto main(int argc, char *argv[]) -> int
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <scene.json> [samplesPerPixel]\n", argv[0]);
        return 2;
    }

    bool useGpu = false;
    int samples = 4000;

    for (int i = 2; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--device") == 0 && (i + 1) < argc)
        {
            useGpu = std::strcmp(argv[++i], "gpu") == 0;
        }
        else
        {
            samples = std::atoi(argv[i]);
        }
    }

    Scene scene(argv[1]);
    scene.createAcceleratedStructure(4);

    const Vec3 environment = scene.getAmbientLighting();
    const float expected = environment.x;

    if (useGpu)
    {
#ifndef PT_HAVE_GPU
        std::fprintf(stderr, "this build has no GPU support\n");
        return 1;
#else
        const float measured = measureOnGpu(scene, samples);

        if (measured < 0.0f)
        {
            return 1;
        }

        const float gpuError = std::fabs(measured - expected) / expected;

        std::printf("GPU: environment radiance %.4f, surface renders as %.4f (%.2f%% error, %d samples)\n", expected,
                    measured, gpuError * 100.0f, samples);

        if (!std::isfinite(measured) || gpuError > 0.02f)
        {
            std::fprintf(stderr, "furnace test failed on the GPU: a fully reflective surface in a uniform "
                                 "environment must match that environment exactly\n");
            return 1;
        }

        return 0;
#endif
    }

    Integrator integrator(&scene, 12);

    // Fire rays straight at the middle of the sphere, where every path must interact
    // with the surface before escaping.
    const Camera &camera = scene.getCamera();

    Rng rng(99, 7);

    Vec3 total{};
    for (int i = 0; i < samples; i++)
    {
        // Jitter slightly so the test covers a patch of the surface rather than a single
        // point, which would hide any dependence on the angle of incidence.
        const Vec3 offset =
            (camera.right * (rng.nextFloatSigned() * 0.15f)) + (camera.up * (rng.nextFloatSigned() * 0.15f));

        const Ray ray(camera.org, camera.dir + offset);
        total += integrator.radiance(ray, rng);
    }

    const Vec3 average = total / static_cast<float>(samples);

    // The estimate is stochastic, so it is only expected to agree to within Monte Carlo
    // error. This tolerance is comfortably tighter than the errors that real integrator
    // bugs produce, which are typically tens of percent.
    constexpr float tolerance = 0.02f;

    const float error = std::fabs(average.x - expected) / expected;

    std::printf("environment radiance %.4f, surface renders as %.4f (%.2f%% error, %d samples)\n", expected, average.x,
                error * 100.0f, samples);

    if (!std::isfinite(average.x) || error > tolerance)
    {
        std::fprintf(stderr, "furnace test failed: a fully reflective surface in a uniform environment "
                             "must match that environment exactly\n");
        return 1;
    }

    return 0;
}
