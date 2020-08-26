#pragma once
#include "Vec3.h"

class Light
{
public:
	Vec3 inline GetPos() { return m_Pos; };
	Vec3 inline GetColor() { return m_Color; };
	Vec3 inline GetIntensity() { return m_Intensity; };
	float inline GetRadius() { return m_Radius; };

protected:
	Vec3 m_Pos;
	Vec3 m_Color;
	float m_Intensity;
	float m_Radius;
};

class PointLight : public Light
{
public:
	PointLight(Vec3 pos, Vec3 color, float i, float radius);
};
