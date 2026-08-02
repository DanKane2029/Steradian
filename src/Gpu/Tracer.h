#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Gpu/DeviceTypes.h"
#include "Scene/Geometry.h"
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
     * \param geometry The scene, as laid out by Stage 1. Not retained.
     * \param error Set to a description when the result is null.
     * \returns A usable tracer, or null.
     */
    static auto create(const Geometry &geometry, std::string &error) -> std::unique_ptr<Tracer>;

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

  private:
    Tracer() = default;

    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Gpu
