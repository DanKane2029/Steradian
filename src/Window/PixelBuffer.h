#pragma once
#include "Vec3.h"

#include <Memory>

struct PixelMetaData
{
	unsigned int numRaysShot = 0;
};

class PixelBuffer
{
public:
	PixelBuffer(unsigned int width, unsigned int height);
	~PixelBuffer();

	void SetPixel(unsigned int x, unsigned int y, Vec3 pixel);
	float* GetPixels();

	void ResizeBuffer(unsigned int width, unsigned int height);
	std::pair<unsigned int, unsigned int> GetSize();

private:
	unsigned int m_Width, m_Height;

	float* m_Buffer;
	PixelMetaData* m_MetaDataBuffer;
};
