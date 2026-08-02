#pragma once

#include "Utils/DeviceCompat.h"
#include "Utils/Vec3.h"

// The primitive records, kept apart from the container that holds them.
//
// Geometry owns std::vectors of these and is host-only for that reason, but the records
// themselves have to exist on a device: they are what gets copied into device buffers and
// read by the intersection and shading programs. Splitting them out is what lets both
// sides name the same types instead of describing the same bytes twice.

/**
 * \brief A triangle, as three indices into the shared vertex arrays.
 *
 * Sixteen bytes, and no vertex data of its own. Previously each triangle was an
 * independent heap object holding its own three positions, three normals, three texture
 * coordinates and a bounding box, reached through a pointer and a virtual call. A mesh
 * whose vertices are shared between faces stored each of those vertices once per face
 * that used it, which is roughly six times over.
 */
struct Triangle
{
    uint32_t vertex0 = 0;
    uint32_t vertex1 = 0;
    uint32_t vertex2 = 0;
    uint32_t materialIndex = 0;
};

/**
 * \brief A triangle's geometry, de-indexed and pre-differenced for the inner loop.
 *
 * Indices are the right way to *store* a mesh -- they are what makes vertex sharing
 * possible, and what an acceleration structure wants to be handed -- but they are the
 * wrong way to *read* one during traversal. Following them costs three dependent loads
 * before the test can start: the primitive index, then the triangle, then its vertices,
 * each waiting on the last. Measured on the dragon that chain cost more than the virtual
 * call the flat arrays removed, leaving the packed layout 5% slower than the fat objects
 * it replaced even though it did provably identical work.
 *
 * So the same triangles are held a second time in the form the test actually wants: one
 * contiguous record per triangle, with the two edges already subtracted. The indexed
 * arrays remain the definitive copy and are what gets handed to a device; this is a
 * read-only view derived from them, rebuilt whenever they change.
 */
struct TriangleEdges
{
    Vec3 point0;
    Vec3 edge1;
    Vec3 edge2;
};

/**
 * \brief A sphere in its final world position.
 *
 * Unlike a triangle it carries an emitter index, because emitters are spherical: the
 * scene's direct lighting samples a cone towards a centre and a radius, which is a
 * description only a sphere satisfies. Emissive triangles still light a scene, just
 * through ordinary path tracing rather than by being sampled.
 */
struct Sphere
{
    Vec3 center{};
    float radius = 0.0f;
    uint32_t materialIndex = 0;
    int32_t emitterIndex = -1;
};

/**
 * \brief Everything shading needs about a point, derived once from a Hit.
 *
 * The split between this and Hit is the point of the whole arrangement. Intersection
 * happens many times per ray -- hundreds of primitive tests against a mesh -- while
 * shading happens once, at the winner. Interpolating a normal and a texture coordinate
 * on every test, as the previous hit record did, paid for work that was thrown away
 * almost every time.
 */
struct Surface
{
    Vec3 position{};

    /** always flipped to face the ray, so back faces shade like front faces */
    Vec3 normal{};

    /** interpolated from the vertices; only x and y are meaningful */
    Vec3 textureCoord{};

    uint32_t materialIndex = 0;

    /** index into the scene's emitter list, or -1 when this is not a sampled emitter */
    int32_t emitterIndex = -1;

    /**
     * \brief True when the ray struck the outside of the surface.
     *
     * The normal cannot answer this, because it has already been flipped towards the
     * ray. Refraction needs it: entering glass and leaving it use reciprocal indices,
     * and the difference is invisible once the normal has been turned round.
     */
    bool frontFace = true;
};

/**
 * \brief A light that direct lighting samples explicitly.
 *
 * Spherical, because that is the shape the scene format already describes and a cone
 * towards a centre and a radius is cheap to sample. Emissive geometry of other shapes
 * still lights a scene through ordinary path tracing, just with more noise.
 *
 * Lives here rather than inside Scene because both backends sample from it.
 */
struct Emitter
{
    Vec3 center;
    float radius = 0.0f;
    Vec3 emission;
};
