#pragma once
#include <string>
#include <vector>

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

    void setMaterialName(std::string &name);

    float getMinX()
    {
        return minX;
    }
    float getMinY()
    {
        return minY;
    }
    float getMinZ()
    {
        return minZ;
    }
    float getMaxX()
    {
        return maxX;
    }
    float getMaxY()
    {
        return maxY;
    }
    float getMaxZ()
    {
        return maxZ;
    }

  protected:
    std::string m_MaterialName;
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

class Triangle : public SceneObject
{
  public:
    ~Triangle() override = default;
    Triangle() = default;
    Triangle(Vec3 p0, Vec3 p1, Vec3 p2);
    Triangle(Vec3 p0, Vec3 n0, Vec3 p1, Vec3 n1, Vec3 p2, Vec3 n2);

    Vec3 getNormal(Vec3 position) override;
    Hit rayIntersect(Ray ray) override;
    Vec3 getCenterPoint() override;
    auto getPoints() -> std::vector<Vec3>;

  private:
    Vec3 point0;
    Vec3 point1;
    Vec3 point2;

    Vec3 normal0;
    Vec3 normal1;
    Vec3 normal2;
    bool hasNormalVertices;
};

class Sphere : public SceneObject
{
  public:
    ~Sphere() override = default;
    Sphere(Vec3 center, float radius);

    Vec3 getNormal(Vec3 position) override;
    Hit rayIntersect(Ray ray) override;
    Vec3 getCenterPoint() override;

  private:
    Vec3 m_Center{};
    float m_Radius;
};
