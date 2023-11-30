#pragma once
#include "Utils/Vec3.h"
#include <string>

/**
 * a set of data that describes a ray-scene object interesction
 */
struct Hit
{
    bool isHit{false};
    float time;

    std::string materialName;

    Vec3 position{};
    Vec3 normal{};

    Vec3 color{};

    Hit() : time(INFINITY)
    {

        materialName;

        position = Vec3();
        normal = Vec3();
    }
};
