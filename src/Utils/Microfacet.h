#pragma once

#include "Utils/DeviceCompat.h"
#include "Utils/Sampling.h"
#include "Utils/Vec3.h"

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
 *
 * Compiled unchanged as device code. The one part that cannot be is the energy
 * compensation table, which is measured at run time by sampling these very routines: see
 * directionalAlbedo below, where the measuring and the reading are separated so that only
 * the reading has to cross.
 */
namespace Microfacet
{

/** below this roughness the lobe is narrower than sampling can resolve, so treat it as a mirror */
inline constexpr float smoothThreshold = 1e-3f;

/** converts artist-facing roughness in [0, 1] to the distribution's width parameter */
inline PT_HOST_DEVICE auto roughnessToAlpha(float roughness) -> float
{
    // Squaring makes the visual change across the slider feel more even; a linear alpha
    // spends most of its range looking almost mirror-like.
    const float r = Math::clamp(roughness, 0.0f, 1.0f);
    return Math::max(r * r, 1e-5f);
}

/**
 * \brief Fraction of microfacets oriented along a given half-vector.
 *
 * \param cosThetaH Cosine between the half-vector and the surface normal.
 * \param alpha Distribution width.
 */
inline PT_HOST_DEVICE auto distribution(float cosThetaH, float alpha) -> float
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
inline PT_HOST_DEVICE auto lambda(const Vec3 &w, float alpha) -> float
{
    const float cosTheta = fabsf(w.z);

    if (cosTheta >= 1.0f || cosTheta <= 0.0f)
    {
        return 0.0f;
    }

    const float tan2 = ((1.0f - (cosTheta * cosTheta)) / (cosTheta * cosTheta));

    return 0.5f * (sqrtf(1.0f + (alpha * alpha * tan2)) - 1.0f);
}

/** proportion of microfacets visible from one direction */
inline PT_HOST_DEVICE auto maskingG1(const Vec3 &w, float alpha) -> float
{
    return 1.0f / (1.0f + lambda(w, alpha));
}

/**
 * \brief Proportion of microfacets visible from both directions at once.
 *
 * Height-correlated form, which accounts for the fact that a facet hidden from the
 * viewer is also likely to be hidden from the light.
 */
inline PT_HOST_DEVICE auto maskingG2(const Vec3 &wo, const Vec3 &wi, float alpha) -> float
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
inline PT_HOST_DEVICE auto sampleVisibleNormal(const Vec3 &wo, float alpha, float u1, float u2) -> Vec3
{
    // Warp to the configuration where the distribution is a hemisphere.
    const Vec3 vh = Vec3(alpha * wo.x, alpha * wo.y, wo.z).normalized();

    const float lensq = (vh.x * vh.x) + (vh.y * vh.y);
    const Vec3 t1 = (lensq > 0.0f) ? (Vec3(-vh.y, vh.x, 0.0f) / sqrtf(lensq)) : Vec3(1.0f, 0.0f, 0.0f);
    const Vec3 t2 = vh.cross(t1);

    // Uniform point on a disc, squashed to account for the projection of the hemisphere.
    const float r = sqrtf(u1);
    const float phi = 2.0f * Sampling::pi * u2;

    const float p1 = r * cosf(phi);
    float p2 = r * sinf(phi);

    const float s = 0.5f * (1.0f + vh.z);
    p2 = ((1.0f - s) * sqrtf(Math::max(0.0f, 1.0f - (p1 * p1)))) + (s * p2);

    const Vec3 nh = (t1 * p1) + (t2 * p2) + (vh * sqrtf(Math::max(0.0f, 1.0f - (p1 * p1) - (p2 * p2))));

    // Warp back.
    return Vec3(alpha * nh.x, alpha * nh.y, Math::max(1e-6f, nh.z)).normalized();
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
inline PT_HOST_DEVICE auto pdf(const Vec3 &wo, const Vec3 &wi, float alpha) -> float
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
inline PT_HOST_DEVICE auto evaluate(const Vec3 &wo, const Vec3 &wi, float alpha) -> float
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

/** side length of the square energy compensation table, in both roughness and cosine */
inline constexpr int albedoResolution = 32;

/**
 * \brief Reads the fraction of incoming light this lobe reflects, Fresnel taken as one.
 *
 * Below one, and increasingly so as roughness rises, because the model follows only a
 * single bounce off the surface and discards light that would have bounced between facets
 * before escaping. Used to put that energy back.
 *
 * The table is measured rather than fitted, by sampling this file's own scattering
 * routines, so it describes the lobe this renderer actually has. Measuring it needs a
 * generator, a few million samples and somewhere to put the result, none of which belongs
 * on a device; reading it is a bilinear lookup. Only the reading is shared, and the table
 * is passed in so a device can be handed its own copy of the same numbers.
 *
 * \param table albedoResolution squared floats, row major in roughness.
 * \param cosThetaO Cosine between the view direction and the surface normal.
 * \param roughness Surface roughness in [0, 1].
 */
inline PT_HOST_DEVICE auto directionalAlbedo(const float *table, float cosThetaO, float roughness) -> float
{
    const float r = (Math::clamp(roughness, 0.0f, 1.0f) * albedoResolution) - 0.5f;
    const float c = (Math::clamp(cosThetaO, 0.0f, 1.0f) * albedoResolution) - 0.5f;

    const int r0 = Math::clamp(static_cast<int>(floorf(r)), 0, albedoResolution - 1);
    const int c0 = Math::clamp(static_cast<int>(floorf(c)), 0, albedoResolution - 1);
    const int r1 = Math::min(r0 + 1, albedoResolution - 1);
    const int c1 = Math::min(c0 + 1, albedoResolution - 1);

    const float fr = Math::clamp(r - static_cast<float>(r0), 0.0f, 1.0f);
    const float fc = Math::clamp(c - static_cast<float>(c0), 0.0f, 1.0f);

    const float a = table[(r0 * albedoResolution) + c0];
    const float b = table[(r0 * albedoResolution) + c1];
    const float d = table[(r1 * albedoResolution) + c0];
    const float e = table[(r1 * albedoResolution) + c1];

    const float top = a + ((b - a) * fc);
    const float bottom = d + ((e - d) * fc);

    // Never report zero: the caller divides by this.
    return Math::max(top + ((bottom - top) * fr), 1e-3f);
}

#ifndef __CUDACC_RTC__

/**
 * \brief The host's table, measured once on first use.
 *
 * \returns albedoResolution squared floats, the same layout the lookup above expects.
 */
auto hostAlbedoTable() -> const float *;

/** \brief directionalAlbedo against the host's own table. */
inline auto directionalAlbedo(float cosThetaO, float roughness) -> float
{
    return directionalAlbedo(hostAlbedoTable(), cosThetaO, roughness);
}

#endif

/**
 * \brief The reflectance weight to carry when a direction was drawn from sampleVisibleNormal.
 *
 * The full expression is the distribution times masking times Fresnel over the two
 * cosines; sampling visible normals cancels almost all of it, leaving only the ratio of
 * two-sided to one-sided masking. Fresnel is applied by the caller, which knows the
 * surface tint.
 */
inline PT_HOST_DEVICE auto visibleNormalWeight(const Vec3 &wo, const Vec3 &wi, float alpha) -> float
{
    const float g1 = maskingG1(wo, alpha);

    if (g1 <= 0.0f)
    {
        return 0.0f;
    }

    return maskingG2(wo, wi, alpha) / g1;
}

} // namespace Microfacet
