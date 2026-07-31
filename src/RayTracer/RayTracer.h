#pragma once
#include <cstdint>
#include <mutex>

#include "Camera.h"
#include "Hit.h"
#include "Scene/Scene.h"
#include "Utils/Config.h"
#include "Utils/Random.h"
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
     * \brief Renders a horizontal band of the image to completion.
     *
     * Renders every pixel in rows [yStart, yEnd) with a fixed number of samples each.
     * Unlike sampleScene this covers its region exhaustively and deterministically: the
     * generator is seeded from the render seed and the row, so the result depends only
     * on (seed, samplesPerPixel) and not on thread count or scheduling.
     *
     * Each band must be rendered by at most one thread. Bands do not overlap, so no
     * synchronization on the pixel buffer is required.
     *
     * \param yStart The first row of the band, inclusive.
     * \param yEnd The last row of the band, exclusive.
     * \param samplesPerPixel The number of samples to average per pixel.
     * \param seed The base seed for the render.
     */
    void renderRows(int yStart, int yEnd, unsigned int samplesPerPixel, uint64_t seed);

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
    auto getHitColor(Hit hit, unsigned int recurseLevel, Rng &rng) -> Vec3;

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
    auto shootShadowRays(std::shared_ptr<Light> light, Vec3 pos, Rng &rng) -> float;

    void updateAspectRatio(float aspectRatio);

  private:
    /**
     * \brief Computes the color of a single sample through the given pixel.
     *
     * \param ix The pixel column.
     * \param iy The pixel row.
     * \param jitterX Sub-pixel horizontal offset in [0, 1).
     * \param jitterY Sub-pixel vertical offset in [0, 1).
     * \param rng The generator to use for stochastic effects such as soft shadows.
     * \returns The linear color of the sample.
     */
    auto samplePixel(int ix, int iy, float jitterX, float jitterY, Rng &rng) -> Vec3;

    /**
     * \brief Builds the primary camera ray through a point on the film plane.
     *
     * \param u Horizontal film coordinate in [0, 1).
     * \param v Vertical film coordinate in [0, 1).
     */
    auto makeCameraRay(float u, float v) -> Ray;

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