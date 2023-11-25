#pragma once
#include "Utils/Vec3.h"

#include <string>

/**
 * a set of data that describes the surface of a scene object
 */
struct Material
{
	std::string name;

	Vec3 color;
	float ambient, diffuse, specular;
	float shininess;

	Material(std::string name, Vec3 color, float ambient, float diffuse, float specular, float shininess)
	{
		this->name = name;
		this->color = color;

		this->ambient = ambient;
		this->diffuse = diffuse;
		this->specular = specular;

		this->shininess = shininess;
	}

	Material(std::string name) : name(name)
	{
		this->color = Vec3(1, 0.33, 0.2);

		this->ambient = 1.0;
		this->diffuse = 1.0;
		this->specular = 1.0;

		this->shininess = 1.0;
	}
};
