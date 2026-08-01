#include "BVH.h"

#include "Utils/Stats.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

BVH::BVH(const std::vector<std::shared_ptr<SceneObject>> &objectList, unsigned int objectsInLeaf)
    : m_ObjectsInLeaf(std::max(1U, objectsInLeaf))
{
    std::cout << "Building BVH over " << objectList.size() << " primitives." << std::endl;

    m_Primitives.reserve(objectList.size());
    m_PrimBounds.reserve(objectList.size());
    m_PrimCentroids.reserve(objectList.size());

    for (const std::shared_ptr<SceneObject> &object : objectList)
    {
        AABB bounds;
        bounds.expand(Vec3(object->getMinX(), object->getMinY(), object->getMinZ()));
        bounds.expand(Vec3(object->getMaxX(), object->getMaxY(), object->getMaxZ()));

        m_Primitives.push_back(object.get());
        m_PrimBounds.push_back(bounds);
        m_PrimCentroids.push_back(bounds.centroid());
    }

    if (m_Primitives.empty())
    {
        return;
    }

    // A balanced hierarchy over n primitives needs about 2n-1 nodes.
    m_Nodes.reserve((2 * m_Primitives.size()) + 1);

    buildNode(0, m_Primitives.size(), 0);

    // The per-primitive caches are only needed during the build.
    m_PrimBounds.clear();
    m_PrimBounds.shrink_to_fit();
    m_PrimCentroids.clear();
    m_PrimCentroids.shrink_to_fit();

    std::cout << "  " << m_Nodes.size() << " nodes, max depth " << m_MaxDepth << std::endl;
}

auto BVH::boundsOf(size_t start, size_t end) const -> AABB
{
    AABB bounds;

    for (size_t i = start; i < end; i++)
    {
        bounds.expand(m_PrimBounds[i]);
    }

    return bounds;
}

auto BVH::findSplit(size_t start, size_t end, const AABB &centroidBounds, int &axis, size_t &splitIndex) -> bool
{
    const size_t count = end - start;

    axis = centroidBounds.maxExtentAxis();

    const float extent = AABB::axisValue(centroidBounds.extent(), axis);
    if (extent <= 0.0f)
    {
        // Every centroid coincides along the widest axis, so no split plane separates
        // them. Falling through to a median partition keeps the recursion terminating.
        return false;
    }

    const float lower = AABB::axisValue(centroidBounds.min, axis);
    const float scale = static_cast<float>(numBuckets) / extent;

    struct Bucket
    {
        size_t count = 0;
        AABB bounds;
    };

    std::array<Bucket, numBuckets> buckets;

    const auto bucketOf = [&](size_t i) {
        const float offset = AABB::axisValue(m_PrimCentroids[i], axis) - lower;
        const auto b = static_cast<int>(offset * scale);
        return std::min(std::max(b, 0), numBuckets - 1);
    };

    for (size_t i = start; i < end; i++)
    {
        Bucket &bucket = buckets[bucketOf(i)];
        bucket.count++;
        bucket.bounds.expand(m_PrimBounds[i]);
    }

    // Sweep from both ends so each candidate split's two halves are known in linear time.
    std::array<float, numBuckets - 1> leftArea{};
    std::array<size_t, numBuckets - 1> leftCount{};
    {
        AABB acc;
        size_t n = 0;
        for (int i = 0; i < numBuckets - 1; i++)
        {
            acc.expand(buckets[i].bounds);
            n += buckets[i].count;
            leftArea[i] = acc.surfaceArea();
            leftCount[i] = n;
        }
    }

    float bestCost = std::numeric_limits<float>::max();
    int bestSplit = -1;
    {
        AABB acc;
        size_t n = 0;
        for (int i = numBuckets - 1; i > 0; i--)
        {
            acc.expand(buckets[i].bounds);
            n += buckets[i].count;

            const size_t nLeft = leftCount[i - 1];
            if (nLeft == 0 || n == 0)
            {
                continue;
            }

            // Surface area heuristic: the expected cost of a split is the chance a ray
            // enters each child, proportional to its surface area, times the work it
            // would do there.
            const float cost =
                (leftArea[i - 1] * static_cast<float>(nLeft)) + (acc.surfaceArea() * static_cast<float>(n));

            if (cost < bestCost)
            {
                bestCost = cost;
                bestSplit = i;
            }
        }
    }

    if (bestSplit < 0)
    {
        return false;
    }

    const AABB nodeBounds = boundsOf(start, end);
    const float parentArea = nodeBounds.surfaceArea();

    // Compare against simply making a leaf. Normalizing by the parent's area turns the
    // accumulated areas above into probabilities.
    const float splitCost = traversalCost + (parentArea > 0.0f ? bestCost / parentArea : bestCost);
    const auto leafCost = static_cast<float>(count);

    if (count <= m_ObjectsInLeaf && leafCost <= splitCost)
    {
        return false;
    }

    // Partition in place, keeping the parallel bounds and centroid arrays in step, so
    // each child ends up owning a contiguous range and no allocation happens per node.
    size_t mid = start;
    for (size_t i = start; i < end; i++)
    {
        if (bucketOf(i) < bestSplit)
        {
            if (i != mid)
            {
                std::swap(m_Primitives[i], m_Primitives[mid]);
                std::swap(m_PrimBounds[i], m_PrimBounds[mid]);
                std::swap(m_PrimCentroids[i], m_PrimCentroids[mid]);
            }
            mid++;
        }
    }

    // A split that leaves one side empty makes no progress and would recurse forever.
    if (mid == start || mid == end)
    {
        return false;
    }

    splitIndex = mid;
    return true;
}

