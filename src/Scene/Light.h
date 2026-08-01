#pragma once
#include "Utils/Vec3.h"

/**
 * the base class for all lights in the scene
 */
class Light
{
  public:
    // Virtual: lights are stored and destroyed through Light pointers, and deriving from
    // a base without a virtual destructor is undefined behaviour on delete.
    virtual ~Light() = default;

    auto inline getPos() const -> Vec3
    {
        return m_Pos;
    };
    auto inline getColor() const -> Vec3
    {
        return m_Color;
    };
    // Returns a float, matching the member. It previously returned Vec3, relying on the
    // implicit broadcast constructor, which made the falloff arithmetic hard to read.
    auto inline getIntensity() const -> float
    {
        return m_Intensity;
    };
    auto inline getRadius() const -> float
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
    ~PointLight() override = default;
    PointLight(Vec3 pos, Vec3 color, float i, float radius);
};
