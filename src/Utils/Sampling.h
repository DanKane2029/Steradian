#pragma once

#include "Utils/DeviceCompat.h"
#include "Utils/Random.h"
#include "Utils/Vec3.h"

/**
 * \brief Sampling routines used by the path integrator.
 *
 * Every function here returns directions together with the probability density they were
 * drawn from, because a Monte Carlo estimator is only unbiased if the sample is divided
 * by the density that produced it.
 *
 * Compiled unchanged as device code, so both backends sample from the same routines
 * rather than from two implementations that agree until one of them is edited.
 */
namespace Sampling
{

inline constexpr float pi = 3.14159265358979323846f;

/**
 * \brief Builds an orthonormal basis around a unit vector.
 *
 * Uses the branchless construction from Duff et al., which stays well conditioned even
 * when the input points close to an axis. Naively crossing with a fixed axis degenerates
 * there and produces NaNs.
 *
 * \param n The unit vector to use as the third basis vector.
 * \param t Set to a unit vector perpendicular to n.
 * \param b Set to a unit vector perpendicular to both.
 */
inline PT_HOST_DEVICE void buildBasis(const Vec3 &n, Vec3 &t, Vec3 &b)
{
    const float sign = copysignf(1.0f, n.z);
    const float a = -1.0f / (sign + n.z);
    const float c = n.x * n.y * a;

    t = Vec3(1.0f + sign * n.x * n.x * a, sign * c, -sign * n.x);
    b = Vec3(c, sign + n.y * n.y * a, -n.y);
}

/** transforms a direction from the local frame of (t, b, n) into world space */
inline PT_HOST_DEVICE auto toWorld(const Vec3 &local, const Vec3 &t, const Vec3 &b, const Vec3 &n) -> Vec3
{
    return (t * local.x) + (b * local.y) + (n * local.z);
}

/**
 * \brief The i'th value of a radical inverse sequence in the given base.
 *
 * These sequences are progressive: every prefix of one covers the interval evenly, not
 * just the whole. That is the property adaptive sampling needs, because it stops at a
 * point decided while running and cannot know in advance how many samples a pixel will
 * receive.
 *
 * A grid of strata does not have this property. Its coverage is only even once every cell
 * has been visited, so stopping partway through leaves the samples bunched into whichever
 * cells came first, and the pixel is measured from a sliver of its own area.
 */
inline PT_HOST_DEVICE auto radicalInverse(uint32_t i, uint32_t base) -> float
{
    float inverseBase = 1.0f / static_cast<float>(base);
    float scale = inverseBase;
    float result = 0.0f;

    while (i > 0)
    {
        result += static_cast<float>(i % base) * scale;
        i /= base;
        scale *= inverseBase;
    }

    return result;
}

/**
 * \brief A progressive 2D sample, offset so neighbouring pixels do not share a sequence.
 *
 * Without the offset every pixel would take the same points and their errors would line up
 * into visible structure rather than looking like noise.
 *
 * \param index Which sample in the sequence.
 * \param scramble Per-pixel offset.
 */
inline PT_HOST_DEVICE auto haltonSample(uint32_t index, uint32_t scramble, float &x, float &y)
{
    x = radicalInverse(index + (scramble & 0xffffu), 2);
    y = radicalInverse(index + (scramble >> 16u), 3);
}

/**
 * \brief Samples a direction on the hemisphere, proportional to the cosine term.
 *
 * Weighting by cosine matters because the rendering equation already contains that
 * factor. Drawing from it means the cosine cancels against the density, so a Lambertian
 * bounce reduces to multiplying by albedo with no other terms and no variance from the
 * cosine at all.
 *
 * The returned density is cos(theta) / pi.
 */
inline PT_HOST_DEVICE auto cosineHemisphere(Rng &rng, float &pdf) -> Vec3
{
    // Concentric mapping of the unit square to the unit disc, then lifted to the
    // hemisphere. This distorts less than the naive polar mapping.
    const float u1 = (2.0f * rng.nextFloat()) - 1.0f;
    const float u2 = (2.0f * rng.nextFloat()) - 1.0f;

    float radius = 0.0f;
    float theta = 0.0f;

    if (u1 != 0.0f || u2 != 0.0f)
    {
        if (fabsf(u1) > fabsf(u2))
        {
            radius = u1;
            theta = (pi / 4.0f) * (u2 / u1);
        }
        else
        {
            radius = u2;
            theta = (pi / 2.0f) - ((pi / 4.0f) * (u1 / u2));
        }
    }

    const float x = radius * cosf(theta);
    const float y = radius * sinf(theta);
    const float z = sqrtf(Math::max(0.0f, 1.0f - (x * x) - (y * y)));

    pdf = z / pi;

    return {x, y, z};
}

/**
 * \brief Samples a point uniformly over the surface of a sphere.
 *
 * Used for direct lighting towards spherical emitters. Sampling the whole sphere rather
 * than only the visible cap is simpler and stays correct, since points facing away
 * contribute nothing once the geometry term is applied.
 */
inline PT_HOST_DEVICE auto uniformSphere(Rng &rng) -> Vec3
{
    const float z = 1.0f - (2.0f * rng.nextFloat());
    const float r = sqrtf(Math::max(0.0f, 1.0f - (z * z)));
    const float phi = 2.0f * pi * rng.nextFloat();

    return {r * cosf(phi), r * sinf(phi), z};
}

/**
 * \brief Samples a direction uniformly inside a cone.
 *
 * \param rng Per-thread generator.
 * \param cosThetaMax Cosine of the cone's half angle.
 * \param axis Unit vector along the cone's axis.
 */
inline PT_HOST_DEVICE auto uniformCone(Rng &rng, float cosThetaMax, const Vec3 &axis) -> Vec3
{
    const float cosTheta = 1.0f - (rng.nextFloat() * (1.0f - cosThetaMax));
    const float sinTheta = sqrtf(Math::max(0.0f, 1.0f - (cosTheta * cosTheta)));
    const float phi = 2.0f * pi * rng.nextFloat();

    Vec3 t;
    Vec3 b;
    buildBasis(axis, t, b);

    return toWorld(Vec3(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta), t, b, axis);
}

/**
 * \brief Reflects a direction about a normal.
 *
 * \param d Incoming direction, pointing towards the surface.
 * \param n Unit surface normal.
 */
inline PT_HOST_DEVICE auto reflect(const Vec3 &d, const Vec3 &n) -> Vec3
{
    return d - (n * (2.0f * d.dot(n)));
}

/**
 * \brief Refracts a direction through a surface, if refraction is possible.
 *
 * \param d Incoming unit direction, pointing towards the surface.
 * \param n Unit surface normal facing against d.
 * \param eta Ratio of the incident to the transmitted index of refraction.
 * \param refracted Set to the transmitted direction when the function returns true.
 * \returns False under total internal reflection, where no transmitted direction exists.
 */
inline PT_HOST_DEVICE auto refract(const Vec3 &d, const Vec3 &n, float eta, Vec3 &refracted) -> bool
{
    const float cosI = -d.dot(n);
    const float sin2T = eta * eta * (1.0f - (cosI * cosI));

    if (sin2T > 1.0f)
    {
        return false;
    }

    const float cosT = sqrtf(1.0f - sin2T);
    refracted = (d * eta) + (n * ((eta * cosI) - cosT));

    return true;
}

/**
 * \brief Fresnel reflectance for a dielectric, using Schlick's approximation.
 *
 * \param cosTheta Cosine of the angle between the view direction and the normal.
 * \param r0 Reflectance at normal incidence.
 */
inline PT_HOST_DEVICE auto fresnelSchlick(float cosTheta, float r0) -> float
{
    const float m = Math::min(Math::max(1.0f - cosTheta, 0.0f), 1.0f);
    const float m2 = m * m;

    return r0 + ((1.0f - r0) * m2 * m2 * m);
}

} // namespace Sampling
