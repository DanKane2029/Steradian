#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Gpu/DeviceTypes.h"
#include "Scene/Scene.h"
#include "Utils/Vec3.h"

namespace Gpu
{

/**
 * \brief Ray traversal on the GPU, through OptiX.
 *
 * Holds everything one scene needs on the device: the acceleration structure, the
 * compiled programs, and the buffers they read. Built once per scene and then traced
 * against many times.
 *
 * OptiX is used in its simplest useful shape. There is one raygen program, one miss
 * program, and one hit group per kind of primitive, and the hit groups report nothing but
 * a distance, a pair of barycentrics and an index. That avoids nearly all of the shader
 * binding table machinery that makes OptiX look forbidding, while still putting the
 * traversal itself on the RT cores, which is the entire reason for being here.
 *
 * Triangles use OptiX's built-in intersection so the hardware can do the work. Spheres
 * are custom primitives intersected by a program that mirrors the CPU's own test, because
 * using the built-in sphere would mean maintaining a second implementation of something
 * this project already has.
 */
class Tracer
{
  public:
    /**
     * \brief Builds everything needed to trace against a scene.
     *
     * Diagnoses rather than throws, because every step here can fail for an environmental
     * reason -- no device, a driver too old, a kernel that would not compile -- and those
     * want reporting rather than unwinding.
     *
     * \param scene The scene. Copied to the device; not retained.
     * \param error Set to a description when the result is null.
     * \returns A usable tracer, or null.
     */
    static auto create(const Scene &scene, std::string &error) -> std::unique_ptr<Tracer>;

    ~Tracer();

    Tracer(const Tracer &) = delete;
    auto operator=(const Tracer &) -> Tracer & = delete;

    /**
     * \brief Traces a batch of rays and reports the closest hit for each.
     *
     * Batched rather than one at a time because a GPU is only worth addressing in bulk:
     * this machine can keep 58,368 threads resident, and a launch of one ray would use
     * one of them.
     *
     * \param origins One per ray.
     * \param directions One per ray, expected normalized, matching origins in length.
     * \param tMin Near bound, as the CPU's Ray carries.
     * \param tMax Far bound.
     * \param hits Resized to the ray count and filled.
     * \returns False if the launch failed.
     */
    auto trace(const std::vector<Vec3> &origins, const std::vector<Vec3> &directions, float tMin, float tMax,
               std::vector<DeviceHit> &hits) -> bool;

    /**
     * \brief Renders the whole image.
     *
     * One thread per pixel, each taking every sample for it, so the result does not
     * depend on how the launch was divided up. It will not match the CPU pixel for pixel
     * and is not meant to: the two backends seed their generators differently, because
     * the CPU's per-row stream cannot be reproduced by threads running at once. They
     * converge to the same image rather than to the same noise.
     *
     * Successive calls accumulate unless restart is asked for, so an interactive view can
     * add a few samples per frame and show the average of everything taken since the
     * camera last moved. The sums stay on the device; only the resolved average is
     * copied back.
     *
     * \param camera Where the view is now. Passed per call rather than fixed at build,
     *        since moving it is the entire point of an interactive view.
     * \param width Image width in pixels.
     * \param height Image height.
     * \param samplesPerPixel Samples to add in this call.
     * \param seed Base seed; the same seed reproduces the same image.
     * \param maxDepth Longest path to follow, in bounces.
     * \param restart True to discard what has accumulated and begin again. Required when
     *        the camera moves: those samples measured light arriving somewhere else.
     * \param colour Resized to width * height and filled with linear radiance.
     * \param albedo Filled with the first hit's surface colour, for a denoiser.
     * \param normal Filled with the first hit's normal.
     * \returns False if the launch failed.
     */
    auto render(const Camera &camera, int width, int height, unsigned int samplesPerPixel, unsigned long long seed,
                unsigned int maxDepth, bool restart, std::vector<Vec3> &colour, std::vector<Vec3> &albedo,
                std::vector<Vec3> &normal) -> bool;

    /** \brief Samples accumulated since the last restart. */
    auto accumulatedSamples() const -> unsigned int;

  private:
    Tracer() = default;

    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Gpu
