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
    Vec3 ambient;
    Vec3 diffuse;
    Vec3 specular;
    float specularExponent;
    float transparency;
    float refraction;
    float reflection;

    Material(std::string name, Vec3 ambient, Vec3 diffuse, Vec3 specular, float specularExponent, float transparency,
             float refraction, float reflection)
        : name(std::move(name)), ambient(ambient), diffuse(diffuse), specular(specular),
          specularExponent(specularExponent), transparency(transparency), refraction(refraction), reflection(reflection)
    {
    }

    Material(std::string name)
        : name(std::move(name)), ambient(Vec3(1.0, 1.0, 1.0)), diffuse(Vec3(1.0, 1.0, 1.0)),
          specular(Vec3(1.0, 1.0, 1.0)), specularExponent(1.0), transparency(0.0), refraction(1.0)
    {
    }

    Material() : name("default material name")
    {
    }
};
