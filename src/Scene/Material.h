#pragma once
#include "Vec3.h"

#include <string>

struct Material
{
	std::string name;

	Vec3 color;

	Material(std::string& name, Vec3& color)
	{
		this->name = name;

		this->color = color;
	}
};