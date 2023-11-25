#include "RayTracer.h"

/**
 * creates a ray tracer that can be used to trace an image of the given 
 * scene from the perspective of the camera and writing the image to the 
 * pixel buffer
 * 
 * \param pixelBuffer - buffer of pixels where the scene image os stored
 * \param scene - the scene to be traced
 * \param camera - the perspective that the image will have of the scene
 */
RayTracer::RayTracer(PixelBuffer* pixelBuffer, Scene* scene, Camera camera)
	: m_PixelBuffer(pixelBuffer), m_Scene(scene), m_Camera(camera)
{
	auto size = m_PixelBuffer->GetSize();
	m_FovX = atan2f((float) size.first, 2.0f);
	m_FovY = atan2f((float) size.second, 2.0f);
}

RayTracer::~RayTracer()
{
}

/**
 * shoots one ray into the scene in a random direction within the bounds
 * of the camera and writes the sampled color to the pixel buffer
 * 
 * \param x - float value between [0, 1) representing the x position of the 
 *			point to sample on the view plane, where 0 is the left most x value
 *			and 1 is the right most x value
 * \param y - float value between [0, 1) representing the y position of the 
 *			point to sample on the view plane, where 0 is the bottom most y value
 *			and 1 is the top most y value
 */
void RayTracer::SampleScene(float x, float y)
{
	auto size = m_PixelBuffer->GetSize();
	float width = (float) size.first;
	float height = (float) size.second;

	// find perpendicular vectors to the camera look direction vector
	Vec3 Vx = m_Camera.dir.Cross(Vec3(0.0f, 1.0f, 0.0f));
	Vec3 Vy = Vx.Cross(m_Camera.dir);

	// calculate height and width of a single pixel on the view plane
	float pixelWidth = 2.0f * tan(m_FovX / 2.0f) / width;
	float pixelHeight = 2.0f * tan(m_FovY / 2.0f) / height;

	// get the position of the bottom left point on the view plane
	Vec3 P0 = m_Camera.org + m_Camera.dir - 
		(Vy * pixelHeight * (height / 2.0f)) - (Vx * pixelWidth * (width / 2.0f));

	// calculate the position of the sampled point on the view plane
	Vec3 Pij = P0 + (Vx * pixelWidth * x * width) + (Vy * pixelHeight * y * height);

	// get the direction from camera origin to the sampled point
	Vec3 dir = Pij - m_Camera.org;
	dir.normalize();

	// create ray that will start at the camera origin and pass through the 
	// sampled point on the view plane
	Ray ray(m_Camera.org, dir);
	Hit hit = ShootRay(ray);

	if (hit.isHit)
	{
		Vec3 color = GetHitColor(hit);

		unsigned int ix = (unsigned int) floorf((size.first-1) * x);
		unsigned int iy = (unsigned int) floorf((size.second-1) * y);

		m_PixelBufferGuard.lock();
		m_PixelBuffer->SetPixel(ix, iy, color);
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
Hit RayTracer::ShootRay(Ray ray)
{
	Hit hit;

	for (std::shared_ptr<SceneObject> so : m_Scene->GetObjectList())
	{
		Hit curHit = so->RayIntersect(ray);
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
Vec3 RayTracer::GetHitColor(Hit hit)
{
	Material mat = *m_Scene->GetMaterial(hit.materialName);

	Vec3 finalColor;

	finalColor += mat.color * m_Scene->GetAmbientLighting() * mat.ambient;

	for (Light* light : m_Scene->GetLightList())
	{
		Vec3 lightDir = light->GetPos() - hit.position;
		lightDir.normalize();

		Vec3 diffuse = light->GetColor() * mat.diffuse *
			std::max(lightDir.Dot(hit.normal), 0.0f);

		Vec3 reflection = lightDir - (hit.normal * 2.0f * lightDir.Dot(hit.normal));
		reflection.normalize();

		Vec3 view = hit.position - m_Camera.org;
		view.normalize();

		Vec3 specular = Vec3(1.0f, 1.0f, 1.0f) * mat.specular *
			powf(std::max(reflection.Dot(view), 0.0f), mat.shininess);

		float shadowValue = ShootShadowRays(light, hit.position);

		finalColor += (diffuse + specular) * shadowValue;
	}

	return finalColor;
}

/**
 * shoots multiple rays from a position to determin how shaded that area is
 * uses monte carlo enstimation to sample rays
 * 
 * \param light - the light to test against
 * \param pos - the position that is being shaded
 * \return - a [0, 1] value where 0 is completely shaded and 1 is completely lit
 */
float RayTracer::ShootShadowRays(Light* light, Vec3 pos)
{
	Vec3 lightCenterPos = light->GetPos();
	Vec3 lightCenterDir = lightCenterPos - pos;

	float rx = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	float ry = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

	Vec3 u = lightCenterDir.Cross({0.0f, 1.0f, 0.0f});
	Vec3 v = lightCenterDir.Cross(u);

	unsigned int litSources = 0;

	for (unsigned int i = 0; i < m_NumShadowRays; i++)
	{
		Vec3 lightPos = lightCenterPos 
			+ (u * (0.5f - rx) * light->GetRadius())
			+ (v * (0.5f - ry) * light->GetRadius());

		Vec3 lightDir = lightPos - pos;
		float lightDist = lightDir.Length();
		lightDir.normalize();

		Ray shadowRay(pos, lightDir);
		Hit shadowHit = ShootRay(shadowRay);

		if (lightDist < shadowHit.time)
		{
			litSources++;
		}
	}

	return (float) litSources / (float) m_NumShadowRays;
}
