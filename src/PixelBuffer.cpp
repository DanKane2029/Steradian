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

void PixelBuffer::SetPixel(unsigned int x, unsigned int y, float pixel[3])
{
	unsigned int index = (y * m_Width + x) * 3;

	m_Buffer[index]	  = pixel[0];
	m_Buffer[index + 1] = pixel[1];
	m_Buffer[index + 2] = pixel[2];

	m_NumSetPixels++;
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
	memset(m_Buffer, 0.0f, size);

	m_NumSetPixels = 0;
}
