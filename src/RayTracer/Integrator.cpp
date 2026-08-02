#include "Integrator.h"

#include "Utils/Microfacet.h"
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

/**
 * \brief Combines two sampling strategies by the balance heuristic.
 *
 * Given the densities the two strategies assign to the same direction, this is the share
 * of the estimate that should come from the first. It is the weighting that minimises
 * variance among all combinations that keep the estimator unbiased, and it has an
 * intuitive reading: whichever strategy was more likely to have produced this particular
 * direction is trusted more for it.
 */
auto misWeight(float pdfA, float pdfB) -> float
{
    const float total = pdfA + pdfB;
    return (total > 0.0f) ? (pdfA / total) : 0.0f;
}

} // namespace

auto Integrator::lightPdf(const Vec3 &from, int emitterIndex) const -> float
{
    const std::vector<Scene::Emitter> &emitters = m_Scene->getEmitters();

    if (emitterIndex < 0 || static_cast<size_t>(emitterIndex) >= emitters.size())
    {
        return 0.0f;
    }

    const Scene::Emitter &emitter = emitters[static_cast<size_t>(emitterIndex)];

    const Vec3 toCenter = emitter.center - from;
    const float centerDistanceSquared = toCenter.lengthSquared();
    const float radiusSquared = emitter.radius * emitter.radius;

    // Inside the emitter there is no cone to sample, so light sampling could not have
    // produced this direction at all.
    if (centerDistanceSquared <= radiusSquared)
    {
        return 0.0f;
    }

    const float cosThetaMax = std::sqrt(std::max(0.0f, 1.0f - (radiusSquared / centerDistanceSquared)));
    const float selectionPdf = 1.0f / static_cast<float>(emitters.size());

    return selectionPdf / (2.0f * Sampling::pi * (1.0f - cosThetaMax));
}

auto Integrator::background(const Vec3 &direction) const -> Vec3
{
    (void)direction;

    // A uniform environment. The old renderer's flat ambient term is reinterpreted as
    // light arriving from every direction, which is what it was always pretending to be,
    // except that now it is occluded by geometry and bounces like any other light.
    return m_Scene->getAmbientLighting();
}

