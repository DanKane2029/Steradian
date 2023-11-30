#pragma once
#include <memory>

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
    PixelBuffer(unsigned int width, unsigned int height);
    ~PixelBuffer();

    void setPixel(unsigned int x, unsigned int y, Vec3 pixel);
    auto getPixels() -> float *;

    void resizeBuffer(unsigned int width, unsigned int height);
    auto getSize() -> std::pair<unsigned int, unsigned int>;

  private:
    unsigned int m_Width, m_Height;

    float *m_Buffer;
    PixelMetaData *m_MetaDataBuffer;
};
