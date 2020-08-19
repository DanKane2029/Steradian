#pragma once
#include "Camera.h"
#include "PixelBuffer.h"
#include "Scene.h"
#include "Hit.h"

class RayTracer
{
public:
	RayTracer(PixelBuffer* pixelBuffer, Scene* scene, Camera camera, float fovX, float fovY);
	~RayTracer();

	void SampleScene(float x, float y);
	bool ShootRay(Ray ray);

private:
	float m_FovX, m_FovY;
	Camera m_Camera;

	Scene* m_Scene;
	PixelBuffer* m_PixelBuffer;
};