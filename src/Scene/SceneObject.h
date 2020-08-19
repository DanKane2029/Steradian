#pragma once
#include "Vec3.h"
#include "Ray.h"

class SceneObject
{
public:
	virtual Vec3 GetNormal() = 0;
	virtual bool RayIntersect(Ray ray) = 0;
};

class Triangle: public SceneObject
{
public:
	Triangle(Vec3 p0, Vec3 p1, Vec3 p2);

	Vec3 GetNormal();
	bool RayIntersect(Ray ray);

private:
	Vec3 m_Points[3];
};