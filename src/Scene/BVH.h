#pragma once
#include "SceneObject.h"

#include "BoundingBox.h"

#include <vector>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <limits>

/**
 * bounding volume heirarchy (octree) used as an acceleration structure
 * to speed up ray-scene intersection tests
 */
class BVH
{
public:
	BVH(std::vector<SceneObject*>& objectList);

private:
	void PartitionSpace(std::vector<SceneObject*>& objectList, std::vector<BoundingBox*>& partitionedSpaces);
	BoundingBox EncapsulateObjects(std::vector<SceneObject*>& objectList);

public:
	std::vector<BoundingBox> m_BoundingBoxes;

private:
	std::vector<SceneObject*>& m_OjectList;
	unsigned int m_ObjectsInLeaf = 10;
};
