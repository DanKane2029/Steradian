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
Triangle::Triangle(Vec3 p0, Vec3 p1, Vec3 p2)
{
    // assign points
    m_Points[0] = p0;
    m_Points[1] = p1;
    m_Points[2] = p2;

    // calculates the smallest of 3 floats
    auto smallest = [](float x, float y, float z) { return std::min(std::min(x, y), z); };

    // calculates the largest of 3 floats
    auto largest = [](float x, float y, float z) { return std::max(std::max(x, y), z); };

    // calculate bounding box by finding the min and max x, y, & z coordinates
    float minX = smallest(m_Points[0].v[0], m_Points[1].v[0], m_Points[2].v[0]);
    float minY = smallest(m_Points[0].v[1], m_Points[1].v[1], m_Points[2].v[1]);
    float minZ = smallest(m_Points[0].v[2], m_Points[1].v[2], m_Points[2].v[2]);

    float maxX = largest(m_Points[0].v[0], m_Points[1].v[0], m_Points[2].v[0]);
    float maxY = largest(m_Points[0].v[1], m_Points[1].v[1], m_Points[2].v[1]);
    float maxZ = largest(m_Points[0].v[2], m_Points[1].v[2], m_Points[2].v[2]);

    m_BoundingBox = BoundingBox(minX, minY, minZ, maxX, maxY, maxZ);
}

/**
 * calculates the normal of the surface of the triangle
 *
 * \param position - the position on the triangle to calculate the normal of
 * \return - the normalmalized normal vector
 */
Vec3 Triangle::getNormal(Vec3 position)
{
    Vec3 v1 = m_Points[1] - m_Points[0];
    Vec3 v2 = m_Points[2] - m_Points[0];

    Vec3 normal = v1.cross(v2);
    normal.normalize();

    return normal;
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

    Vec3 v0 = m_Points[0];
    Vec3 v1 = m_Points[1];
    Vec3 v2 = m_Points[2];

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
        hit.normal = getNormal(Vec3());
        hit.time = t;
        hit.position = ray.posAt(t);

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
    return (m_Points[0] + m_Points[1] + m_Points[2]) / 3.0f;
}

/**
 * returns the axis aligned bounding box of the triangle
 *
 * \return - the traingle's bounding box
 */
auto Triangle::getBoundingBox() -> BoundingBox
{
    return m_BoundingBox;
}

std::vector<Vec3> Triangle::getPoints()
{
    return {m_Points[0], m_Points[1], m_Points[2]};
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
    float minX = m_Center.v[0] - m_Radius;
    float minY = m_Center.v[1] - m_Radius;
    float minZ = m_Center.v[2] - m_Radius;

    float maxX = m_Center.v[0] + m_Radius;
    float maxY = m_Center.v[1] + m_Radius;
    float maxZ = m_Center.v[2] + m_Radius;

    m_BoundingBox = BoundingBox(minX, minY, minZ, maxX, maxY, maxZ);
}

/**
 * calculates the normal vector of a point on a sphere
 *
 * \param position - the position on the point of the sphere
 * \return - the normalized normal vector
 */
auto Sphere::getNormal(Vec3 position) -> Vec3
{
    Vec3 normal = Vec3(position.v[0] - m_Center.v[0], position.v[1] - m_Center.v[1], position.v[2] - m_Center.v[2]);
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
        return hit;

    float d2 = l.dot(l) - tca * tca;
    float radius2 = m_Radius * m_Radius;
    if (d2 > radius2)
        return hit;

    float thc = sqrtf(radius2 - d2);

    float t0 = tca - thc;
    float t1 = tca + thc;

    if (t0 > t1)
        std::swap(t0, t1);

    if (t0 < 0.0f)
    {
        t0 = t1;
        // t0 and t1 are both negative
        if (t0 < 0.0f)
            return hit;
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

/**
 * returns the sphere's bounding box
 *
 * \return - the sphere's bounding box
 */
auto Sphere::getBoundingBox() -> BoundingBox
{
    return m_BoundingBox;
}
