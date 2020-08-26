#pragma once
#include "Vec3.h"

struct Ray
{
	Vec3 org, dir;

	Ray(Vec3 org, Vec3 dir)
	{
		this->org = org;
		this->dir = dir;
		this->dir.normalize();
	}

	Vec3 PosAt(float t)
	{
		return org + (dir * t);
	}
};
