#include "Integrator.h"

#include "Utils/Sampling.h"
#include "Utils/Stats.h"

#include <algorithm>
#include <cmath>

namespace
{

/** depth after which paths start being terminated randomly */
constexpr unsigned int russianRouletteStart = 3;

/** largest survival probability used by Russian roulette */
constexpr float maxSurvivalProbability = 0.95f;

} // namespace

auto Integrator::background(const Vec3 &direction) const -> Vec3
{
    (void)direction;

    // A uniform environment. The old renderer's flat ambient term is reinterpreted as
    // light arriving from every direction, which is what it was always pretending to be,
    // except that now it is occluded by geometry and bounces like any other light.
    return m_Scene->getAmbientLighting();
}

auto Integrator::sampleDirectLighting(const Hit &hit, const Material &material, const Vec3 &albedo,
                                      Rng &rng) const -> Vec3
{
    // Only diffuse surfaces gather light this way. A mirror reflects exactly one
    // direction, so a randomly chosen point on a light almost never lies along it, and
    // the contribution would be zero with probability one.
    if (material.type != Material::Type::Diffuse)
    {
        return {};
    }

    const std::vector<Scene::Emitter> &emitters = m_Scene->getEmitters();
    if (emitters.empty())
    {
        return {};
    }

    // Pick one emitter at random and weight by that choice, rather than looping over all
    // of them. Cost then stays constant as scenes gain lights.
    const auto index = static_cast<size_t>(rng.nextFloat() * static_cast<float>(emitters.size()));
    const Scene::Emitter &emitter = emitters[std::min(index, emitters.size() - 1)];
    const float selectionPdf = 1.0f / static_cast<float>(emitters.size());

    const Vec3 toCenter = emitter.center - hit.position;
    const float centerDistanceSquared = toCenter.lengthSquared();
    const float radiusSquared = emitter.radius * emitter.radius;

    // A shading point inside the emitter has no well-defined cone to sample.
    if (centerDistanceSquared <= radiusSquared)
    {
        return {};
    }

    // Sample the cone of directions the sphere subtends, rather than picking a point on
    // its surface. Surface sampling has a singularity at the silhouette: the density
    // carries a 1/cos term against the light's own normal, which tends to zero there and
    // produces individual samples of enormous weight -- the bright isolated pixels known
    // as fireflies. The cone has no such term, and it never wastes a sample on the far
    // side of the sphere, which is occluded by the sphere itself.
    const float distanceToCenter = std::sqrt(centerDistanceSquared);
    const float sinThetaMaxSquared = radiusSquared / centerDistanceSquared;
    const float cosThetaMax = std::sqrt(std::max(0.0f, 1.0f - sinThetaMaxSquared));

    const Vec3 axis = toCenter / distanceToCenter;
    const Vec3 wi = Sampling::uniformCone(rng, cosThetaMax, axis);

    const float nDotL = hit.normal.dot(wi);
    if (nDotL <= 0.0f)
    {
        return {};
    }

    // Distance to where the sampled direction meets the sphere, so the shadow ray can be
    // bounded just short of the light itself.
    const float b = wi.dot(toCenter);
    const float discriminant = (b * b) - centerDistanceSquared + radiusSquared;
    if (discriminant <= 0.0f)
    {
        return {};
    }

    const float distance = b - std::sqrt(discriminant);
    if (distance <= 0.0f)
    {
        return {};
    }

    const Ray shadowRay(Ray::offsetOrigin(hit.position, hit.normal), wi, Ray::defaultEpsilon,
                        distance - (Ray::defaultEpsilon * 4.0f));

    Stats::countRay();
    if (m_Scene->getAccelerationStructure()->isOccluded(shadowRay))
    {
        return {};
    }

    // Uniform density over the sampled cone, times the chance of picking this emitter.
    const float pdfSolidAngle = selectionPdf / (2.0f * Sampling::pi * (1.0f - cosThetaMax));

    if (pdfSolidAngle <= 0.0f)
    {
        return {};
    }

    // Lambertian BRDF is albedo / pi.
    const Vec3 brdf = albedo / Sampling::pi;

    return emitter.emission * brdf * (nDotL / pdfSolidAngle);
}

