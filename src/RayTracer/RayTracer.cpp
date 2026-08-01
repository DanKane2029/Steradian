#include "RayTracer.h"

#include "Utils/Stats.h"

#include <cmath>
#include <cstdint>

RayTracer::RayTracer(PixelBuffer *pixelBuffer, Scene *scene, Config &config)
    : m_Scene(scene), m_PixelBuffer(pixelBuffer)
{
    auto size = m_PixelBuffer->getSize();

    m_aspectRatio = (float)size.first / (float)size.second;
    m_MaxDepth = config.maxRecurseLevel;

    m_Integrator = std::make_unique<Integrator>(scene, m_MaxDepth);
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

auto RayTracer::samplePixel(int ix, int iy, float jitterX, float jitterY, Rng &rng, Vec3 &albedo, Vec3 &normal) -> Vec3
{
    auto size = m_PixelBuffer->getSize();

    const float u = (static_cast<float>(ix) + jitterX) / static_cast<float>(size.first);
    const float v = (static_cast<float>(iy) + jitterY) / static_cast<float>(size.second);

    return m_Integrator->radiance(makeCameraRay(u, v), rng, albedo, normal);
}

void RayTracer::sampleScene(float x, float y)
{
    auto size = m_PixelBuffer->getSize();

    const int ix = static_cast<int>(floorf(static_cast<float>(size.first - 1) * x));
    const int iy = static_cast<int>(floorf(static_cast<float>(size.second - 1) * y));

    // The interactive viewer samples random points from several threads at once, so each
    // thread needs its own generator rather than a shared one.
    static thread_local Rng rng(0x9e3779b97f4a7c15ULL, reinterpret_cast<uintptr_t>(&rng));

    Vec3 albedo;
    Vec3 normal;
    const Vec3 color = samplePixel(ix, iy, rng.nextFloat(), rng.nextFloat(), rng, albedo, normal);

    m_PixelBufferGuard.lock();
    m_PixelBuffer->setSample(ix, iy, color, albedo, normal);
    m_PixelBufferGuard.unlock();
}

void RayTracer::renderRows(int yStart, int yEnd, unsigned int samplesPerPixel, uint64_t seed)
{
    auto size = m_PixelBuffer->getSize();
    const int width = size.first;

    // Counters are thread-local while rendering and merged once at the end, so the hot
    // path stays lock-free.
    Stats::resetThread();

    // Samples are stratified over a grid rather than drawn independently. Independent
    // samples clump together by chance, leaving parts of the pixel uncovered; placing one
    // sample in each cell of a grid and jittering within the cell covers the pixel far
    // more evenly and converges faster for the same sample count.
    auto strataPerAxis = static_cast<unsigned int>(std::sqrt(static_cast<float>(samplesPerPixel)));
    if (strataPerAxis == 0)
    {
        strataPerAxis = 1;
    }
    const unsigned int stratifiedSamples = strataPerAxis * strataPerAxis;

    for (int iy = yStart; iy < yEnd; iy++)
    {
        // Seeding per row rather than per thread is what makes the output independent of
        // how the image was divided up between workers.
        Rng rng(seed, static_cast<uint64_t>(iy));

        for (int ix = 0; ix < width; ix++)
        {
            for (unsigned int s = 0; s < samplesPerPixel; s++)
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
                    // Any samples beyond a full grid are taken uniformly.
                    jitterX = rng.nextFloat();
                    jitterY = rng.nextFloat();
                }

                Vec3 albedo;
                Vec3 normal;
                const Vec3 color = samplePixel(ix, iy, jitterX, jitterY, rng, albedo, normal);

                m_PixelBuffer->setSample(ix, iy, color, albedo, normal);
            }
        }
    }

    Stats::mergeThread();
}
