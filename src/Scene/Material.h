#pragma once
#include <cmath>
#include <cstdint>
#include <type_traits>

#include "Utils/Vec3.h"

/**
 * \brief How light interacts with a surface.
 *
 * This replaces the previous Blinn-Phong parameter set (ambient/diffuse/specular plus a
 * specular exponent). Those describe how a surface should be *shaded* under a direct
 * lighting model; a path tracer instead needs to know how the surface *scatters* light,
 * so that energy arriving from any direction is handled the same way.
 *
 * Scenes written against the old fields still load: see Scene.cpp, which maps them onto
 * these.
 *
 * Deliberately a plain aggregate of numbers: no name, no owned texture, nothing that
 * allocates. A material is read once per shade from an array indexed by the primitive
 * that was hit, and it has to be something an array of which can be copied to a device
 * as bytes. The static assertion below is there so that stays true -- adding a
 * std::string or a container here would break the build rather than the port.
 *
 * Its name lives in the scene's name-to-index map, which is load-time only, and its
 * texture in the scene's texture array; see Scene::albedoAt.
 */
struct Material
{
    /** \brief The kind of scattering this surface performs. */
    enum class Type
    {
        Diffuse,   ///< Lambertian reflection
        Metal,     ///< specular reflection tinted by albedo, blurred by roughness
        Dielectric ///< glass: refracts, with Fresnel-weighted reflection
    };

    /** proportion of light reflected per channel; for metals, the tint of the reflection */
    Vec3 albedo{0.8f, 0.8f, 0.8f};

    /** radiance emitted by the surface. Any non-zero value makes this an area light */
    Vec3 emissive{};

    Type type = Type::Diffuse;

    /** 0 is a perfect mirror, 1 scatters reflections widely. Metals only */
    float roughness = 0.0f;

    /** index of refraction. Dielectrics only; 1.5 is typical glass */
    float ior = 1.5f;

    /**
     * \brief How strongly the interior of a dielectric absorbs light, per channel.
     *
     * Absorption happens along the path *through* a material, not at its surface, so what
     * it produces depends on how far light travelled inside: a thick part of a glass
     * object is darker and more saturated than a thin one, and the colour deepens towards
     * the middle. A surface tint cannot reproduce that, because it has no notion of
     * distance.
     *
     * These are extinction coefficients, so larger absorbs more, and the channel that
     * survives is the colour you see. Absorbing red and a little green leaves the blue
     * green cast of thick window glass. Zero, the default, is perfectly clear.
     */
    Vec3 absorption{};

    /** index into the scene's texture array, or -1 for an untextured material */
    int32_t textureIndex = -1;

    /**
     * \brief The second colour of a procedural checker, and the size of one square.
     *
     * Keyed on where a point is in the world rather than on texture coordinates, because
     * the surfaces that most want a checker are the large flat ones written directly into
     * a scene as a pair of triangles, and those carry no useful texture coordinates at all.
     * A world-space checker also stays the same size across a floor no matter how the
     * geometry underneath it happens to be divided up.
     *
     * A scale of zero means no checker, and the plain albedo is used.
     */
    Vec3 checkerAlbedo{};
    float checkerScale = 0.0f;

    /** true when this material paints a checker rather than a flat colour */
    auto hasChecker() const -> bool
    {
        return checkerScale > 0.0f;
    }

    /** true when this surface emits light and should be sampled as an emitter */
    auto isEmissive() const -> bool
    {
        return emissive.x > 0.0f || emissive.y > 0.0f || emissive.z > 0.0f;
    }

    /**
     * \brief The surface colour at a point, before any texture is applied.
     *
     * Texturing is applied by Scene::albedoAt, which is where the texture array lives.
     * Keeping it out of here is what lets this type stay free of anything that owns
     * memory.
     *
     * \param position Where the point is in the world, for the procedural checker.
     */
    auto baseAlbedo(const Vec3 &position) const -> Vec3
    {
        return hasChecker() ? checkerAt(position) : albedo;
    }

  private:
    /** \brief Which of the two checker colours covers a point. */
    auto checkerAt(const Vec3 &position) const -> Vec3
    {
        const float inverseScale = 1.0f / checkerScale;

        // The half-square offset matters more than it looks. Without it the cell boundaries
        // sit at whole multiples of the square size, which is exactly where scenes tend to
        // put their floors: a floor lying on y = 0 would have every hit land on a boundary,
        // and whether floor() rounded to one side or the other would come down to whether
        // the intersection returned +0 or a value a hair below it. That decides the colour,
        // so the floor would come out speckled rather than checkered. Offsetting by half a
        // square puts the common cases in the middle of a cell instead of on its edge.
        const auto cell = [inverseScale](float v) { return static_cast<int>(std::floor((v * inverseScale) + 0.5f)); };

        const int parity = (cell(position.x) + cell(position.y) + cell(position.z)) & 1;

        return parity == 0 ? albedo : checkerAlbedo;
    }
};

// The whole point of the type. If this ever fails, something that owns memory has been
// added to a material, and an array of them can no longer be handed to a device.
static_assert(std::is_trivially_copyable_v<Material>, "Material must stay copyable as bytes");
