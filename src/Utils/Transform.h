#pragma once

#include "Utils/Vec3.h"

#include <cmath>

/**
 * \brief Places an object in the scene: scale, then rotate, then translate.
 *
 * Applied once when a scene loads rather than at every intersection. The primitives are
 * moved into their final positions and the renderer never sees a transform, which keeps
 * the traversal and intersection code unchanged and costs nothing per ray.
 *
 * The trade is that an object appearing twice is stored twice. A transform evaluated at
 * intersection time instead would allow one copy of a mesh to appear many times, which is
 * worth doing when scenes grow large enough to need it.
 */
struct Transform
{
    Vec3 translation{0.0f, 0.0f, 0.0f};

    /** rotation in degrees, applied about X then Y then Z */
    Vec3 rotationDegrees{0.0f, 0.0f, 0.0f};

    Vec3 scale{1.0f, 1.0f, 1.0f};

    auto isIdentity() const -> bool
    {
        return translation.lengthSquared() == 0.0f && rotationDegrees.lengthSquared() == 0.0f && scale.x == 1.0f &&
               scale.y == 1.0f && scale.z == 1.0f;
    }

    /** true when the three scale factors differ, which spheres cannot represent */
    auto hasNonUniformScale() const -> bool
    {
        return scale.x != scale.y || scale.y != scale.z;
    }

    /** the single scale factor to use where only one is meaningful, such as a radius */
    auto uniformScale() const -> float
    {
        return std::max(std::max(std::fabs(scale.x), std::fabs(scale.y)), std::fabs(scale.z));
    }

    /**
     * \brief Rotates a vector by this transform's rotation, ignoring scale and position.
     */
    auto rotate(const Vec3 &v) const -> Vec3
    {
        constexpr float degreesToRadians = 3.14159265358979323846f / 180.0f;

        const float cx = std::cos(rotationDegrees.x * degreesToRadians);
        const float sx = std::sin(rotationDegrees.x * degreesToRadians);
        const float cy = std::cos(rotationDegrees.y * degreesToRadians);
        const float sy = std::sin(rotationDegrees.y * degreesToRadians);
        const float cz = std::cos(rotationDegrees.z * degreesToRadians);
        const float sz = std::sin(rotationDegrees.z * degreesToRadians);

        // About X, then Y, then Z.
        Vec3 r = v;

        r = Vec3(r.x, (r.y * cx) - (r.z * sx), (r.y * sx) + (r.z * cx));
        r = Vec3((r.x * cy) + (r.z * sy), r.y, (-r.x * sy) + (r.z * cy));
        r = Vec3((r.x * cz) - (r.y * sz), (r.x * sz) + (r.y * cz), r.z);

        return r;
    }

    /** moves a point into its final position */
    auto transformPoint(const Vec3 &p) const -> Vec3
    {
        return rotate(Vec3(p.x * scale.x, p.y * scale.y, p.z * scale.z)) + translation;
    }

    /**
     * \brief Reorients a surface normal.
     *
     * Normals do not transform like points. Squashing an object along one axis tilts its
     * surfaces the opposite way to how it moves its points, so scaling a normal directly
     * leaves it no longer perpendicular to the surface it describes, and the shading goes
     * subtly wrong. The correct transform divides by the scale rather than multiplying by
     * it, before applying the same rotation.
     */
    auto transformNormal(const Vec3 &n) const -> Vec3
    {
        const Vec3 inverseScaled(n.x / scale.x, n.y / scale.y, n.z / scale.z);

        return rotate(inverseScaled).normalized();
    }
};
