#include "RayTracer.h"

#include <cmath>

/**
 * creates a ray tracer that can be used to trace an image of the given
 * scene from the perspective of the camera and writing the image to the
 * pixel buffer
 *
 * \param pixelBuffer - buffer of pixels where the scene image os stored
 * \param scene - the scene to be traced
 * \param camera - the perspective that the image will have of the scene
 */
RayTracer::RayTracer(PixelBuffer *pixelBuffer, Scene *scene) : m_PixelBuffer(pixelBuffer), m_Scene(scene)
{
    auto size = m_PixelBuffer->getSize();
    m_FovX = atan2f((float)size.first, 2.0f);
    m_FovY = atan2f((float)size.second, 2.0f);
}

RayTracer::~RayTracer() = default;

/**
 * shoots one ray into the scene in a random direction within the bounds
 * of the camera and writes the sampled color to the pixel buffer
 *
 * \param x - float value between [0, 1) representing the x position of the
 *			point to sample on the view plane, where 0 is the left most x
 *          value and 1 is the right most x value
 * \param y - float value between [0, 1) representing the y position of the
 *          point to sample on the view plane, where 0 is the bottom most y
 *          value and 1 is the top most y value
 */
void RayTracer::sampleScene(float x, float y)
{
    auto size = m_PixelBuffer->getSize();
    auto width = (float)size.first;
    auto height = (float)size.second;

    Camera camera = m_Scene->getCamera();

    // find perpendicular vectors to the camera look direction vector
    Vec3 vx = camera.dir.cross(Vec3(0.0f, 1.0f, 0.0f));
    Vec3 vy = vx.cross(camera.dir);

    // calculate height and width of a single pixel on the view plane
    float pixelWidth = 2.0f * tan(m_FovX / 2.0f) / width;
    float pixelHeight = 2.0f * tan(m_FovY / 2.0f) / height;

    // get the position of the bottom left point on the view plane
    Vec3 p0 = camera.org + camera.dir - (vy * pixelHeight * (height / 2.0f)) - (vx * pixelWidth * (width / 2.0f));

    // calculate the position of the sampled point on the view plane
    Vec3 pij = p0 + (vx * pixelWidth * x * width) + (vy * pixelHeight * y * height);

    // get the direction from camera origin to the sampled point
    Vec3 dir = pij - camera.org;
    dir.normalize();

    // create ray that will start at the camera origin and pass through the
    // sampled point on the view plane
    Ray ray(camera.org, dir);
    Hit hit = shootRay(ray);

    if (hit.isHit)
    {
        Vec3 color = getHitColor(hit);

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

/**
 * tests if a ray intersects any scene object within a scene
 * TODO: use acceleration structure to speed up intersection test
 *
 * \param ray - the ray that passes through the scene
 * \return - a hit object detailing if the ray hit an object and
 *			other relevant information
 */
auto RayTracer::shootRay(Ray ray) -> Hit
{
    Hit hit;

    for (std::shared_ptr<SceneObject> so : m_Scene->getObjectList())
    {
        Hit curHit = so->rayIntersect(ray);
        if (curHit.isHit && curHit.time < hit.time)
        {
            hit = curHit;
        }
    }

    return hit;
}

/**
 * calculates the color of the hit using the Phong reflection model
 * https://en.wikipedia.org/wiki/Phong_reflection_model
 * TODO: add reflections to equation, may requires adding ray as input
 *
 * \param hit - the hit in the scene to color
 * \return - the final color of the hit
 */
auto RayTracer::getHitColor(Hit hit) -> Vec3
{
    Material mat = *(m_Scene->getMaterial(hit.materialName));

    Vec3 finalColor = 0;

    finalColor += m_Scene->getAmbientLighting() * (mat.color * mat.ambient);

    for (std::shared_ptr<Light> light : m_Scene->getLightList())
    {
        // diffuse component
        Vec3 lightDir = light->getPos() - hit.position;
        lightDir.normalize();
        Vec3 diffuse = light->getColor() * std::max(lightDir.dot(hit.normal), 0.0f) * mat.diffuse;

        // specular component (Jim Blinn)
        Vec3 view = m_Scene->getCamera().org - hit.position;
        view.normalize();

        Vec3 halfWay = lightDir + view;
        halfWay.normalize();

        Vec3 specular = powf(hit.normal.dot(halfWay), mat.specular);

        // shadow value
        float shadowValue = shootShadowRays(light, hit.position);

        finalColor += (diffuse + specular); // * shadowValue;
    }

    return finalColor;
}

/**
 * shoots multiple rays from a position to determin how shaded that area is
 * uses monte carlo enstimation to sample rays
 *
 * \param light - the light to test against
 * \param pos - the position that is being shaded
 * \return - a [0, 1] value where 0 is completely shaded and 1 is completely
 * lit
 */
auto RayTracer::shootShadowRays(std::shared_ptr<Light> light, Vec3 pos) -> float
{
    Vec3 lightCenterPos = light->getPos();
    Vec3 lightCenterDir = lightCenterPos - pos;

    float rx = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    float ry = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

    Vec3 u = lightCenterDir.cross({0.0f, 1.0f, 0.0f});
    Vec3 v = lightCenterDir.cross(u);

    unsigned int litSources = 0;

    for (unsigned int i = 0; i < m_NumShadowRays; i++)
    {
        Vec3 lightPos =
            lightCenterPos + (u * (0.5f - rx) * light->getRadius()) + (v * (0.5f - ry) * light->getRadius());

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
