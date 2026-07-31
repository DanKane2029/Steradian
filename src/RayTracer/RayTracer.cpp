#include "RayTracer.h"

#include "Utils/Stats.h"

#include <cmath>
#include <cstdint>

RayTracer::RayTracer(PixelBuffer *pixelBuffer, Scene *scene, Config &config)
    : m_Scene(scene), m_PixelBuffer(pixelBuffer)
{
    auto size = m_PixelBuffer->getSize();

    m_FovX = atan2f((float)size.first, 2.0f);
    m_FovY = atan2f((float)size.second, 2.0f);

    m_aspectRatio = (float)size.first / (float)size.second;
    m_NumShadowRays = config.numShadowRays;
    m_MaxRecurseLevel = config.maxRecurseLevel;

    vx = tanf(m_FovX / 2.0f);
    vy = tanf(m_FovY / 2.0f);
}

RayTracer::~RayTracer() = default;

void RayTracer::updateAspectRatio(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
}

auto RayTracer::makeCameraRay(float u, float v) -> Ray
{
    Camera camera = m_Scene->getCamera();

    // NOTE: the film offsets below are applied in world XY rather than in the camera's
    // own basis, so the camera only behaves correctly when looking along +/-Z, and the
    // field of view is derived from the pixel count rather than from the scene. Both
    // are left alone here so this change stays behaviour preserving; they are fixed in
    // the camera work of a later stage.
    const float worldX = ((u * 2.0f) - 1.0f) * vx * m_aspectRatio;
    const float worldY = ((v * 2.0f) - 1.0f) * vy;

    const Vec3 pij = (camera.org + camera.dir) + Vec3(worldX, worldY, 0.0f);

    // get the direction from camera origin to the sampled point
    Vec3 dir = pij - camera.org;
    dir.normalize();

    // create a ray that starts at the camera origin and passes through the sampled
    // point on the view plane
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

    for (std::shared_ptr<Light> light : m_Scene->getLightList())
    {
        // diffuse component
        Vec3 lightDir = light->getPos() - hit.position;
        float lightDist = lightDir.length();
        lightDir.normalize();

        Vec3 lightDiffuse = light->getColor() * light->getIntensity() * (light->getIntensity() / lightDist);
        Vec3 dotDiffuse = std::max(lightDir.dot(hit.normal), 0.0f);
        Vec3 diffuse = lightDiffuse * dotDiffuse * mat.diffuse;

        // specular component (Jim Blinn)
        Vec3 view = m_Scene->getCamera().org - hit.position;
        view.normalize();

        Vec3 halfWay = lightDir + view;
        halfWay.normalize();

        Vec3 specular = mat.specular * powf(hit.normal.dot(halfWay), mat.specularExponent);

        // shadow value
        float shadowValue = shootShadowRays(light, hit.position, rng);

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

auto RayTracer::shootShadowRays(std::shared_ptr<Light> light, Vec3 pos, Rng &rng) -> float
{
    Vec3 lightCenterPos = light->getPos();
    Vec3 lightCenterDir = lightCenterPos - pos;

    Vec3 up = lightCenterDir.cross({0.0f, 1.0f, 0.0f});

    Vec3 u = lightCenterDir.cross(up);
    u.normalize();

    Vec3 v = lightCenterDir.cross(u);
    v.normalize();

    unsigned int litSources = 0;

    for (unsigned int i = 0; i < m_NumShadowRays; i++)
    {
        float rx = rng.nextFloatSigned() * light->getRadius();
        float ry = rng.nextFloatSigned() * light->getRadius();

        Vec3 lightPos = lightCenterPos + (u * rx) + (v * ry);

        Vec3 lightDir = lightPos - pos;
        float lightDist = lightDir.length();
        lightDir.normalize();

        Ray shadowRay(pos, lightDir);
        Hit shadowHit = shootRay(shadowRay);

        if (lightDist < shadowHit.time)
        {
            litSources++;
        }
    }

    return static_cast<float>(litSources) / static_cast<float>(m_NumShadowRays);
}
