#pragma once
#include "Vec3.h"

/**
 * describes the perspective the rendered image using
 * the position and direction the camera looks at
 */
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
