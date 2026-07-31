#include "ImageIO.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Utils/stb_image_write.h"

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

auto writePNG(const std::string &path, int width, int height, const float *linearRgb) -> bool
{
    if (linearRgb == nullptr || width <= 0 || height <= 0)
    {
        return false;
    }

    const size_t numComponents = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;

    std::vector<unsigned char> bytes(numComponents);
    for (size_t i = 0; i < numComponents; i++)
    {
        const float encoded = linearToSrgb(linearRgb[i]);
        bytes[i] = static_cast<unsigned char>((encoded * 255.0f) + 0.5f);
    }

    // The render buffer has row 0 at the bottom; PNG expects row 0 at the top.
    stbi_flip_vertically_on_write(1);
    const int ok = stbi_write_png(path.c_str(), width, height, 3, bytes.data(), width * 3);

    return ok != 0;
}

} // namespace ImageIO
