#pragma once

#include "RayTracer/Hit.h"
#include "RayTracer/Ray.h"
#include "Scene/Scene.h"
#include "Utils/Random.h"
#include "Utils/Vec3.h"

/**
 * \brief Monte Carlo path tracing integrator.
 *
 * Estimates the rendering equation by following paths from the camera and averaging many
 * such paths per pixel. Unlike the direct lighting model this replaced, light that has
 * bounced off other surfaces is carried along the path, so indirect illumination and
 * colour bleeding fall out of the method rather than being approximated by a constant
 * ambient term.
 *
 * The loop is iterative rather than recursive, carrying a throughput term. Recursion
 * would bound path length by the stack, and throughput is what lets Russian roulette
 * terminate paths without bias.
 */
class Integrator
{
  public:
    /**
     * \param scene The scene to trace against.
     * \param maxDepth Longest path to follow before giving up, counted in bounces.
     */
    Integrator(const Scene *scene, unsigned int maxDepth) : m_Scene(scene), m_MaxDepth(maxDepth)
    {
    }

    /**
     * \brief Estimates the radiance arriving along a ray.
     *
     * \param ray The camera ray.
     * \param rng Per-thread generator.
     * \returns Linear radiance. Values may exceed one and are tone mapped on output.
     */
    auto radiance(Ray ray, Rng &rng) const -> Vec3;

  private:
    /**
     * \brief Estimates direct lighting at a surface point by sampling the emitters.
     *
     * Sampling lights explicitly, rather than hoping a randomly scattered ray happens to
     * strike one, is what keeps small bright sources from producing extreme noise. A
     * light subtending a small solid angle is hit by chance only rarely, and each such
     * hit carries enormous weight.
     */
    auto sampleDirectLighting(const Hit &hit, const Material &material, const Vec3 &albedo, Rng &rng) const -> Vec3;

    /** radiance arriving from a direction that escapes the scene */
    auto background(const Vec3 &direction) const -> Vec3;

    const Scene *m_Scene;
    unsigned int m_MaxDepth;
};
