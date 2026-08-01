#pragma once
#include <cstddef>
#include <utility>

#include "Utils/Vec3.h"

/**
 * a metadata object used for statistics and debuging
 */
struct PixelMetaData
{
    unsigned int numRaysShot = 0;
};

/**
 * a container for the buffer containing the pixel data for the rendered image
 */
class PixelBuffer
{
  public:
    PixelBuffer(int width, int height);
    ~PixelBuffer();

    void setPixel(int x, int y, Vec3 pixel);

    /**
     * \brief Adds a sample together with the surface it came from.
     *
     * The albedo and normal of the first surface a ray struck are averaged alongside the
     * colour, so a denoiser has a guide describing where the real edges in the image are.
     */
    void setSample(int x, int y, Vec3 color, Vec3 albedo, Vec3 normal);

    auto getPixels() -> float *;

    /** base colour of the surface visible at each pixel */
    auto getAlbedo() const -> const float *
    {
        return m_AlbedoBuffer;
    }

    /** surface normal visible at each pixel */
    auto getNormals() const -> const float *
    {
        return m_NormalBuffer;
    }

    void resizeBuffer(int width, int height);
    void clearBuffer();
    auto getSize() -> std::pair<int, int>;

  private:
    /** number of pixels in the buffer */
    auto numPixels() const -> size_t
    {
        return static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height);
    }

    /** number of float components in the color buffer (3 per pixel) */
    auto numColorComponents() const -> size_t
    {
        return numPixels() * 3;
    }

    unsigned int m_Width, m_Height;

    float *m_Buffer;
    float *m_AlbedoBuffer;
    float *m_NormalBuffer;
    PixelMetaData *m_MetaDataBuffer;
};
