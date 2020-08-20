#include "SceneObject.h"

#define EPSILON 0.0000001f

// SCENE OBJECT
void SceneObject::SetMaterialName(std::string& name)
{
    m_MaterialName = name;
}


// TRIANGLE
Triangle::Triangle(Vec3 p0, Vec3 p1, Vec3 p2)
{
	m_Points[0] = p0;
	m_Points[1] = p1;
	m_Points[2] = p2;
}

Vec3 Triangle::GetNormal(Vec3 position)
{
	Vec3 v1 = m_Points[1] - m_Points[0];
	Vec3 v2 = m_Points[2] - m_Points[0];

	return v2.Cross(v2);
}

Hit Triangle::RayIntersect(Ray ray)
{
    Hit hit;
    
    // adapted from Moller-Trumbore intersection algorithm pseudocode on wikipedia
    Vec3 orig = ray.org;
    Vec3 dir = ray.dir;

    Vec3 v0 = m_Points[0];
    Vec3 v1 = m_Points[1];
    Vec3 v2 = m_Points[2];

    // vectors for edges sharing V1
    Vec3 e1 = v1 - v0;
    Vec3 e2 = v2 - v0;

    // begin calculating determinant - also used to calculate u param
    Vec3 P = dir.Cross(e2);

    // if determinant is near zero, ray lies in plane of triangle
    float det = e1.Dot(P);
    // NOT culling
    if (det > -EPSILON && det < EPSILON)
    {
        return hit;
    }
    float inv_det = 1.0f / det;

    // calculate distance from v0 to ray origin
    Vec3 T = orig - v0;

    // calculate u parameter and test bound
    float u = T.Dot(P) * inv_det;
    // the intersection lies outside of the triangle
    if (u < 0.0f || u > 1.0f)
    {
        return hit;
    }
    // prepare to test v parameter
    Vec3 Q = T.Cross(e1);

    // calculate v param and test bound
    float v = dir.Dot(Q) * inv_det;

    // the intersection is outside the triangle
    if (v < 0.0 || (u + v) > 1.0f)
    {
        return hit;
    }

    float t = e2.Dot(Q) * inv_det;

    if (t > 0.0) {
        
        hit.isHit = true;
        hit.materialName = m_MaterialName;
        hit.normal = GetNormal(Vec3());
        hit.time = t;
        hit.position = ray.PosAt(t);
        
        return hit;
    }

    return hit;
}


// SPHERE
Sphere::Sphere(Vec3 center, float radius)
    : m_Center(center), m_Radius(radius)
{
}

Vec3 Sphere::GetNormal(Vec3 position)
{
    Vec3 normal(
        position.v[0] - m_Center.v[0], 
        position.v[1] - m_Center.v[1], 
        position.v[2] - m_Center.v[2]
    );
    normal.normalize();

    return normal;
}

Hit Sphere::RayIntersect(Ray ray)
{
    Hit hit;

    Vec3 L = m_Center - ray.org;

    float tca = L.Dot(ray.dir);
    if (tca < 0.0f) return hit;

    float d2 = L.Dot(L) - tca * tca;
    float radius2 = m_Radius * m_Radius;
    if (d2 > radius2) return hit;

    float thc = sqrtf(radius2 - d2);
    
    float t0 = tca - thc;
    float t1 = tca + thc;

    if (t0 > t1) std::swap(t0, t1);

    if (t0 < 0.0f)
    {
        t0 = t1;
        if (t0 < 0.0f) return hit; // t0 and t1 are both negative
    }

    hit.isHit = true;
    hit.materialName = m_MaterialName;
    hit.time = t0;
    hit.position = ray.PosAt(t0);
    hit.normal = GetNormal(hit.position);

    return hit;
}
