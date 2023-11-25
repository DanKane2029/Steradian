#pragma once
#include "SceneObject.h"

#include "BoundingBox.h"

#include <vector>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <limits>
#include <memory>

/**
 * bounding volume heirarchy (octree) used as an acceleration structure
 * to speed up ray-scene intersection tests
 */
class BVH
{
public:
	BVH(std::vector<std::shared_ptr<SceneObject>> objectList);

private:
	void PartitionSpace(std::vector<std::shared_ptr<SceneObject>> objectList, std::vector<BoundingBox*>& partitionedSpaces);
	BoundingBox EncapsulateObjects(std::vector<std::shared_ptr<SceneObject>> objectList);

public:
	std::vector<BoundingBox> m_BoundingBoxes;

private:
	std::vector<std::shared_ptr<SceneObject>> m_OjectList;
	unsigned int m_ObjectsInLeaf = 10;
};
