#pragma once
#include <mutex>

#include "Camera.h"
#include "Hit.h"
#include "Scene/Scene.h"
#include "Utils/Config.h"
#include "Window/PixelBuffer.h"

/**
 * \brief Class that shoots rays into a scene and calculates light values.
 *
 * A class that writes color values to a pixel buffer. The color value is determined by shooting a ray into the scene
 * and calculating the resulting intersection based on the Blinn-Phong lighting model.
 */
class RayTracer
{
  public:
    /**
     * \brief Creates a RayTracer object.
     *
     * Creates a RayTracer object with a pixel buffer to write to and a scene to render.
     *
     * \param pixelBuffer The pixel buffer that the ray tracer will write to.
     * \param scene The scene that the ray tracer will shoot rays into.
     * \param config The configuration object to configure the  ray tracer parameters.
     */
    RayTracer(PixelBuffer *pixelBuffer, Scene *scene, Config &config);

    /**
     * \brief Destroys the RayTracer object.
     *
     * Destroys a RayTracer object.
     */
    ~RayTracer();

    /**
     * \brief Calculates the color at one point in the window and writes to the pixel buffer.
     *
     * Calculates the color at the point (x, y) in the window. The point (0, 0) represents the top left corner of the
     * screen and the point (1, 1) represents the bottom right coner of the window.
     *
     * \param x The horizontal coordinate of the point to calculate between 0 and 1.
     * \param y The vertical coordinate of the point to calculate between 0 and 1.
     */
    void sampleScene(float x, float y);

    /**
     * \brief Shoots a ray into the scene.
     *
     * Shoots a ray into the scene and returns a Hit object that describes the ray object intersection.
     *
     * \param ray The ray being shot into the scene.
     * \returns The hit object that describes the intersection if there is one.
     */
    auto shootRay(Ray ray) -> Hit;

    /**
     * \brief Calculates the color of a ray scene intersection.
     *
     * Calculates the color of a ray scene object intersection using the Blinn-Phong lighting calculation. The hit
     * object is expected to be a real intersection between ray and scene object so the hit's 'isHit' member should
     * always be true.
     *
     * \param hit The hit to calculate the color of.
     * \param recurseLevel The current level of recursion used for reflection calculations.
     * \returns The color of the hit as a Vector3.
     */
    auto getHitColor(Hit hit, unsigned int recurseLevel) -> Vec3;

    /**
     * \brief Calculates if the position is in a shadow.
     *
     * Shoots multiple rays from a position in a cone towards a light source to see if that position is shadowed.
     *
     * \param light The light source that potentially casts a shadow on the position.
     * \param pos The position where the rays are cast from to determine if it is in shadow.
     * \returns A float value that determines how much the position is in shadow. 0 is completely in shadow and 1 is
     * completly lit.
     */
    auto shootShadowRays(std::shared_ptr<Light> light, Vec3 pos) -> float;

    void updateAspectRatio(float aspectRatio);

  private:
    unsigned int m_NumShadowRays = 5;
    unsigned int m_ReflectionLimit = 100;
    unsigned int m_MaxRecurseLevel = 10;

    float m_FovX{}, m_FovY{};
    float m_aspectRatio;
    Camera m_Camera;

    Scene *m_Scene{};
    PixelBuffer *m_PixelBuffer{};
    std::mutex m_PixelBufferGuard;

    float vx;
    float vy;
};