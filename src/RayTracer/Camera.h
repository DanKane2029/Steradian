#pragma once
#include "Utils/Vec3.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

/**
 * describes the perspective the rendered image using
 * the position and direction the camera looks at
 */
struct Camera
{
    Vec3 org, dir;

    Camera() : org(Vec3()), dir(Vec3(0, 0, 1))
    {
    }

    Camera(Vec3 org, Vec3 lookAt) : org(org)
    {

        this->dir = lookAt - org;
        this->dir.normalize();
    }

    void from_json(const json &j, Camera &c)
    {
        std::cout << "camera org: " << j["org"] << std::endl;
    }
};