auto BVH::buildNode(size_t start, size_t end, unsigned int depth) -> uint32_t
{
    m_MaxDepth = std::max(m_MaxDepth, depth);

    const auto nodeIndex = static_cast<uint32_t>(m_Nodes.size());
    m_Nodes.emplace_back();

    const size_t count = end - start;

    // Bounds are taken from the primitives the node actually contains, not from a spatial
    // subdivision of the parent. The previous octree used partition planes as node bounds
    // while assigning primitives by centroid, so a primitive straddling a plane stuck out
    // of its own node's box.
    AABB nodeBounds = boundsOf(start, end);

    const auto makeLeaf = [&]() {
        BVHNode &node = m_Nodes[nodeIndex];
        node.bounds = nodeBounds;
        node.rightOrFirst = static_cast<uint32_t>(start);
        node.count = static_cast<uint16_t>(std::min<size_t>(count, std::numeric_limits<uint16_t>::max()));
        node.axis = 0;
    };

    if (count <= 1 || depth >= maxBuildDepth)
    {
        makeLeaf();
        return nodeIndex;
    }

    AABB centroidBounds;
    for (size_t i = start; i < end; i++)
    {
        centroidBounds.expand(m_PrimCentroids[i]);
    }

    int axis = 0;
    size_t splitIndex = 0;

    if (!findSplit(start, end, centroidBounds, axis, splitIndex))
    {
        // No worthwhile split. Fall back to a median partition when the range is still
        // too large for one leaf, so degenerate geometry cannot produce a huge leaf.
        if (count <= m_ObjectsInLeaf)
        {
            makeLeaf();
            return nodeIndex;
        }

        splitIndex = start + (count / 2);
        axis = centroidBounds.maxExtentAxis();
    }

    const uint32_t leftIndex = buildNode(start, splitIndex, depth + 1);
    (void)leftIndex; // always nodeIndex + 1 by construction

    const uint32_t rightIndex = buildNode(splitIndex, end, depth + 1);

    BVHNode &node = m_Nodes[nodeIndex];
    node.bounds = nodeBounds;
    node.rightOrFirst = rightIndex;
    node.count = 0;
    node.axis = static_cast<uint8_t>(axis);

    return nodeIndex;
}

auto BVH::intersect(Ray ray) const -> Hit
{
    Hit closest;

    if (m_Nodes.empty())
    {
        return closest;
    }

    const Vec3 invDir(1.0f / ray.dir.x, 1.0f / ray.dir.y, 1.0f / ray.dir.z);
    const bool dirIsNeg[3] = {invDir.x < 0.0f, invDir.y < 0.0f, invDir.z < 0.0f};

    std::array<uint32_t, maxBuildDepth + 8> stack{};
    int stackSize = 0;
    uint32_t current = 0;

    while (true)
    {
        const BVHNode &node = m_Nodes[current];

        Stats::countNodeVisit();

        float tNear = 0.0f;
        if (node.bounds.intersect(ray.org, invDir, ray.tMin, ray.tMax, tNear))
        {
            if (node.isLeaf())
            {
                for (uint16_t i = 0; i < node.count; i++)
                {
                    Stats::countPrimitiveTest();

                    const Hit hit = m_Primitives[node.rightOrFirst + i]->rayIntersect(ray);

                    if (hit.isHit && hit.time < ray.tMax)
                    {
                        closest = hit;

                        // Shrinking the ray's far bound is what makes traversal cheap:
                        // every later box and primitive test is now bounded by the best
                        // hit found so far.
                        ray.tMax = hit.time;
                    }
                }
            }
            else
            {
                // Visit the nearer child first so the far one is often culled by the
                // tightened tMax before it is ever reached.
                const uint32_t left = current + 1;
                const uint32_t right = node.rightOrFirst;

                if (dirIsNeg[node.axis])
                {
                    stack[stackSize++] = left;
                    current = right;
                }
                else
                {
                    stack[stackSize++] = right;
                    current = left;
                }

                continue;
            }
        }

        if (stackSize == 0)
        {
            break;
        }

        current = stack[--stackSize];
    }

    return closest;
}

auto BVH::isOccluded(const Ray &ray) const -> bool
{
    if (m_Nodes.empty())
    {
        return false;
    }

    const Vec3 invDir(1.0f / ray.dir.x, 1.0f / ray.dir.y, 1.0f / ray.dir.z);

    std::array<uint32_t, maxBuildDepth + 8> stack{};
    int stackSize = 0;
    uint32_t current = 0;

    while (true)
    {
        const BVHNode &node = m_Nodes[current];

        Stats::countNodeVisit();

        float tNear = 0.0f;
        if (node.bounds.intersect(ray.org, invDir, ray.tMin, ray.tMax, tNear))
        {
            if (node.isLeaf())
            {
                for (uint16_t i = 0; i < node.count; i++)
                {
                    Stats::countPrimitiveTest();

                    // Any intersection blocks the light, so there is no reason to keep
                    // looking for the closest one.
                    if (m_Primitives[node.rightOrFirst + i]->rayIntersect(ray).isHit)
                    {
                        return true;
                    }
                }
            }
            else
            {
                stack[stackSize++] = node.rightOrFirst;
                current = current + 1;
                continue;
            }
        }

        if (stackSize == 0)
        {
            break;
        }

        current = stack[--stackSize];
    }

    return false;
}
