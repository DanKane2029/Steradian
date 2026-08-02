#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "AABB.h"
#include "Primitives.h"
#include "RayTracer/Hit.h"
#include "RayTracer/Ray.h"
#include "Utils/Transform.h"
#include "Utils/Vec3.h"

/**
 * \brief All the geometry in a scene, in flat typed arrays.
 *
 * Vertices live in three parallel arrays shared by every triangle, and primitives are
 * addressed by index in one space: triangles first, then spheres. Nothing here is
 * polymorphic and nothing is individually allocated, which removes a virtual call from
 * the intersection inner loop and shrinks the working set the traversal walks over.
 *
 * A vertex with no normal in the source model stores a zero normal, and interpolation
 * falls back to the triangle's own plane -- which is exactly what a triangle without
 * vertex normals did before, by the same code path. Absent texture coordinates likewise
 * store zero, and interpolate to zero. Neither case needs a flag.
 */
class Geometry
{
  public:
    /** \brief Appends a vertex and returns the index that refers to it. */
    auto addVertex(const Vec3 &position, const Vec3 &normal, const Vec3 &texCoord) -> uint32_t
    {
        const auto index = static_cast<uint32_t>(m_Positions.size());

        m_Positions.push_back(position);
        m_Normals.push_back(normal);
        m_TexCoords.push_back(texCoord);

        return index;
    }

    void addTriangle(uint32_t vertex0, uint32_t vertex1, uint32_t vertex2, uint32_t materialIndex)
    {
        m_Triangles.push_back(Triangle{vertex0, vertex1, vertex2, materialIndex});
        m_TriangleEdges.push_back(edgesOf(m_Triangles.back()));
    }

    /** \brief Appends a sphere and returns its index within the sphere array. */
    auto addSphere(const Vec3 &center, float radius, uint32_t materialIndex) -> uint32_t
    {
        const auto index = static_cast<uint32_t>(m_Spheres.size());

        m_Spheres.push_back(Sphere{center, radius, materialIndex, -1});

        return index;
    }

    /** \brief Marks a sphere as a light that direct lighting samples explicitly. */
    void setSphereEmitter(uint32_t sphereIndex, int32_t emitterIndex)
    {
        m_Spheres[sphereIndex].emitterIndex = emitterIndex;
    }

    /**
     * \brief Moves vertices [firstVertex, end) into place.
     *
     * Placement is applied once, at load, to the vertices an object contributed, rather
     * than to each of its triangles. A vertex shared by six faces used to be transformed
     * six times.
     */
    void transformVerticesFrom(uint32_t firstVertex, const Transform &transform);

    /**
     * \brief Lays the primitives and vertices out in the order a traversal reads them.
     *
     * Primitives are renumbered into the sequence the hierarchy visits them, and vertices
     * into the order those primitives first refer to them, so triangles that are tested
     * together sit together and so do their vertices.
     *
     * On its own this recovers about a third of what indexing costs; TriangleEdges
     * recovers the rest. Both are kept, because ordering is also what makes the reads of
     * the edge array sequential: with it the bunny renders 3% faster than without, and no
     * scene measured slower.
     *
     * \param traversalOrder The hierarchy's primitive order, rewritten in place to the
     *        new numbering.
     */
    void reorderPrimitives(std::vector<uint32_t> &traversalOrder);

    /** \brief Reserves room for a model about to be appended. */
    void reserveVertices(size_t count)
    {
        m_Positions.reserve(m_Positions.size() + count);
        m_Normals.reserve(m_Normals.size() + count);
        m_TexCoords.reserve(m_TexCoords.size() + count);
    }

    void reserveTriangles(size_t count)
    {
        m_Triangles.reserve(m_Triangles.size() + count);
        m_TriangleEdges.reserve(m_TriangleEdges.size() + count);
    }

    auto vertexCount() const -> uint32_t
    {
        return static_cast<uint32_t>(m_Positions.size());
    }

    auto triangleCount() const -> uint32_t
    {
        return static_cast<uint32_t>(m_Triangles.size());
    }

    auto sphereCount() const -> uint32_t
    {
        return static_cast<uint32_t>(m_Spheres.size());
    }

    /** \brief Triangles and spheres in one index space, triangles first. */
    auto primitiveCount() const -> uint32_t
    {
        return triangleCount() + sphereCount();
    }

    auto isTriangle(uint32_t primitive) const -> bool
    {
        return primitive < triangleCount();
    }

    /** \brief Axis aligned bounds of one primitive, as the hierarchy is built from. */
    auto primitiveBounds(uint32_t primitive) const -> AABB;

    /** \brief Bounds of everything, for callers that need the scene's extent. */
    auto bounds() const -> AABB;

