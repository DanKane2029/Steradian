#include "SceneObject.h"

#define EPSILON 0.0000001f

void SceneObject::SetMaterialName(std::string& name)
{
    m_MaterialName = name;
}

Triangle::Triangle(Vec3 p0, Vec3 p1, Vec3 p2)
{
	m_Points[0] = p0;
	m_Points[1] = p1;
	m_Points[2] = p2;
}

Vec3 Triangle::GetNormal()
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
        hit.normal = GetNormal();
        hit.time = t;
        
        return hit;
    }

    return hit;
}

