#pragma once
#include <algorithm>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <vector>

#include "BoundingBox.h"
#include "SceneObject.h"

struct BVHNode
{
    BoundingBox boundingBox;
    std::vector<std::shared_ptr<BVHNode>> children;
    bool isLeaf;
    std::vector<std::shared_ptr<SceneObject>> sceneObjects;

    BVHNode() : isLeaf(false)
    {
    }

    Hit rayIntersect(Ray ray)
    {
        Hit hit;

        if (isLeaf)
        {
            for (std::shared_ptr<SceneObject> so : sceneObjects)
            {
                Hit curHit = so->rayIntersect(ray);
                if (curHit.isHit && hit.time > curHit.time)
                {
                    hit = curHit;
                }
            }
        }
        else
        {
            for (std::shared_ptr<BVHNode> child : children)
            {
                Hit curHit = child->rayIntersect(ray);
                if (curHit.isHit && hit.time > curHit.time)
                {
                    hit = curHit;
                }
            }
        }

        return hit;
    }
};

/**
 * bounding volume heirarchy (octree) used as an acceleration structure
 * to speed up ray-scene intersection tests
 */
class BVH
{
  public:
    BVH(std::vector<std::shared_ptr<SceneObject>> objectList, unsigned int objectsInLeaf = 10);
    std::shared_ptr<BVHNode> root;

  private:
    void partitionSpace(std::shared_ptr<BVHNode> root, std::vector<std::shared_ptr<SceneObject>> objectList);
    auto encapsulateObjects(std::vector<std::shared_ptr<SceneObject>> objectList) -> BoundingBox;
    auto filterObjects(std::shared_ptr<BVHNode>, std::vector<std::shared_ptr<SceneObject>> objectList)
        -> std::vector<std::shared_ptr<SceneObject>>;

  public:
    std::vector<BoundingBox> m_BoundingBoxes{};

  private:
    std::vector<std::shared_ptr<SceneObject>> m_OjectList{};
    unsigned int m_ObjectsInLeaf;
};
