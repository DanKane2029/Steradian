#pragma once
#include <string>
#include <vector>

#include "BoundingBox.h"
#include "RayTracer/Hit.h"
#include "RayTracer/Ray.h"
#include "Utils/Vec3.h"

/**
 * the base class of all objects in a scene
 */
class SceneObject
{
  public:
    virtual ~SceneObject() = default;
    virtual Vec3 getNormal(Vec3 position) = 0;
    virtual Hit rayIntersect(Ray ray) = 0;
    virtual Vec3 getCenterPoint() = 0;
    virtual BoundingBox getBoundingBox() = 0;

    void setMaterialName(std::string &name);

  protected:
    std::string m_MaterialName;
    BoundingBox m_BoundingBox;
};

class Triangle : public SceneObject
{
  public:
    ~Triangle() override = default;
    Triangle(Vec3 p0, Vec3 p1, Vec3 p2);

    Vec3 getNormal(Vec3 position) override;
    Hit rayIntersect(Ray ray) override;
    Vec3 getCenterPoint() override;
    BoundingBox getBoundingBox() override;
    auto getPoints() -> std::vector<Vec3>;

  private:
    Vec3 m_Points[3];
};

class Sphere : public SceneObject
{
  public:
    ~Sphere() override = default;
    Sphere(Vec3 center, float radius);

    Vec3 getNormal(Vec3 position) override;
    Hit rayIntersect(Ray ray) override;
    Vec3 getCenterPoint() override;
    BoundingBox getBoundingBox() override;

  private:
    Vec3 m_Center{};
    float m_Radius;
};
