#pragma once
#include "SceneObject.h"

#include <vector>

struct BoundingBox
{
	float min[3], max[3];
	std::vector<BoundingBox*> children;

	BoundingBox(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
	{
		min[0] = minX; min[1] = minY; min[2] = minZ;
		max[0] = maxX; max[1] = maxY; max[2] = maxZ;
	}
};

class BVH
{
public:
	BVH(std::vector<SceneObject*> objectList);

private:
	unsigned int m_ObjectsInLeaf = 10;
};
