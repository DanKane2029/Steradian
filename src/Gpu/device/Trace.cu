// The device side of ray traversal.
//
// Compiled at run time by NVRTC and executed by OptiX, so the ray/box and ray/triangle
// work lands on the RT cores. That is the whole point of the exercise: the CPU renderer
// spends 658 bounding box visits per ray on the dragon against 87.5 triangle tests, and
// box traversal is precisely what that hardware does in silicon.
//
// The programs here are deliberately as small as they can be. They report which primitive
// was hit, how far along, and where on it -- nothing else. Shading reads those four
// numbers later, exactly as the CPU integrator does.

#include <optix.h>

#include "Gpu/DeviceTypes.h"
#include "Scene/Primitives.h"
#include "Utils/Vec3.h"

extern "C" __constant__ Gpu::LaunchParams params;

namespace
{

/**
 * \brief Carries a hit back from a trace through the payload registers.
 *
 * Four 32-bit registers, which is small enough to stay in registers the whole way rather
 * than spilling. A distance and two barycentrics are floats reinterpreted as integers;
 * OptiX payloads are untyped words and this is the usual way to put a float in one.
 */
__device__ __forceinline__ auto asInt(float value) -> unsigned int
{
    return __float_as_uint(value);
}

__device__ __forceinline__ auto asFloat(unsigned int value) -> float
{
    return __uint_as_float(value);
}

} // namespace

extern "C" __global__ void __raygen__trace()
{
    const unsigned int index = optixGetLaunchIndex().x;

    if (index >= params.rayCount)
    {
        return;
    }

    const Vec3 origin = params.rayOrigins[index];
    const Vec3 direction = params.rayDirections[index];

    // Initialised to a miss, so a ray that reaches no hit program reports one without the
    // miss program having to write anything but the primitive word.
    unsigned int p0 = asInt(params.tMax);
    unsigned int p1 = 0;
    unsigned int p2 = 0;
    unsigned int p3 = Gpu::noPrimitive;

    optixTrace(params.handle, make_float3(origin.x, origin.y, origin.z),
               make_float3(direction.x, direction.y, direction.z), params.tMin, params.tMax,
               0.0f, // ray time, unused
               OptixVisibilityMask(255), OPTIX_RAY_FLAG_NONE,
               0, // SBT offset
               1, // SBT stride: one record per primitive kind
               0, // miss index
               p0, p1, p2, p3);

    Gpu::DeviceHit hit;
    hit.time = asFloat(p0);
    hit.u = asFloat(p1);
    hit.v = asFloat(p2);
    hit.primitive = p3;

    params.hits[index] = hit;
}

extern "C" __global__ void __miss__nothing()
{
    optixSetPayload_3(Gpu::noPrimitive);
}

/**
 * \brief Reports a triangle hit found by the built-in intersection.
 *
 * The barycentrics come from OptiX rather than from Moller-Trumbore, because the
 * intersection was done by the RT cores. They follow the same convention the CPU uses --
 * weights of (1 - u - v, u, v) over the three vertices -- so everything downstream reads
 * them the same way. The numbers are not identical to the CPU's, and cannot be: this is a
 * different intersection algorithm running on different hardware.
 */
extern "C" __global__ void __closesthit__triangle()
{
    const float2 barycentrics = optixGetTriangleBarycentrics();

    optixSetPayload_0(__float_as_uint(optixGetRayTmax()));
    optixSetPayload_1(__float_as_uint(barycentrics.x));
    optixSetPayload_2(__float_as_uint(barycentrics.y));
    optixSetPayload_3(optixGetPrimitiveIndex());
}

/**
 * \brief Intersects a sphere.
 *
 * Written out rather than using OptiX's built-in sphere primitive, so that both backends
 * run the same test. The built-in one would be a second implementation of something the
 * CPU already does, and the two would agree until one was changed.
 *
 * Mirrors Geometry::intersectSphere, including its refusal to reject on tca < 0: a ray
 * whose origin is inside the sphere still has a valid forward intersection, and
 * refraction depends on finding it.
 */
extern "C" __global__ void __intersection__sphere()
{
    const unsigned int sphereIndex = optixGetPrimitiveIndex();
    const Sphere sphere = params.geometry.spheres[sphereIndex];

    const float3 rawOrigin = optixGetObjectRayOrigin();
    const float3 rawDirection = optixGetObjectRayDirection();

    const Vec3 origin(rawOrigin.x, rawOrigin.y, rawOrigin.z);
    const Vec3 direction(rawDirection.x, rawDirection.y, rawDirection.z);

    const Vec3 l = sphere.center - origin;
    const float tca = l.dot(direction);

    const float d2 = l.dot(l) - (tca * tca);
    const float radius2 = sphere.radius * sphere.radius;

    if (d2 > radius2)
    {
        return;
    }

    const float thc = sqrtf(radius2 - d2);

    const float tNear = optixGetRayTmin();
    const float tFar = optixGetRayTmax();

    // The near root first; if it is behind the interval, the far one. Reporting outside
    // the interval is not an error, OptiX simply ignores it, but checking here saves the
    // call.
    float t = tca - thc;
    if (t < tNear || t > tFar)
    {
        t = tca + thc;
        if (t < tNear || t > tFar)
        {
            return;
        }
    }

    optixReportIntersection(t, 0);
}

extern "C" __global__ void __closesthit__sphere()
{
    optixSetPayload_0(__float_as_uint(optixGetRayTmax()));

    // A sphere has no barycentrics. The surface point follows from the distance alone,
    // which is what the CPU does with them too.
    optixSetPayload_1(0);
    optixSetPayload_2(0);

    // Spheres occupy the index space above the triangles, matching Geometry.
    optixSetPayload_3(params.geometry.triangleCount + optixGetPrimitiveIndex());
}

// ---------------------------------------------------------------------------------------
// The path tracer.
//
// A direct port of Integrator::radiance. It is written to follow that function step for
// step rather than to be clever, because the two have to agree, and a reader checking
// whether they do should be able to read them side by side. Where the CPU calls into
// Sampling or Microfacet, so does this: those headers are shared, not reimplemented.
// ---------------------------------------------------------------------------------------

#include "Scene/Material.h"
#include "Utils/Microfacet.h"
#include "Utils/Random.h"
#include "Utils/Sampling.h"

