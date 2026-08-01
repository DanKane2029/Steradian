#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "AABB.h"
#include "RayTracer/Hit.h"
#include "RayTracer/Ray.h"
#include "SceneObject.h"

/**
 * \brief A node in the flattened bounding volume hierarchy.
 *
 * Nodes live in one contiguous array. The left child of an interior node is always the
 * next entry, so only the right child index has to be stored, which keeps the node at 32
 * bytes.
 */
struct BVHNode
{
    AABB bounds;

    /** interior: index of the right child. leaf: first index into the primitive order */
    uint32_t rightOrFirst = 0;

    /** number of primitives; zero marks an interior node */
    uint16_t count = 0;

    /** axis this node was split on, used to descend near-to-far */
    uint8_t axis = 0;

    uint8_t pad = 0;

    auto isLeaf() const -> bool
    {
        return count > 0;
    }
};

/**
 * \brief Bounding volume hierarchy over the scene's primitives.
 *
 * Built with a binned surface area heuristic and traversed iteratively, descending the
 * nearer child first and shrinking the ray's far bound on every hit, so subtrees lying
 * beyond the closest intersection found so far are skipped entirely.
 *
 * This replaces a median-split octree that never tested a bounding box while traversing.
 * That structure visited every node and every primitive on every ray, and because
 * primitives were assigned to children by centroid they were duplicated across children,
 * so it performed more intersection tests than a brute force loop over the whole scene.
 */
class BVH
{
  public:
    /**
     * \brief Builds a hierarchy over a list of scene objects.
     *
     * The BVH stores raw pointers to objects that remain owned by the Scene, so it must
     * be rebuilt if the scene's object list changes.
     *
     * \param objectList The primitives to organize.
     * \param objectsInLeaf Maximum primitives per leaf.
     */
    explicit BVH(const std::vector<std::shared_ptr<SceneObject>> &objectList, unsigned int objectsInLeaf = 2);

    /**
     * \brief Finds the closest intersection along a ray.
     *
     * \param ray The ray, whose tMin and tMax bound the search.
     * \returns The closest hit, or a Hit with isHit false.
     */
    auto intersect(Ray ray) const -> Hit;

    /**
     * \brief Reports whether anything blocks the ray within its interval.
     *
     * Returns as soon as one intersection is found rather than continuing to look for the
     * closest. Shadow rays only need to know whether the light is visible.
     */
    auto isOccluded(const Ray &ray) const -> bool;

    auto nodeCount() const -> size_t
    {
        return m_Nodes.size();
    }

    auto maxDepth() const -> unsigned int
    {
        return m_MaxDepth;
    }

    auto primitiveCount() const -> size_t
    {
        return m_Primitives.size();
    }

  private:
    /**
     * \brief Recursively builds the subtree covering primitives [start, end).
     *
     * \returns The index of the node that was created.
     */
    auto buildNode(size_t start, size_t end, unsigned int depth) -> uint32_t;

    /**
     * \brief Chooses a split using a binned surface area heuristic.
     *
     * \param start First primitive in the range.
     * \param end One past the last primitive.
     * \param centroidBounds Bounds of the range's centroids.
     * \param axis Set to the chosen axis.
     * \param splitIndex Set to the partition point.
     * \returns True when splitting is cheaper than making a leaf.
     */
    auto findSplit(size_t start, size_t end, const AABB &centroidBounds, int &axis, size_t &splitIndex) -> bool;

    /** bounds over a range of the current primitive order */
    auto boundsOf(size_t start, size_t end) const -> AABB;

    /** buckets considered per axis when evaluating candidate splits */
    static constexpr int numBuckets = 12;

    /** cost of visiting a node relative to intersecting one primitive */
    static constexpr float traversalCost = 0.125f;

    /** guards against unbounded recursion on degenerate input, e.g. coincident centroids */
    static constexpr unsigned int maxBuildDepth = 64;

    std::vector<BVHNode> m_Nodes;

    /** primitives in traversal order; leaves index contiguous ranges of this */
    std::vector<SceneObject *> m_Primitives;

    /** cached per-primitive bounds and centroids, used only while building */
    std::vector<AABB> m_PrimBounds;
    std::vector<Vec3> m_PrimCentroids;

    unsigned int m_ObjectsInLeaf;
    unsigned int m_MaxDepth = 0;
};