    /**
     * \brief Intersects one primitive, reporting only what the traversal needs.
     *
     * Deliberately returns the sixteen byte hit record and nothing else. Position,
     * normal and texture coordinate are recovered later by surfaceAt, for the one
     * primitive that turns out to be closest.
     */
    auto intersect(uint32_t primitive, const Ray &ray) const -> Hit
    {
        return isTriangle(primitive) ? intersectTriangle(primitive, ray)
                                     : intersectSphere(primitive - triangleCount(), ray);
    }

    /** \brief Recovers the shading attributes at a hit. */
    auto surfaceAt(const Ray &ray, const Hit &hit) const -> Surface;

    auto getTriangles() const -> const std::vector<Triangle> &
    {
        return m_Triangles;
    }

    auto getSpheres() const -> const std::vector<Sphere> &
    {
        return m_Spheres;
    }

    auto getPositions() const -> const std::vector<Vec3> &
    {
        return m_Positions;
    }

    auto getNormals() const -> const std::vector<Vec3> &
    {
        return m_Normals;
    }

    auto getTexCoords() const -> const std::vector<Vec3> &
    {
        return m_TexCoords;
    }

  private:
    /**
     * \brief Moller-Trumbore, without the surface attributes.
     *
     * Kept in the header because this is the inner loop: the hierarchy calls it once per
     * primitive in every leaf a ray reaches, and it must inline.
     */
    auto intersectTriangle(uint32_t triangleIndex, const Ray &ray) const -> Hit
    {
        constexpr float parallelEpsilon = 0.0000001f;

        const TriangleEdges &triangle = m_TriangleEdges[triangleIndex];

        const Vec3 p = ray.dir.cross(triangle.edge2);

        // A determinant near zero means the ray lies in the triangle's plane. Not
        // culling: back faces are hit as readily as front ones.
        const float det = triangle.edge1.dot(p);
        if (det > -parallelEpsilon && det < parallelEpsilon)
        {
            return {};
        }

        const float invDet = 1.0f / det;
        const Vec3 dist = ray.org - triangle.point0;

        const float u = dist.dot(p) * invDet;
        if (u < 0.0f || u > 1.0f)
        {
            return {};
        }

        const Vec3 q = dist.cross(triangle.edge1);

        const float v = ray.dir.dot(q) * invDet;
        if (v < 0.0f || (u + v) > 1.0f)
        {
            return {};
        }

        const float t = triangle.edge2.dot(q) * invDet;
        if (!ray.isValidHit(t))
        {
            return {};
        }

        Hit hit;
        hit.time = t;
        hit.u = u;
        hit.v = v;
        hit.primitive = triangleIndex;

        return hit;
    }

    auto intersectSphere(uint32_t sphereIndex, const Ray &ray) const -> Hit
    {
        const Sphere &sphere = m_Spheres[sphereIndex];

        const Vec3 l = sphere.center - ray.org;
        const float tca = l.dot(ray.dir);

        // Note: no early rejection on tca < 0. That would discard rays whose origin is
        // inside the sphere, which still have a valid forward intersection, and would
        // block refraction.
        const float d2 = l.dot(l) - (tca * tca);
        const float radius2 = sphere.radius * sphere.radius;
        if (d2 > radius2)
        {
            return {};
        }

        const float thc = sqrtf(radius2 - d2);

        // Take the nearest intersection inside the ray's valid interval; if the near root
        // is behind tMin (the ray starts inside, or just off the surface), try the far one.
        float t = tca - thc;
        if (!ray.isValidHit(t))
        {
            t = tca + thc;
            if (!ray.isValidHit(t))
            {
                return {};
            }
        }

        Hit hit;
        hit.time = t;
        hit.primitive = triangleCount() + sphereIndex;

        return hit;
    }

    /** \brief The inner loop's view of one triangle, taken from the indexed arrays. */
    auto edgesOf(const Triangle &triangle) const -> TriangleEdges
    {
        const Vec3 &point0 = m_Positions[triangle.vertex0];

        return TriangleEdges{point0, m_Positions[triangle.vertex1] - point0, m_Positions[triangle.vertex2] - point0};
    }

    /** \brief Rebuilds the whole traversal view after the vertices or triangles change. */
    void rebuildTriangleEdges();

    /** \brief Interpolates vertex normals, falling back to the plane when there are none. */
    auto interpolateNormal(const Triangle &triangle, float u, float v) const -> Vec3;

    /** \brief The geometric normal of a triangle's plane. */
    auto faceNormal(const Triangle &triangle) const -> Vec3;

    std::vector<Vec3> m_Positions;

    /** parallel to m_Positions; zero where the source model supplied no normal */
    std::vector<Vec3> m_Normals;

    /** parallel to m_Positions; zero where the source model supplied no coordinate */
    std::vector<Vec3> m_TexCoords;

    std::vector<Triangle> m_Triangles;
    std::vector<Sphere> m_Spheres;

    /** parallel to m_Triangles; derived from it and the positions, never authored */
    std::vector<TriangleEdges> m_TriangleEdges;
};
