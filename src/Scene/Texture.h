#pragma once

#include "Utils/Vec3.h"

#include <string>
#include <vector>

/**
 * \brief An image sampled by materials.
 *
 * Pixel data is held in a vector rather than a raw stbi allocation. The previous raw
 * pointer had no destructor, so every loaded texture leaked, and no copy constructor, so
 * copying a Material shallow-copied the pointer and left two owners of one buffer.
 */
class Texture
{
  public:
    Texture() = default;

    /**
     * \brief Loads a texture from an image file.
     *
     * A file that cannot be read leaves the texture empty rather than throwing, so a
     * scene referencing a bad texture path still renders.
     */
    explicit Texture(const std::string &path);

    /** true when pixel data was loaded successfully */
    auto isValid() const -> bool
    {
        return !m_Data.empty();
    }

    auto getWidth() const -> int
    {
        return m_Width;
    }

    auto getHeight() const -> int
    {
        return m_Height;
    }

    /**
     * \brief Samples the texture at the given coordinates.
     *
     * Coordinates wrap, so values at or outside [0, 1] tile rather than reading past the
     * end of the buffer as they previously did.
     *
     * \param u Horizontal coordinate.
     * \param v Vertical coordinate.
     * \returns The colour at that point, or black when the texture is empty.
     */
    auto getTexel(float u, float v) const -> Vec3;

  private:
    int m_Width = 0;
    int m_Height = 0;
    int m_NumChannels = 0;
    std::vector<float> m_Data;
};
