#pragma once
#include "Utils/Vec3.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

/**
 * \brief The camera used to render the image of a scene.
 *
 * Describes the perspective of the rendered image using the position and direction the camera looks at.
 */
struct Camera
{
  public:
    /**
     * \brief The origin of the camera.
     *
     * The Vector3 that describes the position of the camera within the scene.
     */
    Vec3 org;

    /**
     * \brief The direction the camera is looking.
     *
     * The Vector3 that describes the direction the camera is looking.
     */
    Vec3 dir;

    /**
     * \brief Creates a new Camera.
     *
     * Creates a new Camera object with origin at (0, 0, 0) and direction (0, 0, 1).
     */
    Camera() : dir(Vec3(0, 0, 1))
    {
    }

    /**
     * \brief Creates a new Camera with origin and direction.
     *
     * Creates a new Camera object with 'org' defining the camera origin and 'lookAt' used to define the camera
     * direction.
     *
     * \param org The origin or position of the camera.
     * \param lookAt The point the camera is looking at. Used to calculate the camera's direction.
     */
    Camera(Vec3 org, Vec3 lookAt) : org(org)
    {
        this->dir = lookAt - org;
        this->dir.normalize();
    }
};
