#include "PixelBuffer.h"

#include <algorithm>

/**
 * creates a pixel buffer object of a given width and height
 *
 * \param width - the number of columns in the pixel buffer
 * \param height - the number of rows in the pixel buffer
 */
PixelBuffer::PixelBuffer(int width, int height)
    : m_Width(width), m_Height(height), m_Buffer(new float[numColorComponents()]()),
      m_AlbedoBuffer(new float[numColorComponents()]()), m_NormalBuffer(new float[numColorComponents()]()),
      m_MetaDataBuffer(new PixelMetaData[numPixels()])
{
}

/**
 * deletes the pixel data and meta data
 */
PixelBuffer::~PixelBuffer()
{
    delete[] m_Buffer;
    delete[] m_AlbedoBuffer;
    delete[] m_NormalBuffer;
    delete[] m_MetaDataBuffer;
}

/**
 * sets the color of a single pixel in the buffer
 * if a pixel is set/colored multiple times the colors are averaged
 *
 * This performs an unsynchronized read-modify-write, so callers must guarantee a given
 * pixel is only ever written by one thread at a time. The renderer does this by giving
 * each worker thread an exclusive band of rows.
 *
 * \param x - the width of the pixel to be colored
 * \param y - the height of the pixel to be colored
 * \param color - the color as a Vec3 to set the desired pixel
 */
void PixelBuffer::setPixel(int x, int y, Vec3 color)
{
    if (x < 0 || y < 0 || static_cast<unsigned int>(x) >= m_Width || static_cast<unsigned int>(y) >= m_Height)
    {
        return;
    }

    const unsigned int metaDataIndex = (static_cast<unsigned int>(y) * m_Width) + static_cast<unsigned int>(x);
    const unsigned int index = metaDataIndex * 3;

    const Vec3 oldColor = Vec3(m_Buffer[index], m_Buffer[index + 1], m_Buffer[index + 2]);

    const unsigned int numRaysShot = m_MetaDataBuffer[metaDataIndex].numRaysShot;

    const float oldProportion = static_cast<float>(numRaysShot) / static_cast<float>(numRaysShot + 1);
    const float newProportion = 1.0f / static_cast<float>(numRaysShot + 1);

    const Vec3 newColor = (oldColor * oldProportion) + (color * newProportion);

    m_Buffer[index] = newColor.x;
    m_Buffer[index + 1] = newColor.y;
    m_Buffer[index + 2] = newColor.z;

    m_MetaDataBuffer[metaDataIndex].numRaysShot = numRaysShot + 1;
}

/**
 * returns the array of pixels in the buffer
 *
 * \return - the pointer to the first float value of the buffer
 */
void PixelBuffer::setSample(int x, int y, Vec3 color, Vec3 albedo, Vec3 normal)
{
    if (x < 0 || y < 0 || static_cast<unsigned int>(x) >= m_Width || static_cast<unsigned int>(y) >= m_Height)
    {
        return;
    }

    const unsigned int metaDataIndex = (static_cast<unsigned int>(y) * m_Width) + static_cast<unsigned int>(x);
    const unsigned int index = metaDataIndex * 3;

    const unsigned int numRaysShot = m_MetaDataBuffer[metaDataIndex].numRaysShot;

    const float oldProportion = static_cast<float>(numRaysShot) / static_cast<float>(numRaysShot + 1);
    const float newProportion = 1.0f / static_cast<float>(numRaysShot + 1);

    // The guide channels are averaged exactly like the colour, so they stay consistent
    // with it and settle immediately: they barely vary between samples.
    const auto accumulate = [&](float *buffer, const Vec3 &value) {
        buffer[index] = (buffer[index] * oldProportion) + (value.x * newProportion);
        buffer[index + 1] = (buffer[index + 1] * oldProportion) + (value.y * newProportion);
        buffer[index + 2] = (buffer[index + 2] * oldProportion) + (value.z * newProportion);
    };

    accumulate(m_Buffer, color);
    accumulate(m_AlbedoBuffer, albedo);
    accumulate(m_NormalBuffer, normal);

    m_MetaDataBuffer[metaDataIndex].numRaysShot = numRaysShot + 1;
}

auto PixelBuffer::getPixels() -> float *
{
    return m_Buffer;
}

/**
 * changes the size of the pixel buffer
 * this function is called whenever the application window is resized
 * all previously calculated pixel values will be reset
 *
 * \param width - the new pixel buffer width
 * \param height - the new pixel buffer height
 */
void PixelBuffer::resizeBuffer(int width, int height)
{
    m_Width = width;
    m_Height = height;

    delete[] m_Buffer;
    m_Buffer = new float[numColorComponents()]();

    delete[] m_AlbedoBuffer;
    m_AlbedoBuffer = new float[numColorComponents()]();

    delete[] m_NormalBuffer;
    m_NormalBuffer = new float[numColorComponents()]();

    delete[] m_MetaDataBuffer;
    m_MetaDataBuffer = new PixelMetaData[numPixels()];
}

/**
 * resets all accumulated color and sample counts to zero
 */
void PixelBuffer::clearBuffer()
{
    std::fill(m_Buffer, m_Buffer + numColorComponents(), 0.0f);
    std::fill(m_AlbedoBuffer, m_AlbedoBuffer + numColorComponents(), 0.0f);
    std::fill(m_NormalBuffer, m_NormalBuffer + numColorComponents(), 0.0f);
    std::fill(m_MetaDataBuffer, m_MetaDataBuffer + numPixels(), PixelMetaData{});
}

/**
 * returns the current pixel buffer width and height as a pair
 *
 * \return - the <width, height> pair
 */
auto PixelBuffer::getSize() -> std::pair<int, int>
{
    return std::make_pair(static_cast<int>(m_Width), static_cast<int>(m_Height));
}
