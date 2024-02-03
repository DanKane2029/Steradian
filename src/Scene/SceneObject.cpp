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
void SceneObject::setMaterialName(std::string &name)
{
    m_MaterialName = name;
}

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
        Vec3 v1 = point1 - point0;
        Vec3 v2 = point2 - point0;

        Vec3 normal = v1.cross(v2);
        normal.normalize();

        return normal;
    }
    else
    {
        float areaTotal = triangleArea(point0, point1, point2);
        float area0 = triangleArea(position, point1, point2);
        float area1 = triangleArea(position, point2, point0);
        float area2 = triangleArea(position, point0, point1);

        float weight0 = area0 / areaTotal;
        float weight1 = area1 / areaTotal;
        float weight2 = area2 / areaTotal;

        Vec3 interpolatedNormal = (normal0 * weight0) + (normal1 * weight1) + (normal2 * weight2);
        interpolatedNormal.normalize();

        return interpolatedNormal;
    }
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

    if (t > 0.0)
    {
        hit.isHit = true;
        hit.materialName = m_MaterialName;
        hit.time = t;
        hit.position = ray.posAt(t);
        hit.normal = getNormal(hit.position);

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

    Vec3 l = m_Center - ray.org;

    float tca = l.dot(ray.dir);
    if (tca < 0.0f)
    {
        return hit;
    }

    float d2 = l.dot(l) - tca * tca;
    float radius2 = m_Radius * m_Radius;
    if (d2 > radius2)
    {
        return hit;
    }

    float thc = sqrtf(radius2 - d2);

    float t0 = tca - thc;
    float t1 = tca + thc;

    if (t0 > t1)
    {
        std::swap(t0, t1);
    }

    if (t0 < 0.0f)
    {
        t0 = t1;
        // t0 and t1 are both negative
        if (t0 < 0.0f)
        {
            return hit;
        }
    }

    hit.isHit = true;
    hit.materialName = m_MaterialName;
    hit.time = t0;
    hit.position = ray.posAt(t0);
    hit.normal = getNormal(hit.position);

    return hit;
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
