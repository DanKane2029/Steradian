#pragma once
#include "Vec3.h"

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

	Material(std::string& name, Vec3& color, float ambient, float diffuse, float specular, float shininess)
	{
		this->name = name;
		this->color = color;
		
		this->ambient = ambient;
		this->diffuse = diffuse;
		this->specular = specular;

		this->shininess = shininess;
	}
};
