// Checks the procedural checker, and in particular the case that makes it fall apart
// silently.
//
// A checker decides its colour from which cell a point falls in, which means every cell
// boundary is a discontinuity. That is fine in the middle of a square and dangerous at its
// edge, because a surface lying exactly on a boundary has no well defined answer: whether a
// hit lands a hair above or a hair below the plane decides its colour, and intersection
// results carry exactly that much error. The result is a floor that comes out speckled
// rather than checkered.
//
// This is not a hypothetical arrangement. Scenes put floors on y = 0 and pick square sizes
// that divide evenly, so the boundary and the floor land on top of each other in the
// ordinary case rather than a contrived one. The checker offsets its grid by half a square
// to move those surfaces into the middle of a cell, and what follows is what that buys:
// colours either side of a plane must agree.

#include "Scene/Material.h"

#include <cmath>
#include <cstdio>

namespace
{

int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition)
    {
        std::printf("  FAIL  %s\n", what);
        failures++;
    }
    else
    {
        std::printf("  ok    %s\n", what);
    }
}

auto sameColour(const Vec3 &a, const Vec3 &b) -> bool
{
    return std::fabs(a.x - b.x) < 1e-6f && std::fabs(a.y - b.y) < 1e-6f && std::fabs(a.z - b.z) < 1e-6f;
}

auto floorMaterial(float squareSize) -> Material
{
    Material m;
    m.albedo = Vec3(0.8f, 0.8f, 0.8f);
    m.checkerAlbedo = Vec3(0.1f, 0.1f, 0.1f);
    m.checkerScale = squareSize;
    return m;
}

/**
 * \brief A plane's colour must not depend on which side of it a hit lands.
 *
 * The offsets used here are the size of the error an intersection routine leaves behind,
 * and are deliberately applied both ways: rounding is only stable if it agrees across the
 * plane, not merely with itself.
 */
void checkPlaneIsStable(float squareSize, float height, const char *what)
{
    const Material m = floorMaterial(squareSize);
    const Vec3 uv{};

    bool stable = true;

    // Swept across the floor rather than tested at one spot, since the failure appears at
    // whichever x and z happen to sit near a boundary rather than everywhere at once.
    for (int i = -40; i <= 40; i++)
    {
        const float x = static_cast<float>(i) * squareSize * 0.5f;
        const float z = static_cast<float>(i) * squareSize * 0.25f;

        const Vec3 exact = m.baseAlbedo(Vec3(x, height, z));
        const Vec3 above = m.baseAlbedo(Vec3(x, height + 1e-6f, z));
        const Vec3 below = m.baseAlbedo(Vec3(x, height - 1e-6f, z));

        stable = stable && sameColour(exact, above) && sameColour(exact, below);
    }

    check(stable, what);
}

} // namespace

int main()
{
    std::printf("checker\n");

    // The arrangement every scene in res/scenes uses.
    checkPlaneIsStable(0.5f, 0.0f, "a floor on y = 0 has one colour per point");

    // Square sizes that divide the height evenly are the ones that put a boundary on the
    // surface, so they are the ones worth checking.
    checkPlaneIsStable(1.0f, 0.0f, "unit squares on y = 0");
    checkPlaneIsStable(0.25f, 2.0f, "a raised floor whose height is a whole number of squares");

    {
        const Material m = floorMaterial(0.5f);
        const Vec3 uv{};

        // Neighbouring squares must actually differ, or the checks above would pass on a
        // material that had quietly stopped drawing a checker at all.
        const Vec3 here = m.baseAlbedo(Vec3(0.0f, 0.0f, 0.0f));
        const Vec3 next = m.baseAlbedo(Vec3(0.5f, 0.0f, 0.0f));
        const Vec3 acrossZ = m.baseAlbedo(Vec3(0.0f, 0.0f, 0.5f));
        const Vec3 diagonal = m.baseAlbedo(Vec3(0.5f, 0.0f, 0.5f));

        check(!sameColour(here, next), "the next square along x is the other colour");
        check(!sameColour(here, acrossZ), "the next square along z is the other colour");
        check(sameColour(here, diagonal), "the diagonal neighbour returns to the first colour");
    }

    {
        // A material with no checker must be untouched by any of this.
        Material plain;
        plain.albedo = Vec3(0.3f, 0.6f, 0.9f);

        check(sameColour(plain.baseAlbedo(Vec3(1.7f, 0.0f, -2.3f)), plain.albedo),
              "a material without a checker keeps its flat albedo");
    }

    if (failures != 0)
    {
        std::printf("\n%d check(s) failed\n", failures);
        return 1;
    }

    std::printf("\nall checks passed\n");
    return 0;
}
