// Compares a rendered image against a committed reference.
//
// Renders are deterministic for a given seed and sample count, so on one machine the
// comparison is exact. Tolerances exist so a different compiler or optimization level,
// which can reorder floating point arithmetic, does not fail the suite for a difference
// no one can see.
//
// Usage: image_compare <reference.png> <actual.png> [maxChannelDiff] [meanChannelDiff]
// Exit status is 0 when the images match within tolerance, 1 otherwise.

#include "Utils/stb_image.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{

struct Image
{
    int width = 0;
    int height = 0;
    unsigned char *pixels = nullptr;

    ~Image()
    {
        if (pixels != nullptr)
        {
            stbi_image_free(pixels);
        }
    }
};

auto load(const char *path, Image &image) -> bool
{
    int channels = 0;
    image.pixels = stbi_load(path, &image.width, &image.height, &channels, 3);

    if (image.pixels == nullptr)
    {
        std::fprintf(stderr, "image_compare: could not read '%s': %s\n", path, stbi_failure_reason());
        return false;
    }

    return true;
}

} // namespace

auto main(int argc, char *argv[]) -> int
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: %s <reference.png> <actual.png> [maxChannelDiff] [meanChannelDiff]\n", argv[0]);
        return 2;
    }

    // Deliberately strict. Renders are deterministic, so the only differences a correct
    // change should produce are last-bit rounding from a different compiler reordering
    // floating point arithmetic. Allowing one level of channel difference covers that.
    //
    // These numbers were chosen against a measurement, not by guessing: a deliberate 2%
    // brightness regression moves max by 2-5 and mean by 0.027-0.30 across the suite, so
    // anything looser than this silently passes a real regression. If a future toolchain
    // makes this flaky, pin the compiler rather than loosening the tolerance -- a loose
    // tolerance turns the suite into a formality.
    const double maxTolerance = (argc > 3) ? std::atof(argv[3]) : 1.0;
    const double meanTolerance = (argc > 4) ? std::atof(argv[4]) : 0.02;

    Image reference;
    Image actual;

    if (!load(argv[1], reference) || !load(argv[2], actual))
    {
        return 2;
    }

    if (reference.width != actual.width || reference.height != actual.height)
    {
        std::fprintf(stderr, "image_compare: size mismatch, reference is %dx%d but actual is %dx%d\n", reference.width,
                     reference.height, actual.width, actual.height);
        return 1;
    }

    const size_t count = static_cast<size_t>(reference.width) * static_cast<size_t>(reference.height) * 3;

    double maxDiff = 0.0;
    double sumDiff = 0.0;
    size_t differing = 0;

    for (size_t i = 0; i < count; i++)
    {
        const double diff = std::fabs(static_cast<double>(reference.pixels[i]) - static_cast<double>(actual.pixels[i]));

        sumDiff += diff;
        if (diff > maxDiff)
        {
            maxDiff = diff;
        }
        if (diff > 0.0)
        {
            differing++;
        }
    }

    const double meanDiff = sumDiff / static_cast<double>(count);
    const double differingPercent = 100.0 * static_cast<double>(differing) / static_cast<double>(count);

    std::printf("max channel diff %.2f (tolerance %.2f), mean %.4f (tolerance %.4f), %.2f%% of channels differ\n",
                maxDiff, maxTolerance, meanDiff, meanTolerance, differingPercent);

    if (maxDiff > maxTolerance || meanDiff > meanTolerance)
    {
        std::fprintf(stderr, "image_compare: '%s' does not match reference '%s'\n", argv[2], argv[1]);
        return 1;
    }

    return 0;
}
