#pragma once

#include "Utils/Vec3.h"

#include <algorithm>
#include <cmath>
#include <limits>

/**
 * \brief An axis aligned bounding box.
 *
 * Stored as min/max corners rather than six loose floats so it can be combined and
 * measured without repeating the same three lines per axis.
 */
struct AABB
{
    Vec3 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest()};

    /** grows the box to contain a point */
    void expand(const Vec3 &p)
    {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);

        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }

    /** grows the box to contain another box */
    void expand(const AABB &other)
    {
        expand(other.min);
        expand(other.max);
    }

    /** true when the box contains no volume, i.e. nothing has been added */
    auto isEmpty() const -> bool
    {
        return min.x > max.x || min.y > max.y || min.z > max.z;
    }

    auto extent() const -> Vec3
    {
        return isEmpty() ? Vec3{} : max - min;
    }

    auto centroid() const -> Vec3
    {
        return (min + max) * 0.5f;
    }

    /**
     * \brief Surface area of the box, the geometric term in the SAH cost function.
     *
     * The probability that a uniformly distributed ray crossing a parent box also crosses
     * a child is the ratio of their surface areas, which is what lets the build compare
     * candidate splits.
     */
    auto surfaceArea() const -> float
    {
        if (isEmpty())
        {
            return 0.0f;
        }

        const Vec3 d = max - min;
        return 2.0f * ((d.x * d.y) + (d.y * d.z) + (d.z * d.x));
    }

    /** index of the longest axis: 0 for x, 1 for y, 2 for z */
    auto maxExtentAxis() const -> int
    {
        const Vec3 d = extent();

        if (d.x > d.y && d.x > d.z)
        {
            return 0;
        }

        return (d.y > d.z) ? 1 : 2;
    }

    /** component of the given axis, for axis-generic build code */
    static auto axisValue(const Vec3 &v, int axis) -> float
    {
        return (axis == 0) ? v.x : ((axis == 1) ? v.y : v.z);
    }

    /**
     * \brief Slab test against a ray.
     *
     * Takes the reciprocal direction rather than computing it per node: a ray tests many
     * boxes, so the three divides are hoisted into the caller.
     *
     * The std::min/std::max ordering is deliberate. When a direction component is zero
     * its reciprocal is infinite, and if the ray origin lies exactly on a slab plane the
     * product is NaN. std::min(a, b) returns a when b is NaN, so a degenerate axis is
     * ignored rather than rejecting the box outright.
     *
     * \param org The ray origin.
     * \param invDir Component-wise reciprocal of the ray direction.
     * \param tMin Near end of the valid interval.
     * \param tMax Far end of the valid interval.
     * \param tNear Set to the entry distance when the test succeeds.
     */
    auto intersect(const Vec3 &org, const Vec3 &invDir, float tMin, float tMax, float &tNear) const -> bool
    {
        const float tx1 = (min.x - org.x) * invDir.x;
        const float tx2 = (max.x - org.x) * invDir.x;
        float near = std::min(tx1, tx2);
        float far = std::max(tx1, tx2);

        const float ty1 = (min.y - org.y) * invDir.y;
        const float ty2 = (max.y - org.y) * invDir.y;
        near = std::max(near, std::min(ty1, ty2));
        far = std::min(far, std::max(ty1, ty2));

        const float tz1 = (min.z - org.z) * invDir.z;
        const float tz2 = (max.z - org.z) * invDir.z;
        near = std::max(near, std::min(tz1, tz2));
        far = std::min(far, std::max(tz1, tz2));

        tNear = std::max(near, tMin);

        return far >= tNear && tNear <= tMax;
    }
};
