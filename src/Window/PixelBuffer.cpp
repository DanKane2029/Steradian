#include "PixelBuffer.h"

#include <iostream>

/**
 * creates a pixel buffer object of a given width and height
 *
 * \param width - the number of columns in the pixel buffer
 * \param height - the number of rows in the pixel buffer
 */
PixelBuffer::PixelBuffer(unsigned int width, unsigned int height)
    : m_Width(width), m_Height(height), m_Buffer(new float[m_Width * m_Height * 3]),
      m_MetaDataBuffer(new PixelMetaData[m_Width * m_Height])
{
}

/**
 * deletes the pixel data and meta data
 */
PixelBuffer::~PixelBuffer()
{
    delete[] m_Buffer;
    delete[] m_MetaDataBuffer;
}

/**
 * sets the color of a single pixel in the buffer
 * if a pixel is set/colored multiple times the colors are averaged
 *
 * \param x - the width of the pixel to be colored
 * \param y - the height of the pixel to be colored
 * \param color - the color as a Vec3 to set the desired pixel
 */
void PixelBuffer::setPixel(unsigned int x, unsigned int y, Vec3 color)
{
    unsigned int metaDataIndex = (y * m_Width + x);
    unsigned int index = metaDataIndex * 3;
    Vec3 oldColor = Vec3(m_Buffer[index], m_Buffer[index + 1], m_Buffer[index + 2]);

    unsigned int numRaysShot = m_MetaDataBuffer[metaDataIndex].numRaysShot;
    unsigned int newNumRaysShot = numRaysShot + 1;

    float oldProportion = (float)numRaysShot / (float)(numRaysShot + 1.0f);
    float newProportion = 1.0f / (float)(numRaysShot + 1.0f);

    Vec3 newColor = (oldColor * oldProportion) + (color * newProportion);

    m_Buffer[index] = newColor.v[0];
    m_Buffer[index + 1] = newColor.v[1];
    m_Buffer[index + 2] = newColor.v[2];

    m_MetaDataBuffer[metaDataIndex].numRaysShot++;
}

/**
 * returns the array of pixels in the buffer
 *
 * \return - the pointer to the first float value of the buffer
 */
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
void PixelBuffer::resizeBuffer(unsigned int width, unsigned int height)
{
    m_Width = width, m_Height = height;
    unsigned int size = m_Width * m_Height * 3;

    delete[] m_Buffer;
    m_Buffer = new float[size];

    delete[] m_MetaDataBuffer;
    m_MetaDataBuffer = new PixelMetaData[size];
}

/**
 * returns the current pixel buffer width and height as a pair
 *
 * \return - the <width, height> pair
 */
auto PixelBuffer::getSize() -> std::pair<unsigned int, unsigned int>
{
    return std::make_pair(m_Width, m_Height);
}
