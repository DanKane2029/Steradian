#pragma once
#include <string>
#include <utility>

#include "Utils/Vec3.h"

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
        : name(std::move(name)), color(color), ambient(ambient), diffuse(diffuse), specular(specular),
          shininess(shininess)
    {
    }

    Material(std::string name) : name(std::move(name)), ambient(1.0), diffuse(1.0), specular(1.0), shininess(1.0)
    {
        this->color = Vec3(1, 0.33, 0.2);
    }

    Material() : name("default material name")
    {
    }
};
