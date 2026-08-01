#pragma once
#include "Utils/Vec3.h"

/**
 * \brief A point and a direction used to simulate a ray.
 *
 * A struct that contains 2 Vector3 parameters an origin and a direction. Used to calculate ray and scene object
 * intersections.
 */
struct Ray
{
    /**
     * \brief The origin of the ray.
     *
     * A Vector3 used to describe the origin and starting point of the ray.
     */
    Vec3 org;

    /**
     * \brief The direction of the ray.
     *
     * A Vector3 used to describe the direction of the ray. The direction Vector3 should be a unit vector.
     */
    Vec3 dir;

    /**
     * \brief Creates a new Ray.
     *
     * Creates a new Ray with an origin and normalized direction.
     *
     * \param org The origin or starting point of the ray.
     * \param dir The direction of the ray.
     */
    Ray(Vec3 org, Vec3 dir) : org(org), dir(dir)
    {
        this->dir.normalize();
    }

    Ray(Vec3 org, Vec3 dir, float tMin, float tMax) : org(org), dir(dir), tMin(tMin), tMax(tMax)
    {
        this->dir.normalize();
    }

    Ray() : org(Vec3()), dir(Vec3(0, 0, 1))
    {
    }

    /**
     * \brief The interval along the ray that counts as a valid intersection.
     *
     * tMin exists to stop a ray that starts on a surface from immediately re-hitting
     * that surface at t close to zero, which is what produced shadow acne. tMax bounds
     * the far end, so a shadow ray can stop at the light rather than searching the whole
     * scene for the closest hit.
     */
    float tMin = defaultEpsilon;
    float tMax = INFINITY;

    /** offset applied to secondary ray origins to avoid self-intersection */
    static constexpr float defaultEpsilon = 1e-4f;

    /** true when t lies within this ray's valid interval */
    auto isValidHit(float t) const -> bool
    {
        return t >= tMin && t <= tMax;
    }

    /**
     * \brief Gets the position along a ray.
     *
     * Calculates the Vector3 position along a ray given a time parameter.
     *
     * \param t The time value that describes how along the ray the position is where t=0 is the ray origin.
     */
    auto posAt(float t) const -> Vec3
    {
        return org + (dir * t);
    }

    /**
     * \brief Reflects the ray about the surface normal at a hit position.
     *
     * The reflected ray starts slightly off the surface, along the normal, so it cannot
     * immediately intersect the surface it is leaving.
     */
    auto getReflectionRay(Vec3 pos, Vec3 normal) const -> Ray
    {
        const Vec3 reverseDir = -dir;
        const float revNDot = reverseDir.dot(normal);
        const Vec3 reflectedDir = (normal * 2.0f * revNDot) - reverseDir;

        return {offsetOrigin(pos, normal), reflectedDir};
    }

    /**
     * \brief Nudges a surface point off the surface along the normal.
     *
     * Secondary rays start here rather than exactly on the geometry. Without this a ray
     * leaving a surface finds that same surface at t close to zero and the point shadows
     * itself, which is the speckled "shadow acne" pattern.
     *
     * \param pos The point on the surface.
     * \param normal The surface normal at that point.
     */
    static auto offsetOrigin(const Vec3 &pos, const Vec3 &normal) -> Vec3
    {
        return pos + (normal * defaultEpsilon);
    }
};
