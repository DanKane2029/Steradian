#pragma once
#include "RayTracer/Ray.h"
#include "Utils/Vec3.h"
#include <cstdint>

/**
 * \brief The result of a ray object intersection.
 *
 * A set of data that describes a ray scene-object interesction.
 */
struct Hit
{
  public:
    /**
     * \brief True if the ray intersected the object.
     *
     * Boolean variable used to describe if the ray intersected the scene object. If false all the other data in the hit
     * object should not be read or used.
     */
    bool isHit = false;

    /**
     * \brief The time it took the ray to hit the object.
     *
     * The time parameter is the value of time in this equation that describes the ray 'rayPosition = rayDirection *
     * time + rayOrigin' where 'rayPosition' is the position of the hit object.
     */
    float time;

    /**
     * \brief The position where the ray and object intersect.
     *
     * The Vector3 value that describes where the ray and object intersected.
     */
    Vec3 position{};

    /**
     * \brief The normal of the scene object at the hit position.
     *
     * The Vector3 value that describes the surface normal of the scene object at the hit position.
     */
    Vec3 normal{};

    /**
     * \brief Index of the material of the primitive that was hit.
     *
     * An index into the scene's material array rather than a name. Looking materials up
     * by string meant a hash lookup and a string copy for every single intersection.
     */
    uint32_t materialIndex = 0;

    /**
     * \brief Index into the scene's emitter list, or -1 when this is not a sampled emitter.
     *
     * The integrator needs to know whether a surface's emission was already accounted for
     * by direct light sampling, so that it is not counted a second time when a scattered
     * ray happens to land on it.
     */
    int32_t emitterIndex = -1;

    /**
     * \brief True when the ray struck the outside of the surface.
     *
     * The normal is always flipped to face the ray, so it cannot be used to work out
     * which side was hit. Refraction needs to know: a ray entering glass and a ray
     * leaving it must use reciprocal indices, and the difference is invisible in the
     * normal once it has been flipped.
     */
    bool frontFace = true;

    /**
     * \brief The texture coordinate at the hit position.
     *
     * Interpolated from the primitive's vertex texture coordinates. Only x and y are
     * meaningful; z is unused and left at zero.
     */
    Vec3 textureCoord{};

    /**
     * \brief The ray that created the hit.
     */
    Ray ray;

    /**
     * \brief Creates a new Hit object.
     *
     * Creates a new Hit object where isHit is set to false. Time is set to INFINITY. All Vector3 parameters are set to
     * (0, 0, 0) and materialName is set to an empty string.
     */
    Hit() : time(INFINITY)
    {
    }
};
