#pragma once

#include "Scene/Primitives.h"
#include "Utils/DeviceCompat.h"
#include "Utils/Vec3.h"

/**
 * \brief Types shared by the host that fills the launch and the device that reads it.
 *
 * Described once so the two sides cannot disagree about a layout. Everything here is
 * plain data: the pointers are device addresses, meaningless to dereference on the host.
 */
namespace Gpu
{

/**
 * \brief What a traced ray reports back.
 *
 * The same four numbers the CPU's Hit carries, and for the same reason: the intersection
 * says only where and what, and everything else is derived once at shade time.
 */
struct DeviceHit
{
    float time;
    float u;
    float v;

    /** index in the scene's primitive space, or noPrimitive */
    unsigned int primitive;
};

/** matches Hit::noPrimitive on the host side */
inline constexpr unsigned int noPrimitive = 0xFFFFFFFFu;

/**
 * \brief The scene's geometry, as device pointers.
 *
 * Triangles are intersected by the RT cores using OptiX's own built-in test, so their
 * vertices are handed to the acceleration structure and not read by any program here.
 * Spheres are custom primitives and are intersected by our own code, which is why the
 * array is present.
 */
struct DeviceGeometry
{
    const Sphere *spheres;
    unsigned int triangleCount;
    unsigned int sphereCount;
};

/**
 * \brief Everything one launch needs.
 *
 * Passed to the device as a single constant-memory block, which is the cheapest place for
 * values every thread reads.
 */
struct LaunchParams
{
    /** the acceleration structure to trace against */
    unsigned long long handle;

    DeviceGeometry geometry;

    /** ray origins and directions, one pair per thread */
    const Vec3 *rayOrigins;
    const Vec3 *rayDirections;

    /** where each ray's result goes */
    DeviceHit *hits;

    /** the interval along each ray that counts, matching the CPU's Ray */
    float tMin;
    float tMax;

    unsigned int rayCount;
};

} // namespace Gpu
