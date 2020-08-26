#pragma once
#include "Vec3.h"

struct Camera
{
	Vec3 org, dir;

	Camera(Vec3 org, Vec3 lookAt)
	{
		this->org = org;

		this->dir = lookAt - org;
		this->dir.normalize();
	}
};
