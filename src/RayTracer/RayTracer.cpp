#include "RayTracer.h"

#include "Utils/Stats.h"

#include <cmath>
#include <cstdint>

RayTracer::RayTracer(PixelBuffer *pixelBuffer, Scene *scene, Config &config)
    : m_Scene(scene), m_PixelBuffer(pixelBuffer)
{
    auto size = m_PixelBuffer->getSize();

    m_aspectRatio = (float)size.first / (float)size.second;
    m_NumShadowRays = config.numShadowRays;
    m_MaxRecurseLevel = config.maxRecurseLevel;
}

RayTracer::~RayTracer() = default;

void RayTracer::updateAspectRatio(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
}

auto RayTracer::makeCameraRay(float u, float v) -> Ray
{
    const Camera &camera = m_Scene->getCamera();

    // Half-extents of the film plane at unit distance. The vertical half-height follows
    // from the field of view; the horizontal one is that scaled by the aspect ratio.
    //
    // Previously the field of view was derived from the pixel count rather than from the
    // scene (which pinned it at roughly 90 degrees regardless of configuration) and the
    // aspect ratio was then applied on top of a term already derived from the width, so
    // it was counted twice and the image came out horizontally stretched.
    const float halfHeight = std::tan(camera.fovY * 0.5f);
    const float halfWidth = halfHeight * m_aspectRatio;

    // Map [0, 1] film coordinates to [-1, 1] screen coordinates.
    const float screenX = ((u * 2.0f) - 1.0f) * halfWidth;
    const float screenY = ((v * 2.0f) - 1.0f) * halfHeight;

    // Offset along the camera's own basis rather than along world X and Y, so the camera
    // can be oriented in any direction.
    const Vec3 dir = camera.dir + (camera.right * screenX) + (camera.up * screenY);

    return {camera.org, dir};
}

auto RayTracer::samplePixel(int ix, int iy, float jitterX, float jitterY, Rng &rng) -> Vec3
{
    auto size = m_PixelBuffer->getSize();

    const float u = (static_cast<float>(ix) + jitterX) / static_cast<float>(size.first);
    const float v = (static_cast<float>(iy) + jitterY) / static_cast<float>(size.second);

    Ray ray = makeCameraRay(u, v);
    Hit hit = shootRay(ray);

    if (!hit.isHit)
    {
        return {};
    }

    return getHitColor(hit, 0, rng);
}

void RayTracer::sampleScene(float x, float y)
{
    auto size = m_PixelBuffer->getSize();

    const int ix = static_cast<int>(floorf(static_cast<float>(size.first - 1) * x));
    const int iy = static_cast<int>(floorf(static_cast<float>(size.second - 1) * y));

    // The interactive viewer samples random points from several threads at once, so
    // each thread needs its own generator rather than a shared one.
    static thread_local Rng rng(0x9e3779b97f4a7c15ULL, reinterpret_cast<uintptr_t>(&rng));

    const Vec3 color = samplePixel(ix, iy, 0.5f, 0.5f, rng);

    m_PixelBufferGuard.lock();
    m_PixelBuffer->setPixel(ix, iy, color);
    m_PixelBufferGuard.unlock();
}

void RayTracer::renderRows(int yStart, int yEnd, unsigned int samplesPerPixel, uint64_t seed)
{
    auto size = m_PixelBuffer->getSize();
    const int width = size.first;

    // Counters are thread-local while rendering and merged once at the end, so the hot
    // path stays lock-free.
    Stats::resetThread();

    for (int iy = yStart; iy < yEnd; iy++)
    {
        // Seeding per row rather than per thread is what makes the output independent
        // of how the image was divided up between workers.
        Rng rng(seed, static_cast<uint64_t>(iy));

        for (int ix = 0; ix < width; ix++)
        {
            for (unsigned int s = 0; s < samplesPerPixel; s++)
            {
                const float jitterX = (samplesPerPixel == 1) ? 0.5f : rng.nextFloat();
                const float jitterY = (samplesPerPixel == 1) ? 0.5f : rng.nextFloat();

                m_PixelBuffer->setPixel(ix, iy, samplePixel(ix, iy, jitterX, jitterY, rng));
            }
        }
    }

    Stats::mergeThread();
}

auto RayTracer::shootRay(Ray ray) -> Hit
{
    Stats::countRay();

    Hit hit = m_Scene->getAccelerationStructure()->root->rayIntersect(ray);
    hit.ray = ray;
    return hit;
}

