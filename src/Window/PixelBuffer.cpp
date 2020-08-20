#include "PixelBuffer.h"

#include <iostream>

PixelBuffer::PixelBuffer(unsigned int width, unsigned int height)
	: m_Width(width), m_Height(height)
{
	m_Buffer = new float[m_Width * m_Height * 3];
	m_MetaDataBuffer = new PixelMetaData[m_Width * m_Height];
}

PixelBuffer::~PixelBuffer()
{
	delete[] m_Buffer;
	delete[] m_MetaDataBuffer;
}

void PixelBuffer::SetPixel(unsigned int x, unsigned int y, Vec3 color)
{
	unsigned int metaDataIndex = (y * m_Width + x);
	unsigned int index = metaDataIndex * 3;
	Vec3 oldColor = Vec3(m_Buffer[index], m_Buffer[index + 1], m_Buffer[index + 2]);

	unsigned int numRaysShot = m_MetaDataBuffer[metaDataIndex].numRaysShot;
	unsigned int newNumRaysShot = numRaysShot + 1;

	float oldProportion = (float) numRaysShot / (float) (numRaysShot + 1.0f);
	float newProportion = 1.0f / (float)(numRaysShot + 1.0f);

	Vec3 newColor = (oldColor * oldProportion) + (color * newProportion);

	m_Buffer[index]     = newColor.v[0];
	m_Buffer[index + 1] = newColor.v[1];
	m_Buffer[index + 2] = newColor.v[2];

	m_MetaDataBuffer[metaDataIndex].numRaysShot++;
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

	delete[] m_MetaDataBuffer;
	m_MetaDataBuffer = new PixelMetaData[size];
}

std::pair<unsigned int, unsigned int> PixelBuffer::GetSize()
{
	return std::make_pair(m_Width, m_Height);
}
