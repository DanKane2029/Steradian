// The device side of ray traversal.
//
// Compiled at run time by NVRTC and executed by OptiX, so the ray/box and ray/triangle
// work lands on the RT cores. That is the whole point of the exercise: the CPU renderer
// spends 658 bounding box visits per ray on the dragon against 87.5 triangle tests, and
// box traversal is precisely what that hardware does in silicon.
//
// The programs here are deliberately as small as they can be. They report which primitive
// was hit, how far along, and where on it -- nothing else. Shading reads those four
// numbers later, exactly as the CPU integrator does.

#include <optix.h>

#include "Gpu/DeviceTypes.h"
#include "Scene/Primitives.h"
#include "Utils/Vec3.h"

extern "C" __constant__ Gpu::LaunchParams params;

namespace
{

/**
 * \brief Carries a hit back from a trace through the payload registers.
 *
 * Four 32-bit registers, which is small enough to stay in registers the whole way rather
 * than spilling. A distance and two barycentrics are floats reinterpreted as integers;
 * OptiX payloads are untyped words and this is the usual way to put a float in one.
 */
__device__ __forceinline__ auto asInt(float value) -> unsigned int
{
    return __float_as_uint(value);
}

__device__ __forceinline__ auto asFloat(unsigned int value) -> float
{
    return __uint_as_float(value);
}

} // namespace

extern "C" __global__ void __raygen__trace()
{
    const unsigned int index = optixGetLaunchIndex().x;

    if (index >= params.rayCount)
    {
        return;
    }

    const Vec3 origin = params.rayOrigins[index];
    const Vec3 direction = params.rayDirections[index];

    // Initialised to a miss, so a ray that reaches no hit program reports one without the
    // miss program having to write anything but the primitive word.
    unsigned int p0 = asInt(params.tMax);
    unsigned int p1 = 0;
    unsigned int p2 = 0;
    unsigned int p3 = Gpu::noPrimitive;

    optixTrace(params.handle, make_float3(origin.x, origin.y, origin.z),
               make_float3(direction.x, direction.y, direction.z), params.tMin, params.tMax,
               0.0f, // ray time, unused
               OptixVisibilityMask(255), OPTIX_RAY_FLAG_NONE,
               0, // SBT offset
               1, // SBT stride: one record per primitive kind
               0, // miss index
               p0, p1, p2, p3);

    Gpu::DeviceHit hit;
    hit.time = asFloat(p0);
    hit.u = asFloat(p1);
    hit.v = asFloat(p2);
    hit.primitive = p3;

    params.hits[index] = hit;
}

extern "C" __global__ void __miss__nothing()
{
    optixSetPayload_3(Gpu::noPrimitive);
}

/**
 * \brief Reports a triangle hit found by the built-in intersection.
 *
 * The barycentrics come from OptiX rather than from Moller-Trumbore, because the
 * intersection was done by the RT cores. They follow the same convention the CPU uses --
 * weights of (1 - u - v, u, v) over the three vertices -- so everything downstream reads
 * them the same way. The numbers are not identical to the CPU's, and cannot be: this is a
 * different intersection algorithm running on different hardware.
 */
extern "C" __global__ void __closesthit__triangle()
{
    const float2 barycentrics = optixGetTriangleBarycentrics();

    optixSetPayload_0(__float_as_uint(optixGetRayTmax()));
    optixSetPayload_1(__float_as_uint(barycentrics.x));
    optixSetPayload_2(__float_as_uint(barycentrics.y));
    optixSetPayload_3(optixGetPrimitiveIndex());
}

/**
 * \brief Intersects a sphere.
 *
 * Written out rather than using OptiX's built-in sphere primitive, so that both backends
 * run the same test. The built-in one would be a second implementation of something the
 * CPU already does, and the two would agree until one was changed.
 *
 * Mirrors Geometry::intersectSphere, including its refusal to reject on tca < 0: a ray
 * whose origin is inside the sphere still has a valid forward intersection, and
 * refraction depends on finding it.
 */
extern "C" __global__ void __intersection__sphere()
{
    const unsigned int sphereIndex = optixGetPrimitiveIndex();
    const Sphere sphere = params.geometry.spheres[sphereIndex];

    const float3 rawOrigin = optixGetObjectRayOrigin();
    const float3 rawDirection = optixGetObjectRayDirection();

    const Vec3 origin(rawOrigin.x, rawOrigin.y, rawOrigin.z);
    const Vec3 direction(rawDirection.x, rawDirection.y, rawDirection.z);

    const Vec3 l = sphere.center - origin;
    const float tca = l.dot(direction);

    const float d2 = l.dot(l) - (tca * tca);
    const float radius2 = sphere.radius * sphere.radius;

    if (d2 > radius2)
    {
        return;
    }

    const float thc = sqrtf(radius2 - d2);

    const float tNear = optixGetRayTmin();
    const float tFar = optixGetRayTmax();

    // The near root first; if it is behind the interval, the far one. Reporting outside
    // the interval is not an error, OptiX simply ignores it, but checking here saves the
    // call.
    float t = tca - thc;
    if (t < tNear || t > tFar)
    {
        t = tca + thc;
        if (t < tNear || t > tFar)
        {
            return;
        }
    }

    optixReportIntersection(t, 0);
}

extern "C" __global__ void __closesthit__sphere()
{
    optixSetPayload_0(__float_as_uint(optixGetRayTmax()));

    // A sphere has no barycentrics. The surface point follows from the distance alone,
    // which is what the CPU does with them too.
    optixSetPayload_1(0);
    optixSetPayload_2(0);

    // Spheres occupy the index space above the triangles, matching Geometry.
    optixSetPayload_3(params.geometry.triangleCount + optixGetPrimitiveIndex());
}
