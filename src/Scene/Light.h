#pragma once
#include "Utils/Vec3.h"

/**
 * the base class for all lights in the scene
 */
class Light
{
  public:
    auto inline getPos() -> Vec3
    {
        return m_Pos;
    };
    auto inline getColor() -> Vec3
    {
        return m_Color;
    };
    auto inline getIntensity() -> Vec3
    {
        return m_Intensity;
    };
    auto inline getRadius() -> float
    {
        return m_Radius;
    };

  protected:
    Vec3 m_Pos;
    Vec3 m_Color;
    float m_Intensity{};
    float m_Radius{};
};

class PointLight : public Light
{
  public:
    PointLight(Vec3 pos, Vec3 color, float i, float radius);
};
