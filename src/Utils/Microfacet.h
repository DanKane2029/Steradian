#pragma once

#include "Utils/Sampling.h"
#include "Utils/Vec3.h"

#include <algorithm>
#include <cmath>

/**
 * \brief GGX microfacet reflection.
 *
 * Models a rough surface as a field of tiny mirrors whose orientations follow a known
 * statistical distribution. That is a physical description of roughness, unlike simply
 * jittering the mirror direction: it yields the elongated highlights and grazing-angle
 * brightening that real rough metal shows, and, more importantly here, it has an
 * evaluable probability density. Without a density a surface cannot take part in
 * multiple importance sampling at all, because there is nothing to weigh against the
 * density of sampling the lights.
 *
 * All directions are in the local shading frame, where the surface normal is +Z.
 *
 * Distribution and masking terms follow Walter et al. 2007; visible-normal sampling
 * follows Heitz 2018, "Sampling the GGX Distribution of Visible Normals".
 */
namespace Microfacet
{

/** below this roughness the lobe is narrower than sampling can resolve, so treat it as a mirror */
inline constexpr float smoothThreshold = 1e-3f;

/** converts artist-facing roughness in [0, 1] to the distribution's width parameter */
inline auto roughnessToAlpha(float roughness) -> float
{
    // Squaring makes the visual change across the slider feel more even; a linear alpha
    // spends most of its range looking almost mirror-like.
    const float r = std::clamp(roughness, 0.0f, 1.0f);
    return std::max(r * r, 1e-5f);
}

/**
 * \brief Fraction of microfacets oriented along a given half-vector.
 *
 * \param cosThetaH Cosine between the half-vector and the surface normal.
 * \param alpha Distribution width.
 */
inline auto distribution(float cosThetaH, float alpha) -> float
{
    if (cosThetaH <= 0.0f)
    {
        return 0.0f;
    }

    const float a2 = alpha * alpha;
    const float c2 = cosThetaH * cosThetaH;
    const float d = (c2 * (a2 - 1.0f)) + 1.0f;

    return a2 / (Sampling::pi * d * d);
}

/**
 * \brief Smith's lambda term: the ratio of hidden to visible microfacet area.
 */
inline auto lambda(const Vec3 &w, float alpha) -> float
{
    const float cosTheta = std::fabs(w.z);

    if (cosTheta >= 1.0f || cosTheta <= 0.0f)
    {
        return 0.0f;
    }

    const float tan2 = ((1.0f - (cosTheta * cosTheta)) / (cosTheta * cosTheta));

    return 0.5f * (std::sqrt(1.0f + (alpha * alpha * tan2)) - 1.0f);
}

/** proportion of microfacets visible from one direction */
inline auto maskingG1(const Vec3 &w, float alpha) -> float
{
    return 1.0f / (1.0f + lambda(w, alpha));
}

/**
 * \brief Proportion of microfacets visible from both directions at once.
 *
 * Height-correlated form, which accounts for the fact that a facet hidden from the
 * viewer is also likely to be hidden from the light.
 */
inline auto maskingG2(const Vec3 &wo, const Vec3 &wi, float alpha) -> float
{
    return 1.0f / (1.0f + lambda(wo, alpha) + lambda(wi, alpha));
}

/**
 * \brief Samples a microfacet normal from those actually visible along wo.
 *
 * Sampling the distribution directly wastes effort on facets that face away from the
 * viewer or are shadowed by their neighbours. Restricting to visible normals removes
 * that waste, and it is what makes rough surfaces converge at a reasonable rate.
 *
 * \param wo Outgoing direction in the local frame, pointing away from the surface.
 * \param alpha Distribution width.
 * \param u1 Uniform sample in [0, 1).
 * \param u2 Uniform sample in [0, 1).
 * \returns A unit half-vector in the local frame.
 */
inline auto sampleVisibleNormal(const Vec3 &wo, float alpha, float u1, float u2) -> Vec3
{
    // Warp to the configuration where the distribution is a hemisphere.
    const Vec3 vh = Vec3(alpha * wo.x, alpha * wo.y, wo.z).normalized();

    const float lensq = (vh.x * vh.x) + (vh.y * vh.y);
    const Vec3 t1 = (lensq > 0.0f) ? (Vec3(-vh.y, vh.x, 0.0f) / std::sqrt(lensq)) : Vec3(1.0f, 0.0f, 0.0f);
    const Vec3 t2 = vh.cross(t1);

    // Uniform point on a disc, squashed to account for the projection of the hemisphere.
    const float r = std::sqrt(u1);
    const float phi = 2.0f * Sampling::pi * u2;

    const float p1 = r * std::cos(phi);
    float p2 = r * std::sin(phi);

    const float s = 0.5f * (1.0f + vh.z);
    p2 = ((1.0f - s) * std::sqrt(std::max(0.0f, 1.0f - (p1 * p1)))) + (s * p2);

    const Vec3 nh = (t1 * p1) + (t2 * p2) + (vh * std::sqrt(std::max(0.0f, 1.0f - (p1 * p1) - (p2 * p2))));

    // Warp back.
    return Vec3(alpha * nh.x, alpha * nh.y, std::max(1e-6f, nh.z)).normalized();
}

/**
 * \brief Density of sampling wi, per unit solid angle.
 *
 * This is what lets a rough surface participate in multiple importance sampling.
 *
 * \param wo Outgoing direction, local frame.
 * \param wi Incoming direction, local frame.
 * \param alpha Distribution width.
 */
inline auto pdf(const Vec3 &wo, const Vec3 &wi, float alpha) -> float
{
    if (wo.z <= 0.0f || wi.z <= 0.0f)
    {
        return 0.0f;
    }

    const Vec3 h = (wo + wi).normalized();

    const float dotWoH = wo.dot(h);
    if (dotWoH <= 0.0f)
    {
        return 0.0f;
    }

    // Density of visible normals, converted from half-vector to incoming direction by
    // the Jacobian of the reflection, which is 1 / (4 dot(wo, h)).
    const float dVisible = maskingG1(wo, alpha) * dotWoH * distribution(h.z, alpha) / wo.z;

    return dVisible / (4.0f * dotWoH);
}

/**
 * \brief The scalar part of the reflectance for a given pair of directions.
 *
 * Fresnel is left out so the caller can apply the surface tint per channel. Multiply by
 * Fresnel and by the incoming cosine to get the contribution of a light sample.
 *
 * \param wo Outgoing direction, local frame.
 * \param wi Incoming direction, local frame.
 * \param alpha Distribution width.
 */
inline auto evaluate(const Vec3 &wo, const Vec3 &wi, float alpha) -> float
{
    if (wo.z <= 0.0f || wi.z <= 0.0f)
    {
        return 0.0f;
    }

    const Vec3 h = (wo + wi).normalized();

    const float d = distribution(h.z, alpha);
    const float g = maskingG2(wo, wi, alpha);

    return (d * g) / (4.0f * wo.z * wi.z);
}

/**
 * \brief The fraction of incoming light this lobe reflects, with Fresnel taken as one.
 *
 * Below one, and increasingly so as roughness rises, because the model follows only a
 * single bounce off the surface and discards light that would have bounced between facets
 * before escaping. Used to put that energy back.
 *
 * \param cosThetaO Cosine between the view direction and the surface normal.
 * \param roughness Surface roughness in [0, 1].
 */
auto directionalAlbedo(float cosThetaO, float roughness) -> float;

/**
 * \brief The reflectance weight to carry when a direction was drawn from sampleVisibleNormal.
 *
 * The full expression is the distribution times masking times Fresnel over the two
 * cosines; sampling visible normals cancels almost all of it, leaving only the ratio of
 * two-sided to one-sided masking. Fresnel is applied by the caller, which knows the
 * surface tint.
 */
inline auto visibleNormalWeight(const Vec3 &wo, const Vec3 &wi, float alpha) -> float
{
    const float g1 = maskingG1(wo, alpha);

    if (g1 <= 0.0f)
    {
        return 0.0f;
    }

    return maskingG2(wo, wi, alpha) / g1;
}

} // namespace Microfacet
