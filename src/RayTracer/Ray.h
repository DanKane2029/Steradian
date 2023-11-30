#pragma once
#include "Utils/Vec3.h"

/**
 * a point and a direction used to simulate a light ray
 */
struct Ray
{
    Vec3 org, dir;

    Ray(Vec3 org, Vec3 dir) : org(org), dir(dir)
    {

        this->dir.normalize();
    }

    auto posAt(float t) -> Vec3
    {
        return org + (dir * t);
    }
};
