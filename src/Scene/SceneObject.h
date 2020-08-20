#pragma once
#include "Vec3.h"
#include "Ray.h"
#include "Hit.h"

#include <string>

class SceneObject
{
public:
	virtual Vec3 GetNormal() = 0;
	virtual Hit RayIntersect(Ray ray) = 0;

	void SetMaterialName(std::string& name);

protected:
	std::string m_MaterialName;
};

class Triangle: public SceneObject
{
public:
	Triangle(Vec3 p0, Vec3 p1, Vec3 p2);

	Vec3 GetNormal();
	Hit RayIntersect(Ray ray);

private:
	Vec3 m_Points[3];
};