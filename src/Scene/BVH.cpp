#include "BVH.h"

#include <cmath>

/**
 * creates an actree bounding volume heirarchy
 *
 * \param objectList - the list of objects that make up the BVH
 */
BVH::BVH(std::vector<std::shared_ptr<SceneObject>> objectList, unsigned int objectsInLeaf)
    : m_OjectList(objectList), m_ObjectsInLeaf(objectsInLeaf)
{
    std::cout << "Creating Acceleration Structure from " << objectList.size() << " objects." << std::endl;
    root = std::make_shared<BVHNode>();
    partitionSpace(root, m_OjectList);
}

/**
 * splits a list of scene objects into 8 axis aligned bounding boxes
 * the bounding boxes are split along axises that are the medians x, y, & z
 * values of the center points of the scene objects
 *
 * \param objectList - the vector of scene objects the split
 * \param partitionedSpaces - the list of
 */
void BVH::partitionSpace(std::shared_ptr<BVHNode> node, std::vector<std::shared_ptr<SceneObject>> objectList)
{
    if (objectList.size() <= m_ObjectsInLeaf)
    {
        // stop recursion
        node->isLeaf = true;
        node->sceneObjects = objectList;
    }
    else
    {
        node->boundingBox = encapsulateObjects(objectList);

        std::vector<Vec3> centerPoints;
        std::transform(objectList.begin(), objectList.end(), std::back_inserter(centerPoints),
                       [](std::shared_ptr<SceneObject> sceneObject) { return sceneObject->getCenterPoint(); });

        // get medians in x direction
        std::sort(centerPoints.begin(), centerPoints.end(), [](Vec3 v, Vec3 u) { return v.x < u.x; });
        float xMedian = centerPoints[centerPoints.size() / 2].x;

        // get medians in y direction
        std::sort(centerPoints.begin(), centerPoints.end(), [](Vec3 v, Vec3 u) { return v.y < u.y; });
        float yMedian = centerPoints[centerPoints.size() / 2].y;

        // get medians in z direction
        std::sort(centerPoints.begin(), centerPoints.end(), [](Vec3 v, Vec3 u) { return v.z < u.z; });
        float zMedian = centerPoints[centerPoints.size() / 2].z;

        node->children = std::vector<std::shared_ptr<BVHNode>>(8);

        node->children[0] = std::make_shared<BVHNode>();
        node->children[0]->boundingBox = BoundingBox(node->boundingBox.minX, xMedian, node->boundingBox.minY, yMedian,
                                                     node->boundingBox.minZ, zMedian);

        node->children[1] = std::make_shared<BVHNode>();
        node->children[1]->boundingBox = BoundingBox(xMedian, node->boundingBox.maxX, node->boundingBox.minY, yMedian,
                                                     node->boundingBox.minZ, zMedian);

        node->children[2] = std::make_shared<BVHNode>();
        node->children[2]->boundingBox = BoundingBox(node->boundingBox.minX, xMedian, yMedian, node->boundingBox.maxY,
                                                     node->boundingBox.minZ, zMedian);

        node->children[3] = std::make_shared<BVHNode>();
        node->children[3]->boundingBox = BoundingBox(xMedian, node->boundingBox.maxX, yMedian, node->boundingBox.maxY,
                                                     node->boundingBox.minZ, zMedian);

        node->children[4] = std::make_shared<BVHNode>();
        node->children[4]->boundingBox = BoundingBox(node->boundingBox.minX, xMedian, node->boundingBox.minY, yMedian,
                                                     zMedian, node->boundingBox.maxZ);

        node->children[5] = std::make_shared<BVHNode>();
        node->children[5]->boundingBox = BoundingBox(xMedian, node->boundingBox.maxX, node->boundingBox.minY, yMedian,
                                                     zMedian, node->boundingBox.maxZ);

        node->children[6] = std::make_shared<BVHNode>();
        node->children[6]->boundingBox = BoundingBox(node->boundingBox.minX, xMedian, yMedian, node->boundingBox.maxY,
                                                     zMedian, node->boundingBox.maxZ);

        node->children[7] = std::make_shared<BVHNode>();
        node->children[7]->boundingBox = BoundingBox(xMedian, node->boundingBox.maxX, yMedian, node->boundingBox.maxY,
                                                     zMedian, node->boundingBox.maxZ);

        std::vector<std::shared_ptr<SceneObject>> filteredObjectList;
        for (std::shared_ptr<BVHNode> child : node->children)
        {
            filteredObjectList = filterObjects(child, objectList);
            partitionSpace(child, child->sceneObjects);
        }
    }
}

auto BVH::filterObjects(std::shared_ptr<BVHNode> node, std::vector<std::shared_ptr<SceneObject>> objectList)
    -> std::vector<std::shared_ptr<SceneObject>>
{
    std::vector<std::shared_ptr<SceneObject>> filteredObjectList;
    for (std::shared_ptr<SceneObject> object : objectList)
    {
        Vec3 centerPoint = object->getCenterPoint();
        if (node->boundingBox.inside(centerPoint))
        {
            node->sceneObjects.push_back(object);
        }
        else
        {
            filteredObjectList.push_back(object);
        }
    }

    return filteredObjectList;
}

/**
 * encasulates a list of scene objects into a bounding box by finding the
 * minimum and maximum x, y, & z values of their own bounding boxes
 *
 * \param objectList - the list of scene objects to encapsulate
 * \return - the bounding box encapsulating the scene objects
 */
auto BVH::encapsulateObjects(std::vector<std::shared_ptr<SceneObject>> objectList) -> BoundingBox
{
    const float floatMin = std::numeric_limits<float>::min();
    const float floatMax = std::numeric_limits<float>::max();

    float minX = floatMax;
    float minY = floatMax;
    float minZ = floatMax;

    float maxX = floatMin;
    float maxY = floatMin;
    float maxZ = floatMin;

    // Get bounding box of each scene object to find bounding box of all objects
    for (std::shared_ptr<SceneObject> so : objectList)
    {
        minX = std::min(minX, so->getMinX());
        minY = std::min(minY, so->getMinY());
        minZ = std::min(minZ, so->getMinZ());

        maxX = std::max(maxX, so->getMaxX());
        maxY = std::max(maxY, so->getMaxY());
        maxZ = std::max(maxZ, so->getMaxZ());
    }

    return {minX, maxX, minY, maxY, minZ, maxZ};
}
