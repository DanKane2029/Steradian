#include "Light.h"

PointLight::PointLight(Vec3 pos, Vec3 color, float i, float radius)
{
	m_Pos = pos;
	m_Color = color;
	m_Intensity = i;
	m_Radius = radius;
}
