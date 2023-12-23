#pragma once
#include "Utils/Vec3.h"
#include <string>

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
     * \brief The name of the material of the scene object that intersected the ray.
     *
     * The string identifier of the material of the scene object that the ray first intersected with.
     */
    std::string materialName;

    /**
     * \brief The color of the hit calculated by the ray tracer.
     *
     * A Vector3 used to describe the color of the scene object at the hit position from the view of the camera. This
     * value is calculated by the ray tracer and stored here to be added to the pixel buffer.
     */
    Vec3 color{};

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
