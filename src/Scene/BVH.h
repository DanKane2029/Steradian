#pragma once
#include <algorithm>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <vector>

#include "BoundingBox.h"
#include "SceneObject.h"

/**
 * bounding volume heirarchy (octree) used as an acceleration structure
 * to speed up ray-scene intersection tests
 */
class BVH
{
  public:
    BVH(std::vector<std::shared_ptr<SceneObject>> objectList);

  private:
    void partitionSpace(std::vector<std::shared_ptr<SceneObject>> objectList,
                        std::vector<BoundingBox *> &partitionedSpaces);
    auto encapsulateObjects(std::vector<std::shared_ptr<SceneObject>> objectList) -> BoundingBox;

  public:
    std::vector<BoundingBox> m_BoundingBoxes{};

  private:
    std::vector<std::shared_ptr<SceneObject>> m_OjectList{};
    unsigned int m_ObjectsInLeaf = 10;
};
