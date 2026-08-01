#pragma once
#include <string>
#include <utility>

#include "Scene/Texture.h"
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

    std::string name;

    /** proportion of light reflected per channel; for metals, the tint of the reflection */
    Vec3 albedo{0.8f, 0.8f, 0.8f};

    /** radiance emitted by the surface. Any non-zero value makes this an area light */
    Vec3 emissive{};

    Type type = Type::Diffuse;

    /** 0 is a perfect mirror, 1 scatters reflections widely. Metals only */
    float roughness = 0.0f;

    /** index of refraction. Dielectrics only; 1.5 is typical glass */
    float ior = 1.5f;

    /** optional albedo texture, sampled with the surface's texture coordinates */
    Texture texture;

    Material() = default;

    explicit Material(std::string name) : name(std::move(name))
    {
    }

    /** true when this surface emits light and should be sampled as an emitter */
    auto isEmissive() const -> bool
    {
        return emissive.x > 0.0f || emissive.y > 0.0f || emissive.z > 0.0f;
    }

    /**
     * \brief The albedo at a point, taking any texture into account.
     *
     * \param texCoord Surface texture coordinates; only x and y are used.
     */
    auto albedoAt(const Vec3 &texCoord) const -> Vec3
    {
        if (!texture.isValid())
        {
            return albedo;
        }

        return albedo * texture.getTexel(texCoord.x, texCoord.y);
    }
};
