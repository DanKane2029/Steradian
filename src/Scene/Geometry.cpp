#include "Geometry.h"

#include <algorithm>

auto Geometry::faceNormal(const Triangle &triangle) const -> Vec3
{
    const Vec3 &point0 = m_Positions[triangle.vertex0];

    const Vec3 edge1 = m_Positions[triangle.vertex1] - point0;
    const Vec3 edge2 = m_Positions[triangle.vertex2] - point0;

    return edge1.cross(edge2).normalized();
}

auto Geometry::interpolateNormal(const Triangle &triangle, float u, float v) const -> Vec3
{
    const float w = 1.0f - u - v;

    const Vec3 interpolated =
        (m_Normals[triangle.vertex0] * w) + (m_Normals[triangle.vertex1] * u) + (m_Normals[triangle.vertex2] * v);

    // Two cases land here and want the same answer. A model that supplied no vertex
    // normals stores zeros, so the weighted sum is exactly zero; and a model that
    // supplied degenerate ones, all zero or cancelling out, would normalize to NaN. Both
    // fall back to the plane the triangle actually lies in.
    if (interpolated.lengthSquared() <= 0.0f)
    {
        return faceNormal(triangle);
    }

    return interpolated.normalized();
}

auto Geometry::surfaceAt(const Ray &ray, const Hit &hit) const -> Surface
{
    Surface surface;

    surface.position = ray.posAt(hit.time);

    if (isTriangle(hit.primitive))
    {
        const Triangle &triangle = m_Triangles[hit.primitive];

        surface.materialIndex = triangle.materialIndex;
        surface.normal = interpolateNormal(triangle, hit.u, hit.v);

        const float w = 1.0f - hit.u - hit.v;
        surface.textureCoord = (m_TexCoords[triangle.vertex0] * w) + (m_TexCoords[triangle.vertex1] * hit.u) +
                               (m_TexCoords[triangle.vertex2] * hit.v);
    }
    else
    {
        const Sphere &sphere = m_Spheres[hit.primitive - triangleCount()];

        surface.materialIndex = sphere.materialIndex;
        surface.emitterIndex = sphere.emitterIndex;
        surface.normal = (surface.position - sphere.center).normalized();

        // Spherical mapping, so a texture wraps round the sphere rather than repeating.
        const Vec3 &n = surface.normal;
        surface.textureCoord =
            Vec3((atan2f(n.x, n.z) / (2.0f * static_cast<float>(M_PI))) + 0.5f, (n.y * 0.5f) + 0.5f, 0.0f);
    }

    // Record which side was struck before the normal is flipped, since flipping destroys
    // that information and refraction depends on it.
    surface.frontFace = surface.normal.dot(ray.dir) < 0.0f;

    // Present the surface facing the ray, so back faces shade like front faces.
    if (!surface.frontFace)
    {
        surface.normal = -surface.normal;
    }

    return surface;
}

auto Geometry::primitiveBounds(uint32_t primitive) const -> AABB
{
    AABB bounds;

    if (isTriangle(primitive))
    {
        const Triangle &triangle = m_Triangles[primitive];

        bounds.expand(m_Positions[triangle.vertex0]);
        bounds.expand(m_Positions[triangle.vertex1]);
        bounds.expand(m_Positions[triangle.vertex2]);
    }
    else
    {
        const Sphere &sphere = m_Spheres[primitive - triangleCount()];

        bounds.expand(sphere.center - Vec3(sphere.radius));
        bounds.expand(sphere.center + Vec3(sphere.radius));
    }

    return bounds;
}

auto Geometry::bounds() const -> AABB
{
    AABB total;

    for (uint32_t primitive = 0; primitive < primitiveCount(); primitive++)
    {
        total.expand(primitiveBounds(primitive));
    }

    return total;
}

void Geometry::rebuildTriangleEdges()
{
    m_TriangleEdges.clear();
    m_TriangleEdges.reserve(m_Triangles.size());

    for (const Triangle &triangle : m_Triangles)
    {
        m_TriangleEdges.push_back(edgesOf(triangle));
    }
}

void Geometry::transformVerticesFrom(uint32_t firstVertex, const Transform &transform)
{
    // An identity placement is left strictly alone rather than applied as a no-op. It
    // would not be one: transformNormal normalizes, and a stored normal that is not
    // exactly unit length would come back slightly different.
    if (transform.isIdentity())
    {
        return;
    }

    for (uint32_t i = firstVertex; i < vertexCount(); i++)
    {
        m_Positions[i] = transform.transformPoint(m_Positions[i]);

        // Vertices with no normal store zero, and must keep storing zero: that is what
        // makes interpolation fall back to the triangle's plane.
        if (m_Normals[i].lengthSquared() > 0.0f)
        {
            m_Normals[i] = transform.transformNormal(m_Normals[i]);
        }
    }

    // The traversal view is derived from the positions that just moved. Rebuilding all of
    // it is wasteful in principle -- only this object's triangles changed -- but it is a
    // handful of subtractions per triangle, done once per object at load, and keeping the
    // two representations unconditionally in step is worth more than the microseconds.
    rebuildTriangleEdges();
}

void Geometry::reorderPrimitives(std::vector<uint32_t> &traversalOrder)
{
    // Triangles keep the lower half of the index space and spheres the upper half, so
    // each kind is permuted within its own range rather than across both. Traversal
    // order mixes the two freely, and a scene is overwhelmingly triangles, so this gets
    // the locality that matters while leaving the "triangles first" numbering intact.
    const uint32_t triangles = triangleCount();

    std::vector<Triangle> orderedTriangles;
    std::vector<Sphere> orderedSpheres;
    orderedTriangles.reserve(m_Triangles.size());
    orderedSpheres.reserve(m_Spheres.size());

    std::vector<uint32_t> newIndex(primitiveCount(), 0);

    for (const uint32_t primitive : traversalOrder)
    {
        if (primitive < triangles)
        {
            newIndex[primitive] = static_cast<uint32_t>(orderedTriangles.size());
            orderedTriangles.push_back(m_Triangles[primitive]);
        }
        else
        {
            newIndex[primitive] = triangles + static_cast<uint32_t>(orderedSpheres.size());
            orderedSpheres.push_back(m_Spheres[primitive - triangles]);
        }
    }

    m_Triangles = std::move(orderedTriangles);
    m_Spheres = std::move(orderedSpheres);

    for (uint32_t &primitive : traversalOrder)
    {
        primitive = newIndex[primitive];
    }

    // Vertices follow, numbered by first use. A vertex no triangle refers to is dropped,
    // which an .obj file listing unused positions will produce.
    constexpr uint32_t unassigned = 0xFFFFFFFFu;
    std::vector<uint32_t> vertexMap(m_Positions.size(), unassigned);

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec3> texCoords;
    positions.reserve(m_Positions.size());
    normals.reserve(m_Normals.size());
    texCoords.reserve(m_TexCoords.size());

    const auto remap = [&](uint32_t &vertex) {
        if (vertexMap[vertex] == unassigned)
        {
            vertexMap[vertex] = static_cast<uint32_t>(positions.size());

            positions.push_back(m_Positions[vertex]);
            normals.push_back(m_Normals[vertex]);
            texCoords.push_back(m_TexCoords[vertex]);
        }

        vertex = vertexMap[vertex];
    };

    for (Triangle &triangle : m_Triangles)
    {
        remap(triangle.vertex0);
        remap(triangle.vertex1);
        remap(triangle.vertex2);
    }

    m_Positions = std::move(positions);
    m_Normals = std::move(normals);
    m_TexCoords = std::move(texCoords);

    rebuildTriangleEdges();
}
