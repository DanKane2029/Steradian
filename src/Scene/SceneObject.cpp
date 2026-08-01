#include "SceneObject.h"

#include <cmath>

#include <cmath>

#define EPSILON 0.0000001f

// SCENE OBJECT

/**
 * sets the name of the material that describes the surface of the scene object
 *
 * \param name - name of the material as a string
 */
// TRIANGLE

/**
 * create a triangle from 3 points
 *
 * \param p0 - position of point 0 as a Vec3
 * \param p1 - position of point 1 as a Vec3
 * \param p2 - position of point 2 as a Vec3
 */
Triangle::Triangle(Vec3 p0, Vec3 p1, Vec3 p2) : point0(p0), point1(p1), point2(p2), hasNormalVertices(false)
{
    // calculates the smallest of 3 floats
    auto smallest = [](float x, float y, float z) { return std::min(std::min(x, y), z); };

    // calculates the largest of 3 floats
    auto largest = [](float x, float y, float z) { return std::max(std::max(x, y), z); };

    // calculate the min and max x, y, & z coordinates
    minX = smallest(point0.x, point1.x, point2.x);
    minY = smallest(point0.y, point1.y, point2.y);
    minZ = smallest(point0.z, point1.z, point2.z);

    maxX = largest(point0.x, point1.x, point2.x);
    maxY = largest(point0.y, point1.y, point2.y);
    maxZ = largest(point0.z, point1.z, point2.z);
}

Triangle::Triangle(Vec3 p0, Vec3 n0, Vec3 p1, Vec3 n1, Vec3 p2, Vec3 n2)
    : point0(p0), normal0(n0), point1(p1), normal1(n1), point2(p2), normal2(n2), hasNormalVertices(true)
{
    // calculates the smallest of 3 floats
    auto smallest = [](float x, float y, float z) { return std::min(std::min(x, y), z); };

    // calculates the largest of 3 floats
    auto largest = [](float x, float y, float z) { return std::max(std::max(x, y), z); };

    // calculate the min and max x, y, & z coordinates
    minX = smallest(point0.x, point1.x, point2.x);
    minY = smallest(point0.y, point1.y, point2.y);
    minZ = smallest(point0.z, point1.z, point2.z);

    maxX = largest(point0.x, point1.x, point2.x);
    maxY = largest(point0.y, point1.y, point2.y);
    maxZ = largest(point0.z, point1.z, point2.z);
}

/**
 * calculate the area of the triangle defined by points p0, p1, and p2.
 *
 * \param p0 - the position of the first point of the triangle
 * \param p1 - the position of the second point of the triangle
 * \param p2 - the position of the thrid point of the triangle
 *
 */
float triangleArea(Vec3 p0, Vec3 p1, Vec3 p2)
{
    Vec3 v02 = p2 - p0;
    Vec3 v12 = p2 - p1;

    return v02.cross(v12).length() / 2.0f;
}

/**
 * calculates the normal of the surface of the triangle
 *
 * \param position - the position on the triangle to calculate the normal of
 * \return - the normalmalized normal vector
 */
Vec3 Triangle::getNormal(Vec3 position)
{
    if (!hasNormalVertices)
    {
        return faceNormal();
    }

    // Recover barycentrics from areas. This is only needed for callers that have a
    // position but no barycentrics; the intersection path uses interpolateNormal
    // directly with the coordinates Moller-Trumbore already computed.
    const float areaTotal = triangleArea(point0, point1, point2);
    if (areaTotal <= 0.0f)
    {
        return faceNormal();
    }

    const float u = triangleArea(position, point2, point0) / areaTotal;
    const float v = triangleArea(position, point0, point1) / areaTotal;

    return interpolateNormal(u, v);
}

/**
 * the geometric normal of the triangle's plane
 */
auto Triangle::faceNormal() const -> Vec3
{
    const Vec3 v1 = point1 - point0;
    const Vec3 v2 = point2 - point0;

    return v1.cross(v2).normalized();
}

/**
 * interpolates the vertex normals using barycentric coordinates
 *
 * \param u The barycentric weight of point1, as produced by the intersection test.
 * \param v The barycentric weight of point2.
 */
auto Triangle::interpolateNormal(float u, float v) const -> Vec3
{
    if (!hasNormalVertices)
    {
        return faceNormal();
    }

    const float w = 1.0f - u - v;
    const Vec3 interpolated = (normal0 * w) + (normal1 * u) + (normal2 * v);

    // Degenerate vertex normals (all zero, or cancelling out) would normalize to NaN.
    if (interpolated.lengthSquared() <= 0.0f)
    {
        return faceNormal();
    }

    return interpolated.normalized();
}

/**
 * interpolates the vertex texture coordinates using barycentric coordinates
 */
auto Triangle::interpolateTextureCoord(float u, float v) const -> Vec3
{
    if (!hasTextureCoords)
    {
        return {};
    }

    const float w = 1.0f - u - v;
    return (texCoord0 * w) + (texCoord1 * u) + (texCoord2 * v);
}

/**
 * calculates if a ray intersects the triangle
 * adapted from Moller-Trumbore intersection algorithm pseudocode on wikipedia
 * https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
 *
 * \param ray - the ray that possibly interescts the triangle
 * \return - the hit object that tells if the ray interesects the triangle
 *			 along with other relevant information
 */
