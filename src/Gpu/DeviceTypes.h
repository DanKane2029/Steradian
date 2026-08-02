#pragma once

#include "Scene/Material.h"
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
    // Vertex attributes, indexed exactly as on the host. Positions are handed to the
    // acceleration structure as well, but shading needs them for the geometric normal.
    const Vec3 *positions;
    const Vec3 *normals;
    const Vec3 *texCoords;

    const Triangle *triangles;
    const Sphere *spheres;

    unsigned int triangleCount;
    unsigned int sphereCount;
};

/**
 * \brief One image, as a flat block of floats with its shape alongside.
 *
 * Textures are uploaded into a single buffer with each image's extent recorded here, so
 * a material's index reaches its pixels in one indirection rather than through a pointer
 * per texture.
 */
struct DeviceTexture
{
    unsigned int offset;
    int width;
    int height;
    int channels;
};

/** \brief The camera, reduced to what generating a ray actually needs. */
struct DeviceCamera
{
    Vec3 origin;
    Vec3 direction;
    Vec3 right;
    Vec3 up;

    float halfWidth;
    float halfHeight;
};

/**
 * \brief Everything one launch needs.
 *
 * Passed to the device as a single constant-memory block, which is the cheapest place for
 * values every thread reads. Two launch shapes share it: a bare traversal, which uses the
 * ray arrays, and a render, which uses the camera and the film.
 */
struct LaunchParams
{
    /** the acceleration structure to trace against */
    unsigned long long handle;

    DeviceGeometry geometry;

    const Material *materials;
    const Emitter *emitters;
    unsigned int emitterCount;

    /** the microfacet energy compensation table, measured on the host */
    const float *albedoTable;

    const DeviceTexture *textures;
    const float *texturePixels;

    /** radiance arriving from every direction where no geometry is hit */
    Vec3 ambient;

    // ---- bare traversal ----------------------------------------------------------
    const Vec3 *rayOrigins;
    const Vec3 *rayDirections;
    DeviceHit *hits;
    float tMin;
    float tMax;
    unsigned int rayCount;

    // ---- rendering ---------------------------------------------------------------
    DeviceCamera camera;

    /**
     * \brief The running sums, kept on the device between launches.
     *
     * An interactive view adds a few samples per frame and shows the average of every
     * sample taken since the camera last moved. Keeping the sum here means a frame costs
     * one launch and one copy out, rather than reading the whole image back, averaging it
     * on the host and writing it again.
     */
    Vec3 *accumColour;
    Vec3 *accumAlbedo;
    Vec3 *accumNormal;

    /** the resolved average, which is what gets copied back */
    Vec3 *film;
    Vec3 *filmAlbedo;
    Vec3 *filmNormal;

    /** samples already in the sums; zero starts a fresh accumulation */
    unsigned int accumulatedBefore;

    int width;
    int height;
    unsigned int samplesPerPixel;
    unsigned int maxDepth;
    unsigned long long seed;
};

} // namespace Gpu
