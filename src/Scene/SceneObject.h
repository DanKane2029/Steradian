#pragma once
#include <cstdint>
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

    void setMaterialIndex(uint32_t index)
    {
        m_MaterialIndex = index;
    }

    auto getMaterialIndex() const -> uint32_t
    {
        return m_MaterialIndex;
    }

    /** marks this primitive as a light that direct lighting samples explicitly */
    void setEmitterIndex(int32_t index)
    {
        m_EmitterIndex = index;
    }

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
    uint32_t m_MaterialIndex = 0;
    int32_t m_EmitterIndex = -1;
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

    /** attaches per-vertex texture coordinates, as loaded from a model file */
    void setTextureCoords(Vec3 t0, Vec3 t1, Vec3 t2);

    /** the geometric normal of the triangle's plane */
    auto faceNormal() const -> Vec3;

    /** interpolates vertex normals from barycentric coordinates (u, v) */
    auto interpolateNormal(float u, float v) const -> Vec3;

    /** interpolates vertex texture coordinates from barycentric coordinates (u, v) */
    auto interpolateTextureCoord(float u, float v) const -> Vec3;

  private:
    Vec3 point0;
    Vec3 point1;
    Vec3 point2;

    Vec3 normal0;
    Vec3 normal1;
    Vec3 normal2;

    Vec3 texCoord0;
    Vec3 texCoord1;
    Vec3 texCoord2;

    bool hasNormalVertices = false;
    bool hasTextureCoords = false;
};

class Sphere : public SceneObject
{
  public:
    ~Sphere() override = default;
    Sphere(Vec3 center, float radius);

    Vec3 getNormal(Vec3 position) override;
    Hit rayIntersect(Ray ray) override;
    Vec3 getCenterPoint() override;

    /** maps a point on the sphere to spherical texture coordinates in [0, 1] */
    auto getTextureCoords(Vec3 pointOnSurface) const -> Vec3;

  private:
    Vec3 m_Center{};
    float m_Radius;
};