auto RayTracer::getHitColor(Hit hit, unsigned int recurseLevel, Rng &rng) -> Vec3
{
    if (recurseLevel > m_MaxRecurseLevel)
    {
        return Vec3{};
    }

    Material mat = *m_Scene->getMaterial(hit.materialName);

    Vec3 finalColor = 0;

    finalColor += m_Scene->getAmbientLighting() + mat.ambient;

    const Vec3 viewDir = (m_Scene->getCamera().org - hit.position).normalized();

    for (const std::shared_ptr<Light> &light : m_Scene->getLightList())
    {
        const Vec3 toLight = light->getPos() - hit.position;
        const float lightDist = toLight.length();
        const Vec3 lightDir = toLight.normalized();

        const float nDotL = lightDir.dot(hit.normal);

        // Nothing to add for a surface facing away from the light, and evaluating the
        // specular term there produced highlights on geometry the light cannot reach.
        if (nDotL <= 0.0f)
        {
            continue;
        }

        // Inverse-square falloff. This previously read intensity^2 / distance, which is
        // neither physical nor what the scene files were authored against.
        const float falloff = light->getIntensity() / std::max(lightDist * lightDist, 1e-6f);
        const Vec3 radiance = light->getColor() * falloff;

        const Vec3 diffuse = radiance * nDotL * mat.diffuse;

        // Specular component (Blinn-Phong). The base is clamped to zero: powf with a
        // negative base and a fractional exponent returns NaN, which then propagated
        // through the running pixel average and poisoned everything it touched.
        const Vec3 halfWay = (lightDir + viewDir).normalized();
        const float nDotH = std::max(hit.normal.dot(halfWay), 0.0f);
        const Vec3 specular = mat.specular * radiance * std::pow(nDotH, mat.specularExponent);

        const float shadowValue = shootShadowRays(light, hit.position, hit.normal, rng);

        finalColor += (diffuse + specular) * shadowValue * (1.0f - mat.reflection);
    }

    // Reflection is a property of the surface, not of any one light, so it is traced
    // once per hit rather than once per light in the loop above.
    if (mat.reflection > 0)
    {
        Ray reflectedRay = hit.ray.getReflectionRay(hit.position, hit.normal);
        Hit reflectionHit = shootRay(reflectedRay);

        if (reflectionHit.isHit)
        {
            Vec3 reflectionColor = getHitColor(reflectionHit, recurseLevel + 1, rng);
            finalColor += reflectionColor * mat.reflection;
        }
    }

    return finalColor;
}

auto RayTracer::shootShadowRays(const std::shared_ptr<Light> &light, Vec3 pos, Vec3 normal, Rng &rng) -> float
{
    const Vec3 lightCenterPos = light->getPos();
    const Vec3 toLight = lightCenterPos - pos;

    // Build a basis spanning the light's disc, perpendicular to the direction towards it.
    // Crossing with a fixed axis is degenerate when the light lies along that axis, which
    // produced a zero-length vector and then NaN sample positions, so pick a reference
    // axis that cannot be parallel to the light direction.
    const Vec3 lightDirN = toLight.normalized();
    const Vec3 reference = (std::fabs(lightDirN.y) > 0.9f) ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f);

    const Vec3 u = lightDirN.cross(reference).normalized();
    const Vec3 v = lightDirN.cross(u).normalized();

    // Start shadow rays just off the surface so they cannot immediately re-hit it.
    const Vec3 origin = Ray::offsetOrigin(pos, normal);

    unsigned int litSources = 0;

    for (unsigned int i = 0; i < m_NumShadowRays; i++)
    {
        // Sample the light's disc uniformly by area. Sampling a square, as this did
        // before, gives a square light and biases samples towards the corners.
        const float radius = light->getRadius() * std::sqrt(rng.nextFloat());
        const float angle = rng.nextFloat() * 2.0f * static_cast<float>(M_PI);

        const Vec3 lightPos = lightCenterPos + (u * (radius * std::cos(angle))) + (v * (radius * std::sin(angle)));

        const Vec3 shadowDir = lightPos - origin;
        const float lightDist = shadowDir.length();

        // Bound the ray at the light: anything beyond it cannot cast a shadow, and this
        // turns a closest-hit search into an occlusion test.
        Ray shadowRay(origin, shadowDir, Ray::defaultEpsilon, lightDist - Ray::defaultEpsilon);

        if (!shootRay(shadowRay).isHit)
        {
            litSources++;
        }
    }

    return static_cast<float>(litSources) / static_cast<float>(m_NumShadowRays);
}
