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
    PixelBuffer(int width, int height);
    ~PixelBuffer();

    void setPixel(int x, int y, Vec3 pixel);
    auto getPixels() -> float *;

    void resizeBuffer(int width, int height);
    void clearBuffer();
    auto getSize() -> std::pair<int, int>;

  private:
    unsigned int m_Width, m_Height;

    float *m_Buffer;
    PixelMetaData *m_MetaDataBuffer;
};
