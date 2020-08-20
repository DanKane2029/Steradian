#pragma once
#include <string>

struct Hit
{
	bool isHit;
	float time;

	std::string materialName;

	Vec3 position;
	Vec3 normal;

	Hit()
	{
		isHit = false;
		time = 0.0f;

		materialName;

		position = Vec3();
		normal = Vec3();
	}
};