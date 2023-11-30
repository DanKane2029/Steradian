#pragma once
#include <mutex>

#include "Camera.h"
#include "Hit.h"
#include "Scene/Scene.h"
#include "Window/PixelBuffer.h"

/**
 * simulates how light interacts with objects to render an image of scene
 */
class RayTracer
{
  public:
    RayTracer(PixelBuffer *pixelBuffer, Scene *scene);
    ~RayTracer();

    void sampleScene(float x, float y);
    auto shootRay(Ray ray) -> Hit;
    auto getHitColor(Hit hit) -> Vec3;
    auto shootShadowRays(Light *light, Vec3 pos) -> float;

  private:
    unsigned int m_NumShadowRays = 50;
    unsigned int m_ReflectionLimit = 100;

    float m_FovX{}, m_FovY{};
    Camera m_Camera;

    Scene *m_Scene{};
    PixelBuffer *m_PixelBuffer{};
    std::mutex m_PixelBufferGuard;
};