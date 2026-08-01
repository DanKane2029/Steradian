#pragma once
#include "Utils/Vec3.h"

#include <cmath>

/**
 * \brief The camera used to render the image of a scene.
 *
 * Holds the position, orientation and field of view of the viewpoint, and builds the
 * orthonormal basis that primary rays are generated in.
 */
struct Camera
{
  public:
    /** the position of the camera within the scene */
    Vec3 org;

    /** unit vector pointing along the direction the camera looks */
    Vec3 dir;

    /** unit vector pointing to the camera's right */
    Vec3 right;

    /** unit vector pointing up in the camera's frame, perpendicular to dir and right */
    Vec3 up;

    /** default vertical field of view, 60 degrees in radians */
    static constexpr float defaultFovY = 1.0471976f;

    /** vertical field of view in radians */
    float fovY = defaultFovY;

    /**
     * \brief Creates a camera at the origin looking along +Z.
     */
    Camera() : dir(Vec3(0, 0, 1))
    {
        buildBasis(Vec3(0.0f, 1.0f, 0.0f));
    }

    /**
     * \brief Creates a camera positioned at org and aimed at lookAt.
     *
     * \param org The position of the camera.
     * \param lookAt The point the camera is aimed at.
     * \param fovY The vertical field of view in radians.
     * \param worldUp The world up direction used to orient the camera's roll.
     */
    Camera(Vec3 org, Vec3 lookAt, float fovY = defaultFovY, Vec3 worldUp = Vec3(0.0f, 1.0f, 0.0f))
        : org(org), fovY(fovY)
    {
        this->dir = (lookAt - org).normalized();
        buildBasis(worldUp);
    }

    /**
     * \brief Builds the orthonormal camera basis from the look direction.
     *
     * Primary rays are offset along `right` and `up` rather than along world X and Y,
     * which is what lets the camera be oriented in any direction.
     *
     * \param worldUp The reference up direction.
     */
    void buildBasis(Vec3 worldUp)
    {
        // Looking straight along worldUp leaves the cross product undefined, so fall back
        // to another reference axis rather than producing NaNs.
        if (std::fabs(dir.dot(worldUp.normalized())) > 0.9999f)
        {
            worldUp = Vec3(0.0f, 0.0f, 1.0f);
        }

        right = dir.cross(worldUp).normalized();
        up = right.cross(dir).normalized();
    }
};
