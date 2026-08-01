// Checks the acceleration structure against brute force.
//
// The structure this replaced silently dropped intersections: it assigned primitives to
// children by centroid while using spatial partition planes as node bounds, so geometry
// straddling a plane fell outside its own node's box. Nothing caught that, because the
// traversal never tested a bounding box at all and so never noticed.
//
// This test removes that whole class of bug from the dark. For a large number of rays it
// compares the BVH's answer against a linear scan over every primitive in the scene. Any
// hit the BVH misses, invents, or reports at the wrong distance fails the test.
//
// Usage: bvh_consistency <scene.json> [rayCount]

#include "Scene/Scene.h"
#include "Utils/Random.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{

/** intersects a ray against every primitive in the scene, ignoring the hierarchy */
auto bruteForce(const std::vector<std::shared_ptr<SceneObject>> &objects, const Ray &ray) -> Hit
{
    Hit closest;
    float bestTime = ray.tMax;

    for (const std::shared_ptr<SceneObject> &object : objects)
    {
        const Hit hit = object->rayIntersect(ray);

        if (hit.isHit && hit.time < bestTime)
        {
            closest = hit;
            bestTime = hit.time;
        }
    }

    return closest;
}

} // namespace

auto main(int argc, char *argv[]) -> int
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <scene.json> [rayCount]\n", argv[0]);
        return 2;
    }

    const int rayCount = (argc > 2) ? std::atoi(argv[2]) : 20000;

    Scene scene(argv[1]);
    scene.createAcceleratedStructure(4);

    const BVH *bvh = scene.getAccelerationStructure();
    if (bvh == nullptr)
    {
        std::fprintf(stderr, "scene produced no acceleration structure\n");
        return 1;
    }

    const std::vector<std::shared_ptr<SceneObject>> &objects = scene.getObjectList();

    std::printf("scene has %zu primitives, %zu nodes, max depth %u\n", bvh->primitiveCount(), bvh->nodeCount(),
                bvh->maxDepth());

    // Aim rays from a shell around the scene towards points inside it, so most of them
    // actually engage with the geometry rather than sailing past.
    AABB sceneBounds;
    for (const std::shared_ptr<SceneObject> &object : objects)
    {
        sceneBounds.expand(Vec3(object->getMinX(), object->getMinY(), object->getMinZ()));
        sceneBounds.expand(Vec3(object->getMaxX(), object->getMaxY(), object->getMaxZ()));
    }

    const Vec3 center = sceneBounds.centroid();
    const float radius = std::max(sceneBounds.extent().length(), 1.0f);

    Rng rng(12345, 6789);

    int mismatches = 0;
    int hits = 0;
    int occlusionMismatches = 0;

    for (int i = 0; i < rayCount; i++)
    {
        const Vec3 origin = center + Vec3(rng.nextFloatSigned(), rng.nextFloatSigned(), rng.nextFloatSigned()) * radius;
        const Vec3 target =
            center + Vec3(rng.nextFloatSigned(), rng.nextFloatSigned(), rng.nextFloatSigned()) * (radius * 0.5f);

        const Ray ray(origin, target - origin);

        const Hit fromBvh = bvh->intersect(ray);
        const Hit fromScan = bruteForce(objects, ray);

        if (fromBvh.isHit != fromScan.isHit)
        {
            if (mismatches < 5)
            {
                std::fprintf(stderr, "ray %d: BVH says %s, brute force says %s\n", i, fromBvh.isHit ? "hit" : "miss",
                             fromScan.isHit ? "hit" : "miss");
            }
            mismatches++;
            continue;
        }

        if (fromScan.isHit)
        {
            hits++;

            // Distances should agree closely. They can differ in the last bits when two
            // primitives overlap and the two orders pick different ones.
            const float delta = std::fabs(fromBvh.time - fromScan.time);
            if (delta > 1e-4f * std::max(1.0f, fromScan.time))
            {
                if (mismatches < 5)
                {
                    std::fprintf(stderr, "ray %d: distance %f from BVH, %f from brute force\n", i, fromBvh.time,
                                 fromScan.time);
                }
                mismatches++;
            }

            // An occlusion query bounded just short of the hit must agree that something
            // is in the way.
            const Ray shadowRay(origin, target - origin, Ray::defaultEpsilon, fromScan.time * 0.999f);
            if (bvh->isOccluded(shadowRay) && !bruteForce(objects, shadowRay).isHit)
            {
                occlusionMismatches++;
            }
        }
    }

    std::printf("%d rays, %d hits, %d mismatches, %d occlusion mismatches\n", rayCount, hits, mismatches,
                occlusionMismatches);

    if (hits == 0)
    {
        std::fprintf(stderr, "no rays hit anything; the test is not exercising the structure\n");
        return 1;
    }

    if (mismatches != 0 || occlusionMismatches != 0)
    {
        std::fprintf(stderr, "acceleration structure disagrees with brute force\n");
        return 1;
    }

    return 0;
}
