// Checks that the GPU finds the same geometry the CPU does.
//
// This is the GPU counterpart of bvh_consistency, and it guards the same class of bug for
// the same reason: an acceleration structure that silently drops or invents intersections
// produces an image that looks plausible. On the CPU that was checked against a brute
// force scan. Here the CPU's own hierarchy is the reference, since it is already known to
// agree with brute force.
//
// What is being checked is the acceleration structure and the traversal: the instance
// hierarchy, the triangle build input, the custom sphere primitives and their
// intersection program, and the mapping from OptiX's primitive indices back to this
// project's own numbering. Shading is not involved.
//
// Exact agreement is neither expected nor asked for. OptiX intersects triangles with its
// own watertight algorithm on dedicated hardware, not with this project's
// Moller-Trumbore, so distances differ in their last bits, and a ray passing almost
// exactly along a shared edge can legitimately be given to either of the two triangles
// that share it. The thresholds below are measured.
//
// Usage: device_traversal <scene.json> [rayCount]

#include "Gpu/Tracer.h"
#include "Scene/Scene.h"
#include "Utils/Random.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

auto main(int argc, char *argv[]) -> int
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <scene.json> [rayCount]\n", argv[0]);
        return 2;
    }

    const int rayCount = (argc > 2) ? std::atoi(argv[2]) : 200000;

    Scene scene(argv[1]);
    scene.createAcceleratedStructure(2);

    const BVH *bvh = scene.getAccelerationStructure();
    const Geometry &geometry = scene.getGeometry();

    if (bvh == nullptr)
    {
        std::fprintf(stderr, "scene produced no acceleration structure\n");
        return 1;
    }

    std::string error;
    const std::unique_ptr<Gpu::Tracer> tracer = Gpu::Tracer::create(scene, error);

    if (tracer == nullptr)
    {
        std::fprintf(stderr, "could not build the GPU tracer: %s\n", error.c_str());
        return 1;
    }

    // The same aiming as bvh_consistency: from a shell around the scene towards points
    // inside it, so most rays engage the geometry rather than sailing past.
    const AABB sceneBounds = geometry.bounds();
    const Vec3 center = sceneBounds.centroid();
    const float radius = std::max(sceneBounds.extent().length(), 1.0f);

    Rng rng(24680, 13579);

    std::vector<Vec3> origins;
    std::vector<Vec3> directions;
    origins.reserve(static_cast<size_t>(rayCount));
    directions.reserve(static_cast<size_t>(rayCount));

    for (int i = 0; i < rayCount; i++)
    {
        const Vec3 origin = center + Vec3(rng.nextFloatSigned(), rng.nextFloatSigned(), rng.nextFloatSigned()) * radius;
        const Vec3 target =
            center + Vec3(rng.nextFloatSigned(), rng.nextFloatSigned(), rng.nextFloatSigned()) * (radius * 0.5f);

        origins.push_back(origin);

        // Normalized here rather than by Ray, so both backends are given the identical
        // direction and any difference is the traversal's rather than the setup's.
        directions.push_back((target - origin).normalized());
    }

    // Warm the pipeline before timing: the first launch pays for module loading and the
    // driver's own lazy setup, which is not what anyone wants measured.
    std::vector<Gpu::DeviceHit> deviceHits;
    if (!tracer->trace(origins, directions, Ray::defaultEpsilon, INFINITY, deviceHits))
    {
        std::fprintf(stderr, "the GPU launch failed\n");
        return 1;
    }

    const auto gpuStart = std::chrono::steady_clock::now();
    tracer->trace(origins, directions, Ray::defaultEpsilon, INFINITY, deviceHits);
    const auto gpuEnd = std::chrono::steady_clock::now();

    std::printf("scene has %u triangles, %u spheres; %d rays\n\n", geometry.triangleCount(), geometry.sphereCount(),
                rayCount);

    const auto cpuStart = std::chrono::steady_clock::now();

    std::vector<Hit> cpuHits(static_cast<size_t>(rayCount));
    for (int i = 0; i < rayCount; i++)
    {
        cpuHits[static_cast<size_t>(i)] =
            bvh->intersect(Ray(origins[static_cast<size_t>(i)], directions[static_cast<size_t>(i)]));
    }

    const auto cpuEnd = std::chrono::steady_clock::now();

    int hits = 0;
    int presenceMismatches = 0;
    int primitiveMismatches = 0;
    int distanceMismatches = 0;

    float worstDistance = 0.0f;

    for (int i = 0; i < rayCount; i++)
    {
        const Hit &cpu = cpuHits[static_cast<size_t>(i)];
        const Gpu::DeviceHit &gpu = deviceHits[static_cast<size_t>(i)];

        const bool gpuHit = gpu.primitive != Gpu::noPrimitive;

        if (cpu.isHit() != gpuHit)
        {
            if (presenceMismatches < 5)
            {
                std::fprintf(stderr, "ray %d: CPU says %s, GPU says %s\n", i, cpu.isHit() ? "hit" : "miss",
                             gpuHit ? "hit" : "miss");
            }
            presenceMismatches++;
            continue;
        }

        if (!gpuHit)
        {
            continue;
        }

        hits++;

        const float scale = std::max(1.0f, std::fabs(cpu.time));
        const float difference = std::fabs(cpu.time - gpu.time) / scale;

        worstDistance = std::max(worstDistance, difference);

        if (difference > 1e-4f)
        {
            distanceMismatches++;
        }

        if (cpu.primitive != gpu.primitive)
        {
            primitiveMismatches++;
        }
    }

    const auto share = [&](int count) { return 100.0 * static_cast<double>(count) / std::max(1, hits); };

    std::printf("  hits                 %d\n", hits);
    std::printf("  hit/miss disagreements %d\n", presenceMismatches);
    std::printf("  different primitive  %d (%.4f%%)\n", primitiveMismatches, share(primitiveMismatches));
    std::printf("  distance beyond 1e-4 %d (%.4f%%)\n", distanceMismatches, share(distanceMismatches));
    std::printf("  worst distance       %.3e relative\n", static_cast<double>(worstDistance));

    // Indicative only, and stated as such. This times traversal alone against a single
    // CPU thread; the renderer runs eight of them, and a whole render is far more than
    // traversal. What the ratio is worth to an actual image is the next stage's question,
    // and it will be answered by rendering one rather than by scaling this.
    const double gpuSeconds = std::chrono::duration<double>(gpuEnd - gpuStart).count();
    const double cpuSeconds = std::chrono::duration<double>(cpuEnd - cpuStart).count();

    std::printf("\n  traversal only, and against one CPU thread:\n");
    std::printf("    GPU                %8.1f M rays/s\n", rayCount / gpuSeconds / 1e6);
    std::printf("    CPU (1 thread)     %8.1f M rays/s\n", rayCount / cpuSeconds / 1e6);
    std::printf("    ratio              %8.1fx\n", cpuSeconds / gpuSeconds);

    if (hits == 0)
    {
        std::fprintf(stderr, "\nno rays hit anything; the test is not exercising anything\n");
        return 1;
    }

    // Measured thresholds.
    //
    // A ray that grazes a shared edge can be given to either of the two triangles meeting
    // there, and the two backends need not choose alike -- so a small number of
    // disagreements is the correct answer rather than a defect. What would not be correct
    // is a systematic one: a wrong index mapping, a structure missing a primitive, or an
    // instance pointed at the wrong hit group all show up as percentages, not fractions
    // of one.
    constexpr double presenceLimit = 0.05;
    constexpr double primitiveLimit = 0.05;
    constexpr double distanceLimit = 0.05;

    int failures = 0;

    if (share(presenceMismatches) > presenceLimit)
    {
        std::fprintf(stderr, "\nthe two backends disagree about whether rays hit at all\n");
        failures++;
    }

    if (share(primitiveMismatches) > primitiveLimit)
    {
        std::fprintf(stderr, "\nthe two backends disagree about which primitive was hit\n");
        failures++;
    }

    if (share(distanceMismatches) > distanceLimit)
    {
        std::fprintf(stderr, "\nthe two backends disagree about how far away it was\n");
        failures++;
    }

    if (failures != 0)
    {
        return 1;
    }

    std::printf("\nThe GPU finds the same geometry the CPU does.\n");
    return 0;
}
