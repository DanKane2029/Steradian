#pragma once
#include <cmath>
#include <cstdint>

/**
 * \brief What an intersection test reports: where along the ray, and which primitive.
 *
 * Sixteen bytes, and deliberately nothing more. This record is produced by every
 * primitive test a ray performs -- hundreds of them against a mesh -- and all but one of
 * those results is discarded. It previously carried a position, a normal, a texture
 * coordinate, a material index and a copy of the ray, all computed on every test and
 * almost always thrown away.
 *
 * The attributes shading needs are recovered from this once, for the winner, by
 * Geometry::surfaceAt.
 */
struct Hit
{
    /** \brief Distance along the ray, in the units of its direction vector. */
    float time = INFINITY;

    /**
     * \brief Barycentric weights of the second and third vertices.
     *
     * Produced for free by the triangle test, which has to compute them anyway to decide
     * whether the intersection lies inside the triangle. Unused for spheres, whose
     * surface point follows from the distance alone.
     */
    float u = 0.0f;
    float v = 0.0f;

    /** \brief Index in the scene's primitive space, or noPrimitive when nothing was hit. */
    uint32_t primitive = noPrimitive;

    static constexpr uint32_t noPrimitive = 0xFFFFFFFFu;

    auto isHit() const -> bool
    {
        return primitive != noPrimitive;
    }
};
