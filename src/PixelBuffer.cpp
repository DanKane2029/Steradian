#include "PixelBuffer.h"

PixelBuffer::PixelBuffer(unsigned int width, unsigned int height)
	: m_Width(width), m_Height(height)
{
	m_Buffer.reset(new float[m_Width * m_Height * 3]);
}

PixelBuffer::PixelBuffer(const PixelBuffer& pixelBuffer)
	: m_Width(pixelBuffer.m_Width), m_Height(pixelBuffer.m_Height),
	  m_Buffer(pixelBuffer.m_Buffer)
{
}

PixelBuffer::~PixelBuffer()
{
}

void PixelBuffer::SetPixel(unsigned int x, unsigned int y, float pixel[3])
{
	unsigned int index = (y * m_Width + x) * 3;

	m_Buffer.get()[index]	  = pixel[0];
	m_Buffer.get()[index + 1] = pixel[1];
	m_Buffer.get()[index + 2] = pixel[2];
}

float* PixelBuffer::GetPixels()
{
	return m_Buffer.get();
}

void PixelBuffer::ResizeBuffer(unsigned int width, unsigned int height)
{
	m_Width = width;
	m_Height = height;
	m_Buffer.reset(new float[m_Width * m_Height * 3]);
}
