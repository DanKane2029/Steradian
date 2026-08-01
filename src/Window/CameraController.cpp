#include "CameraController.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace
{

/** radians of rotation per pixel of cursor movement */
constexpr float lookSensitivity = 0.003f;

/** how much the speed changes per scroll notch, as a multiplier */
constexpr float speedStep = 1.15f;

} // namespace

CameraController::CameraController(const Camera &camera)
{
    // Recover yaw and pitch from the direction the scene file specified, so control picks
    // up exactly where the authored view left off.
    m_Yaw = std::atan2(camera.dir.x, camera.dir.z);
    m_Pitch = std::asin(std::clamp(camera.dir.y, -1.0f, 1.0f));
}

auto CameraController::update(GLFWwindow *window, Camera &camera, float deltaTime) -> bool
{
    bool moved = false;

    // Look: dragging with the left mouse button rotates the view. Rotation is tied to
    // cursor movement rather than elapsed time, so it does not depend on frame rate.
    const int mouseState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);

    if (mouseState == GLFW_PRESS)
    {
        if (!m_Dragging)
        {
            // Start of a drag: take the current position as the reference, otherwise the
            // view snaps by however far the cursor travelled since the last drag.
            m_Dragging = true;
            m_LastCursorX = cursorX;
            m_LastCursorY = cursorY;
        }

        const auto deltaX = static_cast<float>(cursorX - m_LastCursorX);
        const auto deltaY = static_cast<float>(cursorY - m_LastCursorY);

        if (deltaX != 0.0f || deltaY != 0.0f)
        {
            m_Yaw += deltaX * lookSensitivity;
            m_Pitch -= deltaY * lookSensitivity;
            m_Pitch = std::clamp(m_Pitch, -maxPitch, maxPitch);

            moved = true;
        }
    }
    else
    {
        m_Dragging = false;
    }

    m_LastCursorX = cursorX;
    m_LastCursorY = cursorY;

    // Rebuild the look direction from the accumulated angles rather than rotating the
    // previous direction, which would drift as small errors compound.
    const float cosPitch = std::cos(m_Pitch);
    const Vec3 forward(std::sin(m_Yaw) * cosPitch, std::sin(m_Pitch), std::cos(m_Yaw) * cosPitch);

    camera.dir = forward.normalized();
    camera.buildBasis(Vec3(0.0f, 1.0f, 0.0f));

    // Movement, relative to where the camera is looking.
    Vec3 motion;

    const auto held = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };

    if (held(GLFW_KEY_W))
    {
        motion += camera.dir;
    }
    if (held(GLFW_KEY_S))
    {
        motion -= camera.dir;
    }
    if (held(GLFW_KEY_D))
    {
        motion += camera.right;
    }
    if (held(GLFW_KEY_A))
    {
        motion -= camera.right;
    }
    if (held(GLFW_KEY_E))
    {
        motion += Vec3(0.0f, 1.0f, 0.0f);
    }
    if (held(GLFW_KEY_Q))
    {
        motion -= Vec3(0.0f, 1.0f, 0.0f);
    }

    // Holding shift moves faster, for crossing a large scene without changing the base
    // speed and having to change it back.
    const float speedMultiplier = (held(GLFW_KEY_LEFT_SHIFT) || held(GLFW_KEY_RIGHT_SHIFT)) ? 4.0f : 1.0f;

    if (motion.lengthSquared() > 0.0f)
    {
        camera.org += motion.normalized() * (m_Speed * speedMultiplier * deltaTime);
        moved = true;
    }

    // Bracket keys adjust the base speed, which matters because scenes here range from a
    // unit-sized Cornell box to a mesh tens of units across.
    if (held(GLFW_KEY_RIGHT_BRACKET))
    {
        m_Speed *= speedStep;
    }
    if (held(GLFW_KEY_LEFT_BRACKET))
    {
        m_Speed /= speedStep;
    }

    m_Speed = std::clamp(m_Speed, 0.01f, 1000.0f);

    return moved;
}
