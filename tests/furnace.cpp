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
// Usage: furnace <scene.json> [samplesPerPixel]

#include "RayTracer/Integrator.h"
#include "Scene/Scene.h"
#include "Utils/Random.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

auto main(int argc, char *argv[]) -> int
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <scene.json> [samplesPerPixel]\n", argv[0]);
        return 2;
    }

    const int samples = (argc > 2) ? std::atoi(argv[2]) : 4000;

    Scene scene(argv[1]);
    scene.createAcceleratedStructure(4);

    const Vec3 environment = scene.getAmbientLighting();
    const float expected = environment.x;

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
