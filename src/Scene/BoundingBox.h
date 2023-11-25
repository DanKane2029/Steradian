#pragma once
#include <vector>

/**
 * An axis aligned bounging box used for fast intersection testing
 */
struct BoundingBox
{
	float min[3], max[3];
	std::vector<BoundingBox*> children;

	BoundingBox()
	{
		min[0] = 0.0f; min[1] = 0.0f; min[2] = 0.0f;
		max[0] = 0.0f; max[1] = 0.0f; max[2] = 0.0f;
	}

	BoundingBox(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
	{
		min[0] = minX; min[1] = minY; min[2] = minZ;
		max[0] = maxX; max[1] = maxY; max[2] = maxZ;
	}
};
