#pragma once
#include "Vec3.h"
#include "Ray.h"
#include "Hit.h"

#include <string>

class SceneObject
{
public:
	virtual Vec3 GetNormal(Vec3 position) = 0;
	virtual Hit RayIntersect(Ray ray) = 0;

	void SetMaterialName(std::string& name);

protected:
	std::string m_MaterialName;
};

class Triangle: public SceneObject
{
public:
	Triangle(Vec3 p0, Vec3 p1, Vec3 p2);

	Vec3 GetNormal(Vec3 position);
	Hit RayIntersect(Ray ray);

private:
	Vec3 m_Points[3];
};

class Sphere : public SceneObject
{
public:
	Sphere(Vec3 center, float radius);

	Vec3 GetNormal(Vec3 position);
	Hit RayIntersect(Ray ray);

private:
	Vec3 m_Center;
	float m_Radius;
};