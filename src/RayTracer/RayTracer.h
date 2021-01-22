#pragma once
#include "Camera.h"
#include "PixelBuffer.h"
#include "Scene.h"
#include "Hit.h"

#include <mutex>

/**
 * simulates how light interacts with objects to render an image of scene
 */
class RayTracer
{
public:
	RayTracer(PixelBuffer* pixelBuffer, Scene* scene, Camera camera);
	~RayTracer();

	void SampleScene(float x, float y);
	Hit ShootRay(Ray ray);
	Vec3 GetHitColor(Hit hit);
	float ShootShadowRays(Light* light, Vec3 pos);

private:
	unsigned int m_NumShadowRays = 50;
	unsigned int m_ReflectionLimit = 100;

	float m_FovX, m_FovY;
	Camera m_Camera;

	Scene* m_Scene;
	PixelBuffer* m_PixelBuffer;
	std::mutex m_PixelBufferGuard;
};