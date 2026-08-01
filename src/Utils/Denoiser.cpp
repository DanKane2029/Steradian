#include "Denoiser.h"

#include <algorithm>
#include <cmath>

namespace Denoiser
{

namespace
{

/**
 * The 5x5 B3-spline kernel used by the a-trous transform, stored as its separable
 * 1D form. Repeating it with a widening gap approximates a Gaussian of growing radius.
 */
constexpr float kernel1D[5] = {1.0f / 16.0f, 1.0f / 4.0f, 3.0f / 8.0f, 1.0f / 4.0f, 1.0f / 16.0f};

auto squaredDistance(const float *a, const float *b) -> float
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];

    return (dx * dx) + (dy * dy) + (dz * dz);
}

} // namespace

auto denoise(int width, int height, const float *color, const float *albedo, const float *normal,
             const Settings &settings) -> std::vector<float>
{
    const auto componentCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;

    std::vector<float> current(color, color + componentCount);
    std::vector<float> next(componentCount);

    if (width <= 0 || height <= 0 || settings.iterations <= 0)
    {
        return current;
    }

    // Pull isolated outliers back towards their surroundings before filtering. The colour
    // term below cannot distinguish a stray bright sample from a real edge, so without
    // this step every firefly is faithfully preserved while everything around it is
    // smoothed, which makes them stand out more than before.
    if (settings.outlierThreshold > 0.0f)
    {
        std::vector<float> clamped(current);

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                const size_t centre =
                    ((static_cast<size_t>(y) * static_cast<size_t>(width)) + static_cast<size_t>(x)) * 3;

                for (int c = 0; c < 3; c++)
                {
                    // Compare against the second-brightest neighbour rather than the
                    // brightest, so a pair of adjacent outliers cannot shield each other.
                    float best = 0.0f;
                    float second = 0.0f;

                    for (int ky = -1; ky <= 1; ky++)
                    {
                        for (int kx = -1; kx <= 1; kx++)
                        {
                            if (kx == 0 && ky == 0)
                            {
                                continue;
                            }

                            const int sx = std::clamp(x + kx, 0, width - 1);
                            const int sy = std::clamp(y + ky, 0, height - 1);

                            const float v = current[(((static_cast<size_t>(sy) * static_cast<size_t>(width)) +
                                                      static_cast<size_t>(sx)) *
                                                     3) +
                                                    static_cast<size_t>(c)];

                            if (v > best)
                            {
                                second = best;
                                best = v;
                            }
                            else if (v > second)
                            {
                                second = v;
                            }
                        }
                    }

                    const float ceiling = second * settings.outlierThreshold;
                    const size_t index = centre + static_cast<size_t>(c);

                    if (current[index] > ceiling)
                    {
                        clamped[index] = ceiling;
                    }
                }
            }
        }

        current.swap(clamped);
    }

    for (int iteration = 0; iteration < settings.iterations; iteration++)
    {
        // Each pass reads taps twice as far apart as the last, so a handful of passes
        // cover a wide neighbourhood without ever needing a large kernel.
        const int step = 1 << iteration;

        // Colour tolerance is held constant across passes, and deliberately tight.
        //
        // Loosening it as the passes widen, which an earlier version did, defeats the
        // whole design: within a flat wall the normal and albedo guides are constant, so
        // colour is the only thing left telling the filter where a shadow edge is. Once
        // the tolerance grows past the size of real lighting differences, later passes
        // become an unguided blur and flatten the shading they were meant to preserve.
        const float colorSigmaSquared = settings.colorSigma * settings.colorSigma;
        const float albedoSigmaSquared = settings.albedoSigma * settings.albedoSigma;

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                const size_t centre =
                    ((static_cast<size_t>(y) * static_cast<size_t>(width)) + static_cast<size_t>(x)) * 3;

                float sum[3] = {0.0f, 0.0f, 0.0f};
                float weightSum = 0.0f;

                for (int ky = -2; ky <= 2; ky++)
                {
                    const int sy = y + (ky * step);
                    if (sy < 0 || sy >= height)
                    {
                        continue;
                    }

                    for (int kx = -2; kx <= 2; kx++)
                    {
                        const int sx = x + (kx * step);
                        if (sx < 0 || sx >= width)
                        {
                            continue;
                        }

                        const size_t tap =
                            ((static_cast<size_t>(sy) * static_cast<size_t>(width)) + static_cast<size_t>(sx)) * 3;

                        // Spatial term: the plain blur this would be without any guides.
                        float weight = kernel1D[ky + 2] * kernel1D[kx + 2];

                        // Colour term: stops the filter dragging a bright region into a
                        // dark one. This is what preserves the shape of shadows.
                        const float colorDistance = squaredDistance(&current[centre], &current[tap]);
                        weight *= std::exp(-colorDistance / colorSigmaSquared);

                        // Normal term: stops the filter blurring across a change in
                        // orientation, which is what keeps creases and silhouettes sharp.
                        const float normalDot = (normal[centre] * normal[tap]) +
                                                (normal[centre + 1] * normal[tap + 1]) +
                                                (normal[centre + 2] * normal[tap + 2]);
                        weight *= std::pow(std::max(0.0f, normalDot), settings.normalSigma);

                        // Albedo term: stops the filter bleeding one material's colour
                        // into another across a boundary, such as a red wall meeting a
                        // white floor.
                        const float albedoDistance = squaredDistance(&albedo[centre], &albedo[tap]);
                        weight *= std::exp(-albedoDistance / albedoSigmaSquared);

                        sum[0] += current[tap] * weight;
                        sum[1] += current[tap + 1] * weight;
                        sum[2] += current[tap + 2] * weight;
                        weightSum += weight;
                    }
                }

                if (weightSum > 0.0f)
                {
                    next[centre] = sum[0] / weightSum;
                    next[centre + 1] = sum[1] / weightSum;
                    next[centre + 2] = sum[2] / weightSum;
                }
                else
                {
                    // Every neighbour was rejected, so the pixel keeps its own value.
                    next[centre] = current[centre];
                    next[centre + 1] = current[centre + 1];
                    next[centre + 2] = current[centre + 2];
                }
            }
        }

        current.swap(next);
    }

    return current;
}

} // namespace Denoiser
