#include "ImageIO.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Utils/stb_image_write.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ImageIO
{

auto linearToSrgb(float linear) -> float
{
    if (!std::isfinite(linear) || linear <= 0.0f)
    {
        return 0.0f;
    }

    if (linear > 1.0f)
    {
        linear = 1.0f;
    }

    if (linear <= 0.0031308f)
    {
        return 12.92f * linear;
    }

    return (1.055f * std::pow(linear, 1.0f / 2.4f)) - 0.055f;
}

auto toneMap(const float *linearRgb) -> std::array<float, 3>
{
    // A path tracer produces unbounded radiance: a light source or a bright specular
    // highlight can be many times brighter than a white surface. Clamping at 1.0 turns
    // all of that into flat white, so the values are compressed into displayable range
    // first.
    //
    // This is the ACES filmic curve as fitted by Krzysztof Narkowicz. It rolls highlights
    // off smoothly rather than clipping them, and keeps saturated bright colours from
    // shifting hue on their way to white.
    const auto curve = [](float x) {
        constexpr float a = 2.51f;
        constexpr float b = 0.03f;
        constexpr float c = 2.43f;
        constexpr float d = 0.59f;
        constexpr float e = 0.14f;

        if (!std::isfinite(x) || x <= 0.0f)
        {
            return 0.0f;
        }

        return std::min(std::max((x * ((a * x) + b)) / ((x * ((c * x) + d)) + e), 0.0f), 1.0f);
    };

    return {curve(linearRgb[0]), curve(linearRgb[1]), curve(linearRgb[2])};
}

auto writePNG(const std::string &path, int width, int height, const float *linearRgb) -> bool
{
    if (linearRgb == nullptr || width <= 0 || height <= 0)
    {
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t numComponents = pixelCount * 3;

    std::vector<unsigned char> bytes(numComponents);
    for (size_t p = 0; p < pixelCount; p++)
    {
        // Tone map first, then apply the sRGB transfer function. Doing it the other way
        // round would compress values that are already perceptually encoded.
        const std::array<float, 3> mapped = toneMap(&linearRgb[p * 3]);

        for (int c = 0; c < 3; c++)
        {
            const float encoded = linearToSrgb(mapped[static_cast<size_t>(c)]);
            bytes[(p * 3) + static_cast<size_t>(c)] = static_cast<unsigned char>((encoded * 255.0f) + 0.5f);
        }
    }

    // The render buffer has row 0 at the bottom; PNG expects row 0 at the top.
    stbi_flip_vertically_on_write(1);
    const int ok = stbi_write_png(path.c_str(), width, height, 3, bytes.data(), width * 3);

    return ok != 0;
}

} // namespace ImageIO