auto Integrator::radiance(Ray ray, Rng &rng) const -> Vec3
{
    Vec3 radiance{};
    Vec3 throughput{1.0f, 1.0f, 1.0f};

    // Emission is added directly only when it arrives along a path that could not have
    // been accounted for by direct light sampling: the camera ray, and rays leaving a
    // specular surface. Adding it after a diffuse bounce as well would count the same
    // light twice, since sampleDirectLighting already gathered it.
    bool countEmission = true;

    for (unsigned int depth = 0; depth <= m_MaxDepth; depth++)
    {
        Stats::countRay();
        const Hit hit = m_Scene->getAccelerationStructure()->intersect(ray);

        if (!hit.isHit)
        {
            radiance += throughput * background(ray.dir);
            break;
        }

        const Material &material = m_Scene->getMaterialByIndex(hit.materialIndex);
        const Vec3 albedo = material.albedoAt(hit.textureCoord);

        if (material.isEmissive())
        {
            // Emission counts unless direct light sampling already accounted for it.
            // That is only true for surfaces registered as sampled emitters: emissive
            // geometry of other shapes is never picked by sampleDirectLighting, so
            // suppressing it here would lose its contribution entirely.
            const bool alreadySampled = !countEmission && hit.emitterIndex >= 0;

            if (!alreadySampled)
            {
                radiance += throughput * material.emissive;
            }
        }

        radiance += throughput * sampleDirectLighting(hit, material, albedo, rng);

        // Choose the next direction, and update throughput by the scattering weight,
        // which is the BRDF times the cosine divided by the density it was sampled from.
        Vec3 nextDirection;
        Vec3 nextOrigin;

        switch (material.type)
        {
        case Material::Type::Diffuse: {
            Vec3 t;
            Vec3 b;
            Sampling::buildBasis(hit.normal, t, b);

            float pdf = 0.0f;
            const Vec3 local = Sampling::cosineHemisphere(rng, pdf);

            if (pdf <= 0.0f)
            {
                return radiance;
            }

            nextDirection = Sampling::toWorld(local, t, b, hit.normal);

            // Cosine-weighted sampling makes the cosine and the density cancel, leaving
            // just the albedo. That cancellation is the reason for sampling this way.
            throughput *= albedo;

            countEmission = false;
            nextOrigin = Ray::offsetOrigin(hit.position, hit.normal);
            break;
        }

        case Material::Type::Metal: {
            const Vec3 reflected = Sampling::reflect(ray.dir, hit.normal);

            // Roughness perturbs the mirror direction. Squaring it gives a control that
            // feels more even across its range.
            Vec3 scattered = reflected;
            if (material.roughness > 0.0f)
            {
                const float spread = material.roughness * material.roughness;
                scattered = (reflected + (Sampling::uniformSphere(rng) * spread)).normalized();
            }

            // A perturbation can push the direction below the surface; absorb there
            // rather than letting the path continue through the geometry.
            if (scattered.dot(hit.normal) <= 0.0f)
            {
                return radiance;
            }

            nextDirection = scattered;
            throughput *= albedo;

            countEmission = true;
            nextOrigin = Ray::offsetOrigin(hit.position, hit.normal);
            break;
        }

        case Material::Type::Dielectric: {
            // hit.normal always faces the ray, so the sign of the dot product with the
            // geometric orientation tells us which side we are entering.
            const bool entering = ray.dir.dot(hit.normal) < 0.0f;
            const float eta = entering ? (1.0f / material.ior) : material.ior;

            const float cosTheta = std::min(-ray.dir.dot(hit.normal), 1.0f);

            const float r0raw = (1.0f - material.ior) / (1.0f + material.ior);
            const float reflectance = Sampling::fresnelSchlick(cosTheta, r0raw * r0raw);

            Vec3 refracted;
            const bool canRefract = Sampling::refract(ray.dir, hit.normal, eta, refracted);

            // Choose reflection or refraction in proportion to Fresnel. Picking one
            // stochastically keeps the path count from doubling at every glass surface.
            if (!canRefract || rng.nextFloat() < reflectance)
            {
                nextDirection = Sampling::reflect(ray.dir, hit.normal);
                nextOrigin = Ray::offsetOrigin(hit.position, hit.normal);
            }
            else
            {
                nextDirection = refracted;
                nextOrigin = Ray::offsetOrigin(hit.position, -hit.normal);
            }

            throughput *= albedo;

            countEmission = true;
            break;
        }
        }

        // Russian roulette. Terminating low-throughput paths at random, and scaling the
        // survivors up by the survival probability, removes the cost of paths that
        // contribute almost nothing while leaving the estimator unbiased.
        if (depth >= russianRouletteStart)
        {
            const float survival =
                std::min(std::max({throughput.x, throughput.y, throughput.z}), maxSurvivalProbability);

            if (survival <= 0.0f || rng.nextFloat() > survival)
            {
                break;
            }

            throughput /= survival;
        }

        ray = Ray(nextOrigin, nextDirection);
    }

    return radiance;
}