namespace
{

/** depth after which paths start being terminated randomly */
constexpr unsigned int russianRouletteStart = 3;

/** largest survival probability used by Russian roulette */
constexpr float maxSurvivalProbability = 0.95f;

/** \brief The share of an estimate that should come from the first of two strategies. */
__device__ __forceinline__ auto misWeight(float pdfA, float pdfB) -> float
{
    const float total = pdfA + pdfB;
    return (total > 0.0f) ? (pdfA / total) : 0.0f;
}

/** \brief Traces for the closest hit. */
__device__ __forceinline__ auto traceClosest(const Vec3 &origin, const Vec3 &direction, float tMin,
                                             float tMax) -> Gpu::DeviceHit
{
    unsigned int p0 = __float_as_uint(tMax);
    unsigned int p1 = 0;
    unsigned int p2 = 0;
    unsigned int p3 = Gpu::noPrimitive;

    optixTrace(params.handle, make_float3(origin.x, origin.y, origin.z),
               make_float3(direction.x, direction.y, direction.z), tMin, tMax, 0.0f, OptixVisibilityMask(255),
               OPTIX_RAY_FLAG_NONE, 0, 1, 0, p0, p1, p2, p3);

    Gpu::DeviceHit hit;
    hit.time = __uint_as_float(p0);
    hit.u = __uint_as_float(p1);
    hit.v = __uint_as_float(p2);
    hit.primitive = p3;

    return hit;
}

/**
 * \brief Reports whether anything blocks the interval.
 *
 * Stops at the first thing it meets rather than looking for the closest, which is all a
 * shadow ray needs to know. The same hit programs serve, since terminating on first hit
 * makes whichever one runs the answer.
 */
__device__ __forceinline__ auto traceOccluded(const Vec3 &origin, const Vec3 &direction, float tMin, float tMax) -> bool
{
    unsigned int p0 = 0;
    unsigned int p1 = 0;
    unsigned int p2 = 0;
    unsigned int p3 = Gpu::noPrimitive;

    optixTrace(params.handle, make_float3(origin.x, origin.y, origin.z),
               make_float3(direction.x, direction.y, direction.z), tMin, tMax, 0.0f, OptixVisibilityMask(255),
               OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT, 0, 1, 0, p0, p1, p2, p3);

    return p3 != Gpu::noPrimitive;
}

/** \brief Samples a texture, mirroring Texture::getTexel. */
__device__ auto sampleTexture(int textureIndex, float u, float v) -> Vec3
{
    const Gpu::DeviceTexture &texture = params.textures[textureIndex];

    if (texture.width <= 0 || texture.height <= 0 || texture.channels < 1)
    {
        return Vec3();
    }

    // Wrap into [0, 1). fmodf keeps the sign of its input, so negative coordinates need a
    // further shift before being scaled to pixels.
    u = fmodf(u, 1.0f);
    v = fmodf(v, 1.0f);
    if (u < 0.0f)
    {
        u += 1.0f;
    }
    if (v < 0.0f)
    {
        v += 1.0f;
    }

    int px = static_cast<int>(u * static_cast<float>(texture.width));
    int py = static_cast<int>(v * static_cast<float>(texture.height));

    px = Math::clamp(px, 0, texture.width - 1);
    py = Math::clamp(py, 0, texture.height - 1);

    const unsigned int index = texture.offset + (((py * texture.width) + px) * texture.channels);

    if (texture.channels < 3)
    {
        const float grey = params.texturePixels[index];
        return Vec3(grey, grey, grey);
    }

    return Vec3(params.texturePixels[index], params.texturePixels[index + 1], params.texturePixels[index + 2]);
}

/** \brief The surface colour of a material at a point, mirroring Scene::albedoAt. */
__device__ __forceinline__ auto albedoAt(const Material &material, const Vec3 &texCoord, const Vec3 &position) -> Vec3
{
    const Vec3 base = material.baseAlbedo(position);

    if (material.textureIndex < 0)
    {
        return base;
    }

    return base * sampleTexture(material.textureIndex, texCoord.x, texCoord.y);
}

/** \brief Recovers the shading attributes at a hit, mirroring Geometry::surfaceAt. */
__device__ auto surfaceAt(const Vec3 &origin, const Vec3 &direction, const Gpu::DeviceHit &hit) -> Surface
{
    const Gpu::DeviceGeometry &geometry = params.geometry;

    Surface surface;
    surface.position = origin + (direction * hit.time);

    if (hit.primitive < geometry.triangleCount)
    {
        const Triangle &triangle = geometry.triangles[hit.primitive];

        surface.materialIndex = triangle.materialIndex;

        const float w = 1.0f - hit.u - hit.v;

        const Vec3 interpolated = (geometry.normals[triangle.vertex0] * w) +
                                  (geometry.normals[triangle.vertex1] * hit.u) +
                                  (geometry.normals[triangle.vertex2] * hit.v);

        if (interpolated.lengthSquared() <= 0.0f)
        {
            const Vec3 &point0 = geometry.positions[triangle.vertex0];
            surface.normal = (geometry.positions[triangle.vertex1] - point0)
                                 .cross(geometry.positions[triangle.vertex2] - point0)
                                 .normalized();
        }
        else
        {
            surface.normal = interpolated.normalized();
        }

        surface.textureCoord = (geometry.texCoords[triangle.vertex0] * w) +
                               (geometry.texCoords[triangle.vertex1] * hit.u) +
                               (geometry.texCoords[triangle.vertex2] * hit.v);
    }
    else
    {
        const Sphere &sphere = geometry.spheres[hit.primitive - geometry.triangleCount];

        surface.materialIndex = sphere.materialIndex;
        surface.emitterIndex = sphere.emitterIndex;
        surface.normal = (surface.position - sphere.center).normalized();

        const Vec3 &n = surface.normal;
        surface.textureCoord = Vec3((atan2f(n.x, n.z) / (2.0f * Sampling::pi)) + 0.5f, (n.y * 0.5f) + 0.5f, 0.0f);
    }

    surface.frontFace = surface.normal.dot(direction) < 0.0f;

    if (!surface.frontFace)
    {
        surface.normal = -surface.normal;
    }

    return surface;
}

/** \brief Density that direct light sampling would have given a direction. */
__device__ auto lightPdf(const Vec3 &from, int emitterIndex) -> float
{
    if (emitterIndex < 0 || static_cast<unsigned int>(emitterIndex) >= params.emitterCount)
    {
        return 0.0f;
    }

    const Emitter &emitter = params.emitters[emitterIndex];

    const Vec3 toCenter = emitter.center - from;
    const float centerDistanceSquared = toCenter.lengthSquared();
    const float radiusSquared = emitter.radius * emitter.radius;

    if (centerDistanceSquared <= radiusSquared)
    {
        return 0.0f;
    }

    const float cosThetaMax = sqrtf(Math::max(0.0f, 1.0f - (radiusSquared / centerDistanceSquared)));
    const float selectionPdf = 1.0f / static_cast<float>(params.emitterCount);

    return selectionPdf / (2.0f * Sampling::pi * (1.0f - cosThetaMax));
}

/** \brief Estimates direct lighting at a surface point by sampling the emitters. */
__device__ auto sampleDirectLighting(const Surface &surface, const Material &material, const Vec3 &albedo,
                                     const Vec3 &viewDir, Rng &rng) -> Vec3
{
    const bool isGlossy = material.type == Material::Type::Metal && material.roughness >= Microfacet::smoothThreshold;

    if (material.type != Material::Type::Diffuse && !isGlossy)
    {
        return Vec3();
    }

    if (params.emitterCount == 0)
    {
        return Vec3();
    }

    const unsigned int index = static_cast<unsigned int>(rng.nextFloat() * static_cast<float>(params.emitterCount));
    const Emitter &emitter =
        params.emitters[Math::min(static_cast<int>(index), static_cast<int>(params.emitterCount) - 1)];
    const float selectionPdf = 1.0f / static_cast<float>(params.emitterCount);

    const Vec3 toCenter = emitter.center - surface.position;
    const float centerDistanceSquared = toCenter.lengthSquared();
    const float radiusSquared = emitter.radius * emitter.radius;

    if (centerDistanceSquared <= radiusSquared)
    {
        return Vec3();
    }

    const float distanceToCenter = sqrtf(centerDistanceSquared);
    const float sinThetaMaxSquared = radiusSquared / centerDistanceSquared;
    const float cosThetaMax = sqrtf(Math::max(0.0f, 1.0f - sinThetaMaxSquared));

    const Vec3 axis = toCenter / distanceToCenter;
    const Vec3 wi = Sampling::uniformCone(rng, cosThetaMax, axis);

    const float nDotL = surface.normal.dot(wi);
    if (nDotL <= 0.0f)
    {
        return Vec3();
    }

    const float b = wi.dot(toCenter);
    const float discriminant = (b * b) - centerDistanceSquared + radiusSquared;
    if (discriminant <= 0.0f)
    {
        return Vec3();
    }

    const float distance = b - sqrtf(discriminant);
    if (distance <= 0.0f)
    {
        return Vec3();
    }

    constexpr float epsilon = 1e-4f;

    if (traceOccluded(surface.position + (surface.normal * epsilon), wi, epsilon, distance - (epsilon * 4.0f)))
    {
        return Vec3();
    }

    const float pdfSolidAngle = selectionPdf / (2.0f * Sampling::pi * (1.0f - cosThetaMax));

    if (pdfSolidAngle <= 0.0f)
    {
        return Vec3();
    }

    Vec3 brdf;
    float bsdfPdf = 0.0f;

    if (isGlossy)
    {
        const float alpha = Microfacet::roughnessToAlpha(material.roughness);

        Vec3 t;
        Vec3 bt;
        Sampling::buildBasis(surface.normal, t, bt);

        const Vec3 woLocal = Vec3(viewDir.dot(t), viewDir.dot(bt), viewDir.dot(surface.normal));
        const Vec3 wiLocal = Vec3(wi.dot(t), wi.dot(bt), wi.dot(surface.normal));

        if (woLocal.z <= 0.0f)
        {
            return Vec3();
        }

        const Vec3 h = (woLocal + wiLocal).normalized();
        const float cosThetaD = Math::max(0.0f, woLocal.dot(h));

        const Vec3 fresnel(Sampling::fresnelSchlick(cosThetaD, albedo.x), Sampling::fresnelSchlick(cosThetaD, albedo.y),
                           Sampling::fresnelSchlick(cosThetaD, albedo.z));

        brdf = fresnel * Microfacet::evaluate(woLocal, wiLocal, alpha);
        bsdfPdf = Microfacet::pdf(woLocal, wiLocal, alpha);
    }
    else
    {
        brdf = albedo / Sampling::pi;
        bsdfPdf = nDotL / Sampling::pi;
    }

    const float weight = misWeight(pdfSolidAngle, bsdfPdf);

    return emitter.emission * brdf * (nDotL / pdfSolidAngle) * weight;
}

/** \brief Estimates the radiance arriving along a ray. */
__device__ auto radiance(Vec3 origin, Vec3 direction, Rng &rng, Vec3 &outAlbedo, Vec3 &outNormal) -> Vec3
{
    constexpr float epsilon = 1e-4f;

    outAlbedo = params.ambient;
    outNormal = Vec3();
    bool recordedFirstHit = false;

    Vec3 radiance;
    Vec3 throughput(1.0f, 1.0f, 1.0f);

    bool countEmission = true;

    Vec3 previousPosition;
    float previousBsdfPdf = 0.0f;

    // Which material the path is currently inside, or -1. An index rather than a pointer
    // because it survives across iterations and a pointer into constant memory would be
    // no cheaper to carry.
    int interior = -1;

    for (unsigned int depth = 0; depth <= params.maxDepth; depth++)
    {
        const Gpu::DeviceHit hit = traceClosest(origin, direction, epsilon, 1e30f);

        if (hit.primitive == Gpu::noPrimitive)
        {
            radiance += throughput * params.ambient;
            break;
        }

        if (interior >= 0)
        {
            const Vec3 &k = params.materials[interior].absorption;

            if (k.x > 0.0f || k.y > 0.0f || k.z > 0.0f)
            {
                throughput *= Vec3(expf(-k.x * hit.time), expf(-k.y * hit.time), expf(-k.z * hit.time));
            }
        }

        const Surface surface = surfaceAt(origin, direction, hit);
        const Material &material = params.materials[surface.materialIndex];
        const Vec3 albedo = albedoAt(material, surface.textureCoord, surface.position);

        if (!recordedFirstHit)
        {
            outAlbedo = material.isEmissive() ? material.emissive : albedo;
            outNormal = surface.normal;
            recordedFirstHit = true;
        }

        if (material.isEmissive())
        {
            float weight = 1.0f;

            if (!countEmission && surface.emitterIndex >= 0)
            {
                weight = misWeight(previousBsdfPdf, lightPdf(previousPosition, surface.emitterIndex));
            }

            radiance += throughput * material.emissive * weight;
        }

        const Vec3 viewDir = -direction;

        radiance += throughput * sampleDirectLighting(surface, material, albedo, viewDir, rng);

        Vec3 nextDirection;
        Vec3 nextOrigin;

        if (material.type == Material::Type::Diffuse)
        {
            Vec3 t;
            Vec3 bt;
            Sampling::buildBasis(surface.normal, t, bt);

            float pdf = 0.0f;
            const Vec3 local = Sampling::cosineHemisphere(rng, pdf);

            if (pdf <= 0.0f)
            {
                return radiance;
            }

            nextDirection = Sampling::toWorld(local, t, bt, surface.normal);
            throughput *= albedo;

            previousBsdfPdf = pdf;
            countEmission = false;
            nextOrigin = surface.position + (surface.normal * epsilon);
        }
        else if (material.type == Material::Type::Metal)
        {
            if (material.roughness < Microfacet::smoothThreshold)
            {
                const Vec3 reflected = Sampling::reflect(direction, surface.normal);

                if (reflected.dot(surface.normal) <= 0.0f)
                {
                    return radiance;
                }

                nextDirection = reflected;
                throughput *= albedo;

                countEmission = true;
                nextOrigin = surface.position + (surface.normal * epsilon);
            }
            else
            {
                const float alpha = Microfacet::roughnessToAlpha(material.roughness);

                Vec3 t;
                Vec3 bt;
                Sampling::buildBasis(surface.normal, t, bt);

                const Vec3 woLocal = Vec3(-direction.dot(t), -direction.dot(bt), -direction.dot(surface.normal));

                if (woLocal.z <= 0.0f)
                {
                    return radiance;
                }

                const Vec3 h = Microfacet::sampleVisibleNormal(woLocal, alpha, rng.nextFloat(), rng.nextFloat());
                const Vec3 wiLocal = Sampling::reflect(-woLocal, h);

                if (wiLocal.z <= 0.0f)
                {
                    return radiance;
                }

                const float weight = Microfacet::visibleNormalWeight(woLocal, wiLocal, alpha);
                if (weight <= 0.0f)
                {
                    return radiance;
                }

                const float cosThetaD = Math::max(0.0f, woLocal.dot(h));
                const Vec3 fresnel(Sampling::fresnelSchlick(cosThetaD, albedo.x),
                                   Sampling::fresnelSchlick(cosThetaD, albedo.y),
                                   Sampling::fresnelSchlick(cosThetaD, albedo.z));

                const float singleScatterAlbedo =
                    Microfacet::directionalAlbedo(params.albedoTable, woLocal.z, material.roughness);
                const Vec3 compensation = Vec3(1.0f, 1.0f, 1.0f) + (albedo * ((1.0f / singleScatterAlbedo) - 1.0f));

                throughput *= fresnel * weight * compensation;

                nextDirection = Sampling::toWorld(wiLocal, t, bt, surface.normal);
                previousBsdfPdf = Microfacet::pdf(woLocal, wiLocal, alpha);

                countEmission = false;
                nextOrigin = surface.position + (surface.normal * epsilon);
            }
        }
        else
        {
            const float eta = surface.frontFace ? (1.0f / material.ior) : material.ior;
            const float cosIncident = Math::min(-direction.dot(surface.normal), 1.0f);

            Vec3 refracted;
            const bool canRefract = Sampling::refract(direction, surface.normal, eta, refracted);

            const float cosForFresnel =
                (canRefract && !surface.frontFace) ? fabsf(refracted.dot(surface.normal)) : cosIncident;

            const float r0raw = (1.0f - material.ior) / (1.0f + material.ior);
            const float reflectance = Sampling::fresnelSchlick(cosForFresnel, r0raw * r0raw);

            if (!canRefract || rng.nextFloat() < reflectance)
            {
                nextDirection = Sampling::reflect(direction, surface.normal);
                nextOrigin = surface.position + (surface.normal * epsilon);
            }
            else
            {
                nextDirection = refracted;
                nextOrigin = surface.position - (surface.normal * epsilon);

                interior = surface.frontFace ? static_cast<int>(surface.materialIndex) : -1;
            }

            throughput *= albedo;
            countEmission = true;
        }

        if (depth >= russianRouletteStart)
        {
            const float survival =
                Math::min(Math::max(throughput.x, Math::max(throughput.y, throughput.z)), maxSurvivalProbability);

            if (survival <= 0.0f || rng.nextFloat() > survival)
            {
                break;
            }

            throughput /= survival;
        }

        previousPosition = surface.position;

        origin = nextOrigin;
        direction = nextDirection.normalized();
    }

    return radiance;
}

} // namespace

