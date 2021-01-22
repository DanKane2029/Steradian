#pragma once
#include <string>

/**
 * a set of data that describes a ray-scene object interesction
 */
struct Hit
{
	bool isHit;
	float time;

	std::string materialName;

	Vec3 position;
	Vec3 normal;

	Vec3 color;

	Hit()
	{
		isHit = false;
		time = INFINITY;

		materialName;

		position = Vec3();
		normal = Vec3();
	}
};
