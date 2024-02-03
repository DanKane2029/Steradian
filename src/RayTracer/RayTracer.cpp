#include "RayTracer.h"

#include <cmath>

RayTracer::RayTracer(PixelBuffer *pixelBuffer, Scene *scene, Config &config)
    : m_PixelBuffer(pixelBuffer), m_Scene(scene)
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

void RayTracer::sampleScene(float x, float y)
{
    auto size = m_PixelBuffer->getSize();
    auto width = (float)size.first;
    auto height = (float)size.second;

    Camera camera = m_Scene->getCamera();

    // find perpendicular vectors to the camera look direction vector
    // Vec3 vx = camera.dir.cross(Vec3(0.0f, 1.0f, 0.0f));
    // Vec3 vy = vx.cross(camera.dir);

    // // calculate height and width of a single pixel on the view plane
    // float pixelWidth = 2.0f * tan(m_FovX / 2.0f) / width;
    // float pixelHeight = 2.0f * tan(m_FovY / 2.0f) / height;

    // // get the position of the bottom left point on the view plane
    // Vec3 p0 = camera.org + camera.dir - (vy * pixelHeight * (height / 2.0f)) - (vx * pixelWidth * (width / 2.0f));

    // // calculate the position of the sampled point on the view plane
    // Vec3 pij = p0 + (vx * pixelWidth * x * width) + (vy * pixelHeight * y * height);

    float worldX = ((x * 2.0f) - 1.0f) * vx * m_aspectRatio;
    float worldY = ((y * 2.0f) - 1.0f) * vy;

    Vec3 pij = (camera.org + camera.dir) + Vec3(worldX, worldY, 0.0f);

    // get the direction from camera origin to the sampled point
    Vec3 dir = pij - camera.org;
    dir.normalize();

    // create ray that will start at the camera origin and pass through the
    // sampled point on the view plane
    Ray ray(camera.org, dir);

    Hit hit = shootRay(ray);

    if (hit.isHit)
    {
        unsigned int recurseLevel = 0;
        Vec3 color = getHitColor(hit, recurseLevel);

        int ix = static_cast<int>(floorf(static_cast<float>(size.first - 1) * x));
        int iy = static_cast<int>(floorf(static_cast<float>(size.second - 1) * y));

        m_PixelBufferGuard.lock();
        m_PixelBuffer->setPixel(ix, iy, color);
        m_PixelBufferGuard.unlock();
    }
    else
    {
        int ix = static_cast<int>(floorf(static_cast<float>(size.first - 1) * x));
        int iy = static_cast<int>(floorf(static_cast<float>(size.second - 1) * y));

        m_PixelBufferGuard.lock();
        m_PixelBuffer->setPixel(ix, iy, Vec3());
        m_PixelBufferGuard.unlock();
    }
}

auto RayTracer::shootRay(Ray ray) -> Hit
{
    Hit hit = m_Scene->getAccelerationStructure()->root->rayIntersect(ray);
    hit.ray = ray;
    return hit;
}

auto RayTracer::getHitColor(Hit hit, unsigned int recurseLevel) -> Vec3
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
        float shadowValue = shootShadowRays(light, hit.position);

        finalColor += (diffuse + specular) * shadowValue * (1.0f - mat.reflection);

        if (mat.reflection > 0)
        {
            Ray reflectedRay = hit.ray.getReflectionRay(hit.position, hit.normal);
            Hit reflectionHit = shootRay(reflectedRay);

            if (reflectionHit.isHit)
            {
                Vec3 reflectionColor = getHitColor(reflectionHit, ++recurseLevel);
                finalColor += reflectionColor * mat.reflection;
            }
        }
    }

    return finalColor;
}

auto RayTracer::shootShadowRays(std::shared_ptr<Light> light, Vec3 pos) -> float
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
        float rx = ((static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f) - 1) * light->getRadius();
        float ry = ((static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f) - 1) * light->getRadius();

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
