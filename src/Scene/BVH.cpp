#include "BVH.h"

/**
 * creates an actree bounding volume heirarchy
 * 
 * \param objectList - the list of objects that make up the BVH
 */
BVH::BVH(std::vector<SceneObject*>& objectList)
	: m_OjectList(objectList)
{
	std::cout << "Creating Acceleration Structure..." << std::endl;
	std::vector<BoundingBox*> partitionedSpaces;
	PartitionSpace(m_OjectList, partitionedSpaces);

}

/**
 * splits a list of scene objects into 8 axis aligned bounding boxes
 * the bounding boxes are split along axises that are the medians x, y, & z
 * values of the center points of the scene objects
 * 
 * \param objectList - the vector of scene objects the split
 * \param partitionedSpaces - the list of 
 */
void BVH::PartitionSpace(std::vector<SceneObject*>& objectList, std::vector<BoundingBox*>& partitionedSpaces)
{
	std::vector<Vec3> centerPoints;
	std::transform(objectList.begin(), objectList.end(), std::back_inserter(centerPoints), [](SceneObject* sceneObject) {
		return sceneObject->GetCenterPoint();
	});

	// get medians in x direction
	std::sort(centerPoints.begin(), centerPoints.end(), [](Vec3 v, Vec3 u) { return v.v[0] < u.v[0]; });
	float xMedian = centerPoints[centerPoints.size() / 2].v[0];

	// get medians in y direction
	std::sort(centerPoints.begin(), centerPoints.end(), [](Vec3 v, Vec3 u) { return v.v[1] < u.v[1]; });
	float yMedian = centerPoints[centerPoints.size() / 2].v[1];

	// get medians in z direction
	std::sort(centerPoints.begin(), centerPoints.end(), [](Vec3 v, Vec3 u) { return v.v[2] < u.v[2]; });
	float zMedian = centerPoints[centerPoints.size() / 2].v[2];

	// TODO: split object list into 8 different bounding boxes using x, y, z medians
}

/**
 * encasulates a list of scene objects into a bounding box by finding the 
 * minimum and maximum x, y, & z values of their own bounding boxes
 * 
 * \param objectList - the list of scene objects to encapsulate
 * \return - the bounding box encapsulating the scene objects
 */
BoundingBox BVH::EncapsulateObjects(std::vector<SceneObject*>& objectList)
{
	constexpr float FLOAT_MIN = std::numeric_limits<float>::min();
	constexpr float FLOAT_MAX = std::numeric_limits<float>::max();

	float minX = FLOAT_MAX, minY = FLOAT_MAX, minZ = FLOAT_MAX;
	float maxX = FLOAT_MIN, maxY = FLOAT_MIN, maxZ = FLOAT_MIN;

	// Get bounding box of each scene object to find bounding box of all objects
	for (SceneObject* so : objectList)
	{
		BoundingBox bb = so->GetBoundingBox();

		minX = std::min(minX, bb.min[0]);
		minY = std::min(minY, bb.min[1]);
		minZ = std::min(minZ, bb.min[2]);

		maxX = std::max(maxX, bb.max[0]);
		maxY = std::max(maxY, bb.max[1]);
		maxZ = std::max(maxZ, bb.max[2]);
	}

	return BoundingBox(minX, minY, minZ, maxX, maxY, maxZ);
}
