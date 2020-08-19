#include "PixelBuffer.h"

PixelBuffer::PixelBuffer(unsigned int width, unsigned int height)
	: m_Width(width), m_Height(height)
{
	m_Buffer = new float[m_Width * m_Height * 3];
}

PixelBuffer::~PixelBuffer()
{
	delete[] m_Buffer;
}

void PixelBuffer::SetPixel(unsigned int x, unsigned int y, Vec3 pixel)
{
	unsigned int index = (y * m_Width + x) * 3;

	m_Buffer[index]	    = pixel.v[0];
	m_Buffer[index + 1] = pixel.v[1];
	m_Buffer[index + 2] = pixel.v[2];
}

float* PixelBuffer::GetPixels()
{
	return m_Buffer;
}

void PixelBuffer::ResizeBuffer(unsigned int width, unsigned int height)
{
	m_Width = width, m_Height = height;
	unsigned int size = m_Width * m_Height * 3;

	delete[] m_Buffer;
	m_Buffer = new float[size];
}

std::pair<unsigned int, unsigned int> PixelBuffer::GetSize()
{
	return std::make_pair(m_Width, m_Height);
}
