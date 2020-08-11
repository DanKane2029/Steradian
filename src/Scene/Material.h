#pragma once
#include "Vec3.h"

struct Material
{
	Vec3 color;

	Material(Vec3& c)
	{
		color = c;
	}
};