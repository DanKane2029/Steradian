#include "Light.h"

/**
 * creates a point light
 * 
 * \param pos - the position of the light
 * \param color - the color of the light
 * \param i - the light intensity
 * \param radius - the radius of the sphere of the light
 *				used when calculating soft shadows
 */
PointLight::PointLight(Vec3 pos, Vec3 color, float i, float radius)
{
	m_Pos = pos;
	m_Color = color;
	m_Intensity = i;
	m_Radius = radius;
}
