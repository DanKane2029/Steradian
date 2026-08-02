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

    /**
     * \brief Replaces the whole buffer with an image that has already been averaged.
     *
     * A backend that accumulates elsewhere -- the GPU keeps its running sum in device
     * memory -- has nothing to gain from being fed one sample at a time, and a great deal
     * to lose: setSample costs about 120 nanoseconds a pixel, which at 1000x800 is a
     * hundred milliseconds. That was six times the cost of the render it was copying,
     * and it capped the interactive frame rate at nine frames a second while the GPU sat
     * idle for most of each one.
     *
     * \param color Width * height * 3 floats of linear radiance.
     * \param albedo Matching surface colour, for the denoiser.
     * \param normal Matching surface normals.
     * \param sampleCount How many samples the average was taken over.
     */
    void setResolved(const float *color, const float *albedo, const float *normal, unsigned int sampleCount);

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