/**
 * \brief One thread per pixel, taking every sample for it.
 *
 * A pixel is owned by exactly one thread, and its generator is seeded from the pixel
 * index, so the result does not depend on how the launch was divided up. The CPU seeds
 * per row instead, and consumes that row's stream left to right, which cannot be
 * reproduced by threads running in parallel -- so the two backends draw different numbers
 * and produce different noise. They converge to the same image, which is what the tests
 * check, rather than to the same pixels, which they cannot.
 */
extern "C" __global__ void __raygen__render()
{
    const uint3 launchIndex = optixGetLaunchIndex();

    const int x = static_cast<int>(launchIndex.x);
    const int y = static_cast<int>(launchIndex.y);

    if (x >= params.width || y >= params.height)
    {
        return;
    }

    const unsigned int pixel =
        (static_cast<unsigned int>(y) * static_cast<unsigned int>(params.width)) + static_cast<unsigned int>(x);

    Rng rng(params.seed, pixel);

    // Stratified over a grid, as on the CPU: independent samples clump by chance and
    // leave parts of a pixel uncovered.
    unsigned int strataPerAxis = static_cast<unsigned int>(sqrtf(static_cast<float>(params.samplesPerPixel)));
    if (strataPerAxis == 0)
    {
        strataPerAxis = 1;
    }
    const unsigned int stratifiedSamples = strataPerAxis * strataPerAxis;

    Vec3 total;
    Vec3 albedoTotal;
    Vec3 normalTotal;

    for (unsigned int s = 0; s < params.samplesPerPixel; s++)
    {
        float jitterX = 0.0f;
        float jitterY = 0.0f;

        if (s < stratifiedSamples)
        {
            const unsigned int sx = s % strataPerAxis;
            const unsigned int sy = s / strataPerAxis;

            jitterX = (static_cast<float>(sx) + rng.nextFloat()) / static_cast<float>(strataPerAxis);
            jitterY = (static_cast<float>(sy) + rng.nextFloat()) / static_cast<float>(strataPerAxis);
        }
        else
        {
            jitterX = rng.nextFloat();
            jitterY = rng.nextFloat();
        }

        const float u = (static_cast<float>(x) + jitterX) / static_cast<float>(params.width);
        const float v = (static_cast<float>(y) + jitterY) / static_cast<float>(params.height);

        const float screenX = ((u * 2.0f) - 1.0f) * params.camera.halfWidth;
        const float screenY = ((v * 2.0f) - 1.0f) * params.camera.halfHeight;

        const Vec3 direction =
            (params.camera.direction + (params.camera.right * screenX) + (params.camera.up * screenY)).normalized();

        Vec3 albedo;
        Vec3 normal;
        total += radiance(params.camera.origin, direction, rng, albedo, normal);

        albedoTotal += albedo;
        normalTotal += normal;
    }

    const float inverse = 1.0f / static_cast<float>(params.samplesPerPixel);

    params.film[pixel] = total * inverse;
    params.filmAlbedo[pixel] = albedoTotal * inverse;
    params.filmNormal[pixel] = normalTotal * inverse;
}
