#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Utils/stb_image.h"

#include <algorithm>
#include <cmath>
#include <iostream>

Texture::Texture(const std::string &path)
{
    // stbi_loadf converts sRGB image data to linear on load, which is what the renderer
    // wants: shading happens in linear space and the sRGB encode is applied on output.
    float *pixels = stbi_loadf(path.c_str(), &m_Width, &m_Height, &m_NumChannels, 0);

    if (pixels == nullptr)
    {
        std::cerr << "Could not load texture '" << path << "': " << stbi_failure_reason() << std::endl;
        m_Width = 0;
        m_Height = 0;
        m_NumChannels = 0;
        return;
    }

    const size_t count =
        static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height) * static_cast<size_t>(m_NumChannels);
    m_Data.assign(pixels, pixels + count);

    stbi_image_free(pixels);
}

auto Texture::getTexel(float u, float v) const -> Vec3
{
    if (m_Data.empty() || m_NumChannels < 1)
    {
        return {};
    }

    // Wrap into [0, 1). std::fmod keeps the sign of its input, so negative coordinates
    // need a further shift before being scaled to pixels.
    u = std::fmod(u, 1.0f);
    v = std::fmod(v, 1.0f);
    if (u < 0.0f)
    {
        u += 1.0f;
    }
    if (v < 0.0f)
    {
        v += 1.0f;
    }

    auto px = static_cast<int>(u * static_cast<float>(m_Width));
    auto py = static_cast<int>(v * static_cast<float>(m_Height));

    // Guard the upper edge: a u just below 1.0 can still round up to width once scaled.
    px = std::min(std::max(px, 0), m_Width - 1);
    py = std::min(std::max(py, 0), m_Height - 1);

    const size_t index = ((static_cast<size_t>(py) * static_cast<size_t>(m_Width)) + static_cast<size_t>(px)) *
                         static_cast<size_t>(m_NumChannels);

    // Greyscale images have a single channel; replicate it across RGB.
    if (m_NumChannels < 3)
    {
        const float grey = m_Data[index];
        return {grey, grey, grey};
    }

    return {m_Data[index], m_Data[index + 1], m_Data[index + 2]};
}
