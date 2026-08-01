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

    /**
     * \brief Estimates radiance, and reports what the ray first struck.
     *
     * The surface colour and normal at the first hit are what a denoiser needs as a
     * guide. They are essentially free to produce and, unlike the lighting, they are
     * nearly noise-free: a single sample gets them almost exactly right, because they
     * depend only on which surface is visible and not on where light came from.
     *
     * \param ray The camera ray.
     * \param rng Per-thread generator.
     * \param outAlbedo Set to the base colour of the first surface hit.
     * \param outNormal Set to the normal of the first surface hit, or zero on a miss.
     */
    auto radiance(Ray ray, Rng &rng, Vec3 &outAlbedo, Vec3 &outNormal) const -> Vec3;

  private:
    /**
     * \brief Estimates direct lighting at a surface point by sampling the emitters.
     *
     * Sampling lights explicitly, rather than hoping a randomly scattered ray happens to
     * strike one, is what keeps small bright sources from producing extreme noise. A
     * light subtending a small solid angle is hit by chance only rarely, and each such
     * hit carries enormous weight.
     */
    auto sampleDirectLighting(const Hit &hit, const Material &material, const Vec3 &albedo, const Vec3 &viewDir,
                              Rng &rng) const -> Vec3;

    /**
     * \brief Density that direct light sampling would have given a direction.
     *
     * Both estimators have to be able to evaluate each other's density, because the
     * multiple importance sampling weight is the ratio between them. This answers "if
     * light sampling had been asked to produce this exact direction, how likely was it?"
     *
     * \param from The shading point the direction was taken from.
     * \param emitterIndex Which emitter the direction reached.
     * \returns Probability density per unit solid angle, or zero if the emitter cannot
     *          be sampled from that point.
     */
    auto lightPdf(const Vec3 &from, int emitterIndex) const -> float;

    /** radiance arriving from a direction that escapes the scene */
    auto background(const Vec3 &direction) const -> Vec3;

    const Scene *m_Scene;
    unsigned int m_MaxDepth;
};
