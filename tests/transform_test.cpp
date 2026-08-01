// Checks the object placement maths, and in particular the part that is easy to get
// wrong and impossible to notice by eye.
//
// Points and normals do not transform the same way. Scaling an object along one axis
// tilts its surfaces the opposite way to how it moves its vertices, so applying the same
// transform to a normal leaves it no longer perpendicular to the surface it describes.
// The render still looks plausible; the shading is simply wrong, by an amount that
// depends on the scale and the angle, which is not something anyone will spot in an
// image.
//
// The invariant checked here is the one that matters: a normal, transformed correctly,
// stays perpendicular to the surface it belongs to.

#include "Utils/Transform.h"

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

auto nearlyEqual(float a, float b, float tolerance = 1e-4f) -> bool
{
    return std::fabs(a - b) <= tolerance;
}

auto nearlyEqual(const Vec3 &a, const Vec3 &b, float tolerance = 1e-4f) -> bool
{
    return nearlyEqual(a.x, b.x, tolerance) && nearlyEqual(a.y, b.y, tolerance) && nearlyEqual(a.z, b.z, tolerance);
}

/** the geometric normal of the plane through three points */
auto faceNormal(const Vec3 &a, const Vec3 &b, const Vec3 &c) -> Vec3
{
    return (b - a).cross(c - a).normalized();
}

/**
 * Transforms a triangle and checks that its transformed normal still describes the
 * transformed surface.
 */
void checkNormalStaysPerpendicular(const Transform &transform, const char *what)
{
    // A triangle tilted in all three axes, so no axis-aligned special case hides an error.
    const Vec3 a(0.3f, 0.1f, -0.4f);
    const Vec3 b(1.2f, 0.7f, 0.2f);
    const Vec3 c(-0.5f, 1.1f, 0.9f);

    const Vec3 normalBefore = faceNormal(a, b, c);

    const Vec3 ta = transform.transformPoint(a);
    const Vec3 tb = transform.transformPoint(b);
    const Vec3 tc = transform.transformPoint(c);

    const Vec3 expected = faceNormal(ta, tb, tc);
    const Vec3 actual = transform.transformNormal(normalBefore);

    // The two may point in opposite directions, since a normal's sign is a convention and
    // a mirroring scale flips the winding. Only the axis matters.
    const float alignment = std::fabs(expected.dot(actual));

    if (!nearlyEqual(alignment, 1.0f, 1e-3f))
    {
        std::printf("  FAIL  %s (alignment %.5f, should be 1.0)\n", what, alignment);
        failures++;
    }
    else
    {
        std::printf("  ok    %s\n", what);
    }
}

} // namespace

auto main() -> int
{
    std::printf("transform maths\n");

    {
        const Transform identity;
        check(identity.isIdentity(), "a default transform is the identity");
        check(nearlyEqual(identity.transformPoint(Vec3(1, 2, 3)), Vec3(1, 2, 3)), "the identity leaves points alone");
    }

    {
        Transform t;
        t.translation = Vec3(5, -2, 1);

        check(nearlyEqual(t.transformPoint(Vec3(1, 1, 1)), Vec3(6, -1, 2)), "translation moves points");
        check(nearlyEqual(t.transformNormal(Vec3(0, 1, 0)), Vec3(0, 1, 0)), "translation leaves normals alone");
    }

    {
        Transform t;
        t.rotationDegrees = Vec3(0, 90, 0);

        // Turning a quarter circle about Y sends +X to -Z.
        check(nearlyEqual(t.transformPoint(Vec3(1, 0, 0)), Vec3(0, 0, -1)), "rotation about Y moves points");
        check(nearlyEqual(t.transformNormal(Vec3(1, 0, 0)), Vec3(0, 0, -1)), "rotation about Y turns normals");
    }

    {
        Transform t;
        t.scale = Vec3(2, 2, 2);

        check(nearlyEqual(t.transformPoint(Vec3(1, 1, 1)), Vec3(2, 2, 2)), "uniform scale grows points");
        check(nearlyEqual(t.uniformScale(), 2.0f), "uniform scale reports its factor");
        check(!t.hasNonUniformScale(), "equal factors are not reported as non-uniform");
    }

    {
        Transform t;
        t.scale = Vec3(1, 4, 1);
        check(t.hasNonUniformScale(), "unequal factors are reported as non-uniform");
    }

    // The cases that matter. A normal transformed the naive way passes the first of these
    // and fails every one after it.
    {
        Transform t;
        t.scale = Vec3(2, 2, 2);
        checkNormalStaysPerpendicular(t, "normal survives uniform scale");
    }
    {
        Transform t;
        t.scale = Vec3(1.0f, 5.0f, 1.0f);
        checkNormalStaysPerpendicular(t, "normal survives being stretched along one axis");
    }
    {
        Transform t;
        t.scale = Vec3(4.0f, 0.25f, 2.0f);
        checkNormalStaysPerpendicular(t, "normal survives three different scale factors");
    }
    {
        Transform t;
        t.scale = Vec3(3.0f, 0.5f, 1.5f);
        t.rotationDegrees = Vec3(20.0f, -35.0f, 50.0f);
        t.translation = Vec3(-2.0f, 7.0f, 0.5f);
        checkNormalStaysPerpendicular(t, "normal survives scale, rotation and translation together");
    }

    if (failures != 0)
    {
        std::printf("\n%d check(s) failed\n", failures);
        return 1;
    }

    std::printf("\nall checks passed\n");
    return 0;
}