auto Integrator::sampleDirectLighting(const Surface &surface, const Material &material, const Vec3 &albedo,
                                      const Vec3 &viewDir, Rng &rng) const -> Vec3
{
    // A mirror, or a smooth conductor, reflects exactly one direction. A point chosen on
    // a light almost never lies along it, so the contribution would be zero with
    // probability one and there is nothing to gain from trying.
    const bool isGlossy = material.type == Material::Type::Metal && material.roughness >= Microfacet::smoothThreshold;

    if (material.type != Material::Type::Diffuse && !isGlossy)
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

    const Vec3 toCenter = emitter.center - surface.position;
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

    const float nDotL = surface.normal.dot(wi);
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

    const Ray shadowRay(Ray::offsetOrigin(surface.position, surface.normal), wi, Ray::defaultEpsilon,
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

    Vec3 brdf;
    float bsdfPdf = 0.0f;

    if (isGlossy)
    {
        const float alpha = Microfacet::roughnessToAlpha(material.roughness);

        Vec3 t;
        Vec3 b;
        Sampling::buildBasis(surface.normal, t, b);

        const Vec3 woLocal = Vec3(viewDir.dot(t), viewDir.dot(b), viewDir.dot(surface.normal));
        const Vec3 wiLocal = Vec3(wi.dot(t), wi.dot(b), wi.dot(surface.normal));

        if (woLocal.z <= 0.0f)
        {
            return {};
        }

        const Vec3 h = (woLocal + wiLocal).normalized();
        const float cosThetaD = std::max(0.0f, woLocal.dot(h));

        const Vec3 fresnel(Sampling::fresnelSchlick(cosThetaD, albedo.x), Sampling::fresnelSchlick(cosThetaD, albedo.y),
                           Sampling::fresnelSchlick(cosThetaD, albedo.z));

        brdf = fresnel * Microfacet::evaluate(woLocal, wiLocal, alpha);
        bsdfPdf = Microfacet::pdf(woLocal, wiLocal, alpha);
    }
    else
    {
        // Lambertian BRDF is albedo / pi.
        brdf = albedo / Sampling::pi;
        bsdfPdf = nDotL / Sampling::pi;
    }

    // Weight this estimate against what scattering would have done. Light sampling is
    // excellent for a small distant source and poor for one that fills the sky; BSDF
    // sampling is the reverse, and for a glossy surface it is far better still. Weighting
    // by the balance heuristic takes the better of the two everywhere without having to
    // decide in advance which case a scene is in.
    const float weight = misWeight(pdfSolidAngle, bsdfPdf);

    return emitter.emission * brdf * (nDotL / pdfSolidAngle) * weight;
}

auto Integrator::radiance(Ray ray, Rng &rng) const -> Vec3
{
    Vec3 albedo;
    Vec3 normal;
    return radiance(ray, rng, albedo, normal);
}

auto Integrator::radiance(Ray ray, Rng &rng, Vec3 &outAlbedo, Vec3 &outNormal) const -> Vec3
{
    // Defaults describe a ray that leaves without hitting anything.
    outAlbedo = background(ray.dir);
    outNormal = Vec3();
    bool recordedFirstHit = false;

    Vec3 radiance{};
    Vec3 throughput{1.0f, 1.0f, 1.0f};

    // Emission is added directly only when it arrives along a path that could not have
    // been accounted for by direct light sampling: the camera ray, and rays leaving a
    // specular surface. Adding it after a diffuse bounce as well would count the same
    // light twice, since sampleDirectLighting already gathered it.
    bool countEmission = true;

    // The multiple importance sampling weight for emission reached by scattering depends
    // on where the scattering happened and how likely that direction was, so both have to
    // survive to the next iteration.
    Vec3 previousPosition{};
    float previousBsdfPdf = 0.0f;

    // The material the path is currently travelling inside, if any. Absorption is applied
    // over the distance between two hits rather than at a surface, so this has to survive
    // from one iteration to the next.
    const Material *interior = nullptr;

    for (unsigned int depth = 0; depth <= m_MaxDepth; depth++)
    {
        Stats::countRay();
        const Hit hit = m_Scene->getAccelerationStructure()->intersect(ray);

        if (!hit.isHit())
        {
            radiance += throughput * background(ray.dir);
            break;
        }

        // Attenuate over the distance just travelled, if that was through a material.
        // Beer's law: what survives falls off exponentially with distance, so doubling
        // the thickness squares the transmitted fraction rather than halving it.
        if (interior != nullptr)
        {
            const Vec3 &k = interior->absorption;

            if (k.x > 0.0f || k.y > 0.0f || k.z > 0.0f)
            {
                throughput *= Vec3(std::exp(-k.x * hit.time), std::exp(-k.y * hit.time), std::exp(-k.z * hit.time));
            }
        }

        // Position, normal and texture coordinate are worked out here, once, from the
        // primitive that won. The intersection tests that lost never computed them.
        const Surface surface = m_Scene->getGeometry().surfaceAt(ray, hit);

        const Material &material = m_Scene->getMaterialByIndex(surface.materialIndex);
        const Vec3 albedo = material.albedoAt(surface.textureCoord, surface.position);

        if (!recordedFirstHit)
        {
            // An emitter has no meaningful reflectance, so its emission stands in as the
            // colour a denoiser should preserve.
            outAlbedo = material.isEmissive() ? material.emissive : albedo;
            outNormal = surface.normal;
            recordedFirstHit = true;
        }

        if (material.isEmissive())
        {
            // How much of this emission belongs to the path that scattered into it.
            //
            // Previously this contribution was discarded whenever direct light sampling
            // could also have found the surface, which avoided double counting but threw
            // away a perfectly good estimate. Weighting the two instead keeps both, and
            // the weights sum to one so the result stays unbiased.
            float weight = 1.0f;

            if (!countEmission && surface.emitterIndex >= 0)
            {
                const float pdfLight = lightPdf(previousPosition, surface.emitterIndex);
                weight = misWeight(previousBsdfPdf, pdfLight);
            }

            radiance += throughput * material.emissive * weight;
        }

        // The view direction points back along the ray, away from the surface.
        const Vec3 viewDir = -ray.dir;

        radiance += throughput * sampleDirectLighting(surface, material, albedo, viewDir, rng);

        // Choose the next direction, and update throughput by the scattering weight,
        // which is the BRDF times the cosine divided by the density it was sampled from.
        Vec3 nextDirection;
        Vec3 nextOrigin;

        switch (material.type)
        {
        case Material::Type::Diffuse: {
            Vec3 t;
            Vec3 b;
            Sampling::buildBasis(surface.normal, t, b);

            float pdf = 0.0f;
            const Vec3 local = Sampling::cosineHemisphere(rng, pdf);

            if (pdf <= 0.0f)
            {
                return radiance;
            }

            nextDirection = Sampling::toWorld(local, t, b, surface.normal);

            // Cosine-weighted sampling makes the cosine and the density cancel, leaving
            // just the albedo. That cancellation is the reason for sampling this way.
            throughput *= albedo;

            previousBsdfPdf = pdf;
            countEmission = false;
            nextOrigin = Ray::offsetOrigin(surface.position, surface.normal);
            break;
        }

        case Material::Type::Metal: {
            // A mirror reflects one direction exactly, so there is no distribution to
            // sample and no density to weigh against anything.
            if (material.roughness < Microfacet::smoothThreshold)
            {
                const Vec3 reflected = Sampling::reflect(ray.dir, surface.normal);

                if (reflected.dot(surface.normal) <= 0.0f)
                {
                    return radiance;
                }

                nextDirection = reflected;
                throughput *= albedo;

                countEmission = true;
                nextOrigin = Ray::offsetOrigin(surface.position, surface.normal);
                break;
            }

            const float alpha = Microfacet::roughnessToAlpha(material.roughness);

            Vec3 t;
            Vec3 b;
            Sampling::buildBasis(surface.normal, t, b);

            // Work in the local frame, where the surface normal is +Z and the
            // distribution is expressed most simply.
            const Vec3 woLocal = Vec3(-ray.dir.dot(t), -ray.dir.dot(b), -ray.dir.dot(surface.normal));

            if (woLocal.z <= 0.0f)
            {
                return radiance;
            }

            const Vec3 h = Microfacet::sampleVisibleNormal(woLocal, alpha, rng.nextFloat(), rng.nextFloat());
            const Vec3 wiLocal = Sampling::reflect(-woLocal, h);

            // A facet can reflect the view below the surface, where the path cannot
            // continue. Those directions are absorbed, which is the physical outcome of
            // the microfacet being shadowed by its neighbours.
            if (wiLocal.z <= 0.0f)
            {
                return radiance;
            }

            const float weight = Microfacet::visibleNormalWeight(woLocal, wiLocal, alpha);
            if (weight <= 0.0f)
            {
                return radiance;
            }

            // Fresnel with the surface tint as the reflectance at normal incidence, which
            // is the usual way to describe a conductor.
            const float cosThetaD = std::max(0.0f, woLocal.dot(h));
            const Vec3 fresnel(Sampling::fresnelSchlick(cosThetaD, albedo.x),
                               Sampling::fresnelSchlick(cosThetaD, albedo.y),
                               Sampling::fresnelSchlick(cosThetaD, albedo.z));

            // Put back the light the single bounce dropped.
            //
            // Without this a rough conductor is simply too dark, and increasingly so with
            // roughness, because energy that should have bounced between facets and
            // eventually left the surface was absorbed instead. Scaling by how much the
            // lobe actually reflects restores it, tinted by the surface, since light that
            // bounces more than once is coloured more than once.
            const float singleScatterAlbedo = Microfacet::directionalAlbedo(woLocal.z, material.roughness);
            const Vec3 compensation = Vec3(1.0f, 1.0f, 1.0f) + (albedo * ((1.0f / singleScatterAlbedo) - 1.0f));

            throughput *= fresnel * weight * compensation;

            nextDirection = Sampling::toWorld(wiLocal, t, b, surface.normal);
            previousBsdfPdf = Microfacet::pdf(woLocal, wiLocal, alpha);

            // A rough surface has a real density, so emission it scatters into can be
            // weighed against direct light sampling rather than being taken whole.
            countEmission = false;
            nextOrigin = Ray::offsetOrigin(surface.position, surface.normal);
            break;
        }

        case Material::Type::Dielectric: {
            // Which side of the surface the ray struck decides the ratio of indices.
            //
            // This previously asked whether the ray pointed against the normal, which is
            // always true: the normal is flipped to face the ray before shading. Every
            // interaction was therefore treated as entering the glass, so rays inside it
            // bent the wrong way trying to leave, never escaped, and a glass sphere
            // rendered as a solid ball.
            const float eta = surface.frontFace ? (1.0f / material.ior) : material.ior;

            const float cosIncident = std::min(-ray.dir.dot(surface.normal), 1.0f);

            Vec3 refracted;
            const bool canRefract = Sampling::refract(ray.dir, surface.normal, eta, refracted);

            // Schlick's approximation is stated from the less dense side, so a ray on its
            // way out of the glass has to be evaluated with the angle it will leave at
            // rather than the shallower one it arrived with. Using the wrong one
            // understates reflection at grazing angles, which is where a glass surface
            // becomes most mirror-like.
            const float cosForFresnel =
                (canRefract && !surface.frontFace) ? std::fabs(refracted.dot(surface.normal)) : cosIncident;

            const float r0raw = (1.0f - material.ior) / (1.0f + material.ior);
            const float reflectance = Sampling::fresnelSchlick(cosForFresnel, r0raw * r0raw);

            // Choose reflection or refraction in proportion to Fresnel. Picking one
            // stochastically keeps the path count from doubling at every glass surface.
            // Total internal reflection leaves no choice to make.
            if (!canRefract || rng.nextFloat() < reflectance)
            {
                nextDirection = Sampling::reflect(ray.dir, surface.normal);
                nextOrigin = Ray::offsetOrigin(surface.position, surface.normal);
            }
            else
            {
                nextDirection = refracted;
                nextOrigin = Ray::offsetOrigin(surface.position, -surface.normal);

                // Refracting is the only way to cross the boundary, so this is where the
                // path enters or leaves the interior.
                interior = surface.frontFace ? &material : nullptr;
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

        previousPosition = surface.position;

        ray = Ray(nextOrigin, nextDirection);
    }

    return radiance;
}
