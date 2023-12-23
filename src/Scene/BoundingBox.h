#pragma once
#include "RayTracer/Hit.h"
#include "RayTracer/Ray.h"
#include "SceneObject.h"

#include <utility>
#include <vector>

/**
 * An axis aligned bounging box used for fast intersection testing
 */
struct BoundingBox
{
    float minX, minY, minZ;
    float maxX, maxY, maxZ;

    BoundingBox()
    {
        this->minX = 0.0f;
        this->minY = 0.0f;
        this->minZ = 0.0f;
        this->maxX = 0.0f;
        this->maxY = 0.0f;
        this->maxZ = 0.0f;
    }

    BoundingBox(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
    {
        this->minX = minX;
        this->minY = minY;
        this->minZ = minZ;

        this->maxX = maxX;
        this->maxY = maxY;
        this->maxZ = maxZ;
    }

    BoundingBox(std::vector<std::shared_ptr<SceneObject>> objectList)
    {
        for (std::shared_ptr<SceneObject> object : objectList)
        {
            this->minX = std::min(minX, object->getMinX());
            this->minY = std::min(minY, object->getMinY());
            this->minZ = std::min(minZ, object->getMinZ());

            this->maxX = std::min(maxX, object->getMaxX());
            this->maxY = std::min(maxY, object->getMaxY());
            this->maxZ = std::min(maxZ, object->getMaxZ());
        }
    }

    bool inside(Vec3 point)
    {
        if (point.x < minX || point.x > maxX)
        {
            return false;
        }

        if (point.y < minY || point.y > maxY)
        {
            return false;
        }

        if (point.z < minZ || point.z > maxZ)
        {
            return false;
        }

        return true;
    }

    Hit rayInersect(Ray ray)
    {
        Hit hit;

        float tmin = (minX - ray.org.x) / ray.dir.x;
        float tmax = (maxX - ray.org.x) / ray.dir.x;

        if (tmin > tmax)
            std::swap(tmin, tmax);

        float tymin = (minY - ray.org.y) / ray.dir.y;
        float tymax = (maxY - ray.org.y) / ray.dir.y;

        if (tymin > tymax)
            std::swap(tymin, tymax);

        if ((tmin > tymax) || (tymin > tmax))
            return hit;

        if (tymin > tmin)
            tmin = tymin;

        if (tymax < tmax)
            tmax = tymax;

        float tzmin = (minZ - ray.org.z) / ray.dir.z;
        float tzmax = (maxZ - ray.org.z) / ray.dir.z;

        if (tzmin > tzmax)
            std::swap(tzmin, tzmax);

        if ((tmin > tzmax) || (tzmin > tmax))
            return hit;

        if (tzmin > tmin)
            tmin = tzmin;

        if (tzmax < tmax)
            tmax = tzmax;

        hit.isHit = true;
        hit.time = tmin;

        return hit;
    }
};