auto Triangle::rayIntersect(Ray ray) -> Hit
{
    Hit hit;

    Vec3 orig = ray.org;
    Vec3 dir = ray.dir;

    Vec3 v0 = point0;
    Vec3 v1 = point1;
    Vec3 v2 = point2;

    // vectors for edges sharing V1
    Vec3 e1 = v1 - v0;
    Vec3 e2 = v2 - v0;

    // begin calculating determinant - also used to calculate u param
    Vec3 p = dir.cross(e2);

    // if determinant is near zero, ray lies in plane of triangle
    float det = e1.dot(p);
    // NOT culling
    if (det > -EPSILON && det < EPSILON)
    {
        return hit;
    }
    float invDet = 1.0f / det;

    // calculate distance from v0 to ray origin
    Vec3 dist = orig - v0;

    // calculate u parameter and test bound
    float u = dist.dot(p) * invDet;
    // the intersection lies outside of the triangle
    if (u < 0.0f || u > 1.0f)
    {
        return hit;
    }
    // prepare to test v parameter
    Vec3 q = dist.cross(e1);

    // calculate v param and test bound
    float v = dir.dot(q) * invDet;

    // the intersection is outside the triangle
    if (v < 0.0 || (u + v) > 1.0f)
    {
        return hit;
    }

    float t = e2.dot(q) * invDet;

    if (ray.isValidHit(t))
    {
        hit.isHit = true;
        hit.materialIndex = m_MaterialIndex;
        hit.emitterIndex = m_EmitterIndex;
        hit.time = t;
        hit.position = ray.posAt(t);

        // Moller-Trumbore already produced the barycentric coordinates, so interpolated
        // normals and texture coordinates come for free rather than being recomputed.
        hit.normal = interpolateNormal(u, v);
        hit.textureCoord = interpolateTextureCoord(u, v);

        // Present the surface facing the ray, so back faces shade like front faces.
        if (hit.normal.dot(ray.dir) > 0.0f)
        {
            hit.normal = -hit.normal;
        }

        return hit;
    }

    return hit;
}

/**
 * calculates the center point of the triangle by averaging all 3 points
 *
 * \return - the center point as a Vec3
 */
auto Triangle::getCenterPoint() -> Vec3
{
    return (point0 + point1 + point2) / 3.0f;
}

/**
 * attaches per-vertex texture coordinates loaded from a model file
 */
void Triangle::setTextureCoords(Vec3 t0, Vec3 t1, Vec3 t2)
{
    texCoord0 = t0;
    texCoord1 = t1;
    texCoord2 = t2;
    hasTextureCoords = true;
}

auto Triangle::getPoints() -> std::vector<Vec3>
{
    return {point0, point1, point2};
}

// SPHERE

/**
 * creates a sphere scene object from a central point and radius
 *
 * \param center - the center of the sphere as a Vec3
 * \param radius - the radius of the sphere
 */
Sphere::Sphere(Vec3 center, float radius) : m_Center(center), m_Radius(radius)
{
    minX = m_Center.x - m_Radius;
    minY = m_Center.y - m_Radius;
    minZ = m_Center.z - m_Radius;

    maxX = m_Center.x + m_Radius;
    maxY = m_Center.y + m_Radius;
    maxZ = m_Center.z + m_Radius;
}

/**
 * calculates the normal vector of a point on a sphere
 *
 * \param position - the position on the point of the sphere
 * \return - the normalized normal vector
 */
auto Sphere::getNormal(Vec3 position) -> Vec3
{
    Vec3 normal = Vec3(position.x - m_Center.x, position.y - m_Center.y, position.z - m_Center.z);
    normal.normalize();

    return normal;
}

/**
 * calculates if a ray intersects a sphere
 *
 * \param ray - the ray to test for an intersection
 * \return - the hit object that tells if the ray interesects the sphere
 *			 along with other relevant information
 */
auto Sphere::rayIntersect(Ray ray) -> Hit
{
    Hit hit;

    const Vec3 l = m_Center - ray.org;

    const float tca = l.dot(ray.dir);

    // Note: no early rejection on tca < 0 here. That would discard rays whose origin is
    // inside the sphere, which still have a valid forward intersection, and would block
    // refraction later.
    const float d2 = l.dot(l) - (tca * tca);
    const float radius2 = m_Radius * m_Radius;
    if (d2 > radius2)
    {
        return hit;
    }

    const float thc = sqrtf(radius2 - d2);

    // Take the nearest intersection inside the ray's valid interval; if the near root is
    // behind tMin (the ray starts inside, or just off the surface), try the far one.
    float t = tca - thc;
    if (!ray.isValidHit(t))
    {
        t = tca + thc;
        if (!ray.isValidHit(t))
        {
            return hit;
        }
    }

    hit.isHit = true;
    hit.materialIndex = m_MaterialIndex;
    hit.emitterIndex = m_EmitterIndex;
    hit.time = t;
    hit.position = ray.posAt(t);
    hit.normal = getNormal(hit.position);
    hit.textureCoord = getTextureCoords(hit.position);

    // Present the surface facing the ray so a ray inside the sphere shades sensibly.
    if (hit.normal.dot(ray.dir) > 0.0f)
    {
        hit.normal = -hit.normal;
    }

    return hit;
}

/**
 * maps a point on the sphere to spherical texture coordinates in [0, 1]
 */
auto Sphere::getTextureCoords(Vec3 pointOnSurface) const -> Vec3
{
    const Vec3 n = (pointOnSurface - m_Center).normalized();

    const float u = (atan2f(n.x, n.z) / (2.0f * static_cast<float>(M_PI))) + 0.5f;
    const float v = (n.y * 0.5f) + 0.5f;

    return {u, v, 0.0f};
}

/**
 * returns the center point of the sphere
 *
 * \return - the sphere's center point as a Vec3
 */
auto Sphere::getCenterPoint() -> Vec3
{
    return m_Center;
}
