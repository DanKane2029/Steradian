#pragma once

#include "RayTracer/Camera.h"

struct GLFWwindow;

/**
 * \brief Turns keyboard and mouse input into camera movement.
 *
 * Kept in the viewer rather than the renderer core, since it is the only part that needs
 * to know about GLFW.
 *
 * The controller reports whether the camera actually moved, because that answer decides
 * whether the accumulated image is still valid. A progressive renderer averages samples
 * over many frames, and every one of those samples belongs to the viewpoint it was taken
 * from: as soon as the camera moves they describe a picture that no longer exists, and
 * keeping them would smear the old view into the new one.
 */
class CameraController
{
  public:
    /**
     * \param camera The camera to drive. Its current direction sets the initial
     *        orientation, so the view does not jump on the first frame.
     */
    explicit CameraController(const Camera &camera);

    /**
     * \brief Applies one frame of input to the camera.
     *
     * \param window The window to read input from.
     * \param camera The camera to update in place.
     * \param deltaTime Seconds since the previous frame, so movement speed does not
     *        depend on frame rate.
     * \returns True when the camera changed and any accumulated image must be discarded.
     */
    auto update(GLFWwindow *window, Camera &camera, float deltaTime) -> bool;

    /** movement speed in world units per second */
    auto getSpeed() const -> float
    {
        return m_Speed;
    }

  private:
    float m_Yaw = 0.0f;   ///< rotation about the world up axis, radians
    float m_Pitch = 0.0f; ///< rotation above and below the horizon, radians

    float m_Speed = 2.0f;

    double m_LastCursorX = 0.0;
    double m_LastCursorY = 0.0;
    bool m_Dragging = false;

    /** stops the view flipping over when looking straight up or down */
    static constexpr float maxPitch = 1.55334f; // just under 90 degrees
};
