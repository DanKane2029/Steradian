#pragma once
#include "Vec3.h"

class Light
{
protected:
	Vec3 m_Pos;
	Vec3 m_Color;
};

class PointLight : public Light
{
public:
	PointLight(Vec3 pos, Vec3 color);
};