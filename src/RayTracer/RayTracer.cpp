#include "RayTracer.h"

RayTracer::RayTracer(PixelBuffer* pixelBuffer, Scene* scene, Camera camera, float fovX, float fovY)
	: m_PixelBuffer(pixelBuffer), m_Scene(scene), m_Camera(camera), m_FovX(fovX), m_FovY(fovY)
{
}

RayTracer::~RayTracer()
{
}

void RayTracer::SampleScene(float x, float y)
{
	auto size = m_PixelBuffer->GetSize();
	float width = (float) size.first;
	float height = (float) size.second;

	Vec3 Vx = m_Camera.dir.Cross(Vec3(0.0f, 1.0f, 0.0f));
	Vec3 Vy = Vx.Cross(m_Camera.dir);

	float pixelWidth = 2.0f * tan(m_FovX / 2) / width;
	float pixelHeight = 2.0f * tan(m_FovY / 2) / height;

	Vec3 P0 = m_Camera.org + m_Camera.dir - 
		(Vy * pixelHeight * (height / 2.0f)) - (Vx * pixelWidth * (width / 2.0f));

	Vec3 Pij = P0 + (Vx * pixelWidth * x * width) + (Vy * pixelHeight * y * height);

	Vec3 dir = Pij - m_Camera.org;
	dir.normalize();

	Ray ray(m_Camera.org, dir);
	Hit hit = ShootRay(ray);

	if (hit.isHit)
	{
		unsigned int ix = (unsigned int) floorf((size.first-1) * x);
		unsigned int iy = (unsigned int) floorf((size.second-1) * y);
		Material* material = m_Scene->GetMaterial(hit.materialName);
		m_PixelBuffer->SetPixel(ix, iy, material->color);
	}
}

Hit RayTracer::ShootRay(Ray ray)
{
	Hit hit;

	for (SceneObject* so : m_Scene->GetObjectList())
	{
		Hit curHit = so->RayIntersect(ray);
		if (curHit.isHit && curHit.time < hit.time)
		{
			hit = curHit;
		}
	}

	return hit;
}
