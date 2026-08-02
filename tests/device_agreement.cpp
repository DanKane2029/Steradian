// Checks that the two backends render the same picture.
//
// They cannot render the same *pixels*. The CPU seeds its generator per row and consumes
// that row's stream left to right, which threads running at once cannot reproduce, so the
// GPU seeds per pixel instead. The two therefore draw different random numbers and carry
// different noise. Comparing them directly against a fixed tolerance would measure that
// noise and call it disagreement.
//
// What can be asked is whether they converge to the same image, and there is a sharp way
// to ask it: compare each backend against *itself* at a different seed, and compare the
// two backends against each other. Both differences are made of the same Monte Carlo
// noise. If the estimators agree, the cross-backend difference is no larger than a
// backend's own seed-to-seed difference.
//
// So the quantity asserted is a ratio, not an absolute, and it needs no tolerance pulled
// out of the air: two unbiased estimators of the same integral give one.
//
// The limits of that are measured and stated at the assertion below. In short: this
// catches gross divergence by an order of magnitude and does not catch small localized
// bias, which is the furnace test's job and which the furnace test does far better.
//
// Usage: device_agreement <scene.json> [samplesPerPixel]

#include "Gpu/Tracer.h"
#include "RayTracer/RayTracer.h"
#include "Scene/Scene.h"
#include "Utils/Config.h"
#include "Utils/ThreadPool.h"
#include "Window/PixelBuffer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

constexpr int width = 160;
constexpr int height = 128;
constexpr unsigned int maxDepth = 10;

/** \brief Renders on the CPU and returns the linear image, one Vec3 per pixel. */
auto renderOnCpu(Scene &scene, unsigned int samples, uint64_t seed) -> std::vector<Vec3>
{
    PixelBuffer buffer(width, height);

    Config config;
    config.windowWidth = width;
    config.windowHeight = height;
    config.maxRecurseLevel = maxDepth;

    RayTracer rayTracer(&buffer, &scene, config);

    ThreadPool pool(8);
    pool.parallelFor(static_cast<uint32_t>(height), [&](uint32_t row) {
        rayTracer.renderRows(static_cast<int>(row), static_cast<int>(row) + 1, samples, seed);
    });

    std::vector<Vec3> image(static_cast<size_t>(width) * height);

    const float *pixels = buffer.getPixels();
    for (size_t i = 0; i < image.size(); i++)
    {
        image[i] = Vec3(pixels[(i * 3) + 0], pixels[(i * 3) + 1], pixels[(i * 3) + 2]);
    }

    return image;
}

/**
 * \brief Mean absolute difference per channel over one block of the image.
 *
 * Taken on the linear values rather than on encoded pixels, so it is not compressed by the
 * tone curve at the bright end where the noise mostly lives.
 */
auto meanDifference(const std::vector<Vec3> &a, const std::vector<Vec3> &b) -> double
{
    double total = 0.0;

    for (size_t i = 0; i < a.size(); i++)
    {
        total += std::fabs(a[i].x - b[i].x) + std::fabs(a[i].y - b[i].y) + std::fabs(a[i].z - b[i].z);
    }

    return total / (3.0 * static_cast<double>(a.size()));
}

} // namespace

auto main(int argc, char *argv[]) -> int
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <scene.json> [samplesPerPixel]\n", argv[0]);
        return 2;
    }

    const auto samples = static_cast<unsigned int>((argc > 2) ? std::atoi(argv[2]) : 256);

    Scene scene(argv[1]);
    scene.createAcceleratedStructure(2);

    std::string error;
    const std::unique_ptr<Gpu::Tracer> tracer = Gpu::Tracer::create(scene, error);

    if (tracer == nullptr)
    {
        std::fprintf(stderr, "could not start the GPU backend: %s\n", error.c_str());
        return 1;
    }

    std::vector<Vec3> gpuOne;
    std::vector<Vec3> gpuTwo;
    std::vector<Vec3> unusedAlbedo;
    std::vector<Vec3> unusedNormal;

    // restart between them: these are two independent images, not one refined twice.
    if (!tracer->render(scene.getCamera(), width, height, samples, 1, maxDepth, true, gpuOne, unusedAlbedo,
                        unusedNormal) ||
        !tracer->render(scene.getCamera(), width, height, samples, 2, maxDepth, true, gpuTwo, unusedAlbedo,
                        unusedNormal))
    {
        std::fprintf(stderr, "the GPU render failed\n");
        return 1;
    }

    const std::vector<Vec3> cpuOne = renderOnCpu(scene, samples, 1);
    const std::vector<Vec3> cpuTwo = renderOnCpu(scene, samples, 2);

    const double cpuNoise = meanDifference(cpuOne, cpuTwo);
    const double gpuNoise = meanDifference(gpuOne, gpuTwo);
    const double crossDifference = meanDifference(cpuOne, gpuOne);

    // The larger of the two self differences is the fairer reference: it is the amount of
    // disagreement that noise alone is known to produce at this sample count.
    const double noiseFloor = std::max(cpuNoise, gpuNoise);
    const double wholeImageRatio = (noiseFloor > 0.0) ? (crossDifference / noiseFloor) : 0.0;

    std::printf("%dx%d at %u spp\n\n", width, height, samples);
    std::printf("  CPU against itself   %.5f\n", cpuNoise);
    std::printf("  GPU against itself   %.5f\n", gpuNoise);
    std::printf("  CPU against GPU      %.5f\n", crossDifference);
    std::printf("  ratio to noise floor %.3f\n", wholeImageRatio);

    if (noiseFloor <= 0.0)
    {
        std::fprintf(stderr, "\nboth seeds gave identical images; the comparison is meaningless\n");
        return 1;
    }

    const double ratio = wholeImageRatio;

    // Set from measurement, and so is the scope of what this test claims.
    //
    // With both backends correct the ratio sits between 0.99 and 1.03 across every scene
    // here, at 64 to 4096 samples. Deleting direct light sampling from the device path
    // takes it to 9.1 on the Cornell box and 13.5 on the glossy spheres, so gross
    // divergence -- a wrong camera, a mishandled material, a missing estimator -- is
    // caught by an order of magnitude.
    //
    // What it does NOT catch is a small bias confined to part of the image: deleting the
    // conductor energy compensation moves this only to 1.03. A per-block form was tried
    // to reach those, and measured: it gave 2.6 for that real bias and 3.8 for a scene
    // with no bias at all, because in near-noiseless blocks the ratio divides a tiny
    // difference by a tinier noise floor. It could not tell the two apart and was
    // dropped rather than tuned until it passed.
    //
    // Localized energy errors are the furnace test's job, and it does that job well: the
    // same deleted compensation shows there as a 44% error against a 2% tolerance,
    // because it asserts a physical invariant instead of comparing two noisy pictures.
    constexpr double limit = 1.25;

    if (!std::isfinite(ratio) || ratio > limit)
    {
        std::fprintf(stderr, "\nthe backends differ from each other by more than either differs from itself,\n"
                             "which is what a bias in one of them looks like\n");
        return 1;
    }

    std::printf("\nThe two backends agree to within their own noise.\n");
    return 0;
}
