#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "BVH.h"
#include "Geometry.h"
#include "Material.h"
#include "RayTracer/Camera.h"
#include "Texture.h"

/**
 * a collection of primitive scene objects, lights, and materials
 * that describe a scene for the ray tracer to render
 */
class Scene
{
  public:
    Scene(std::string filePath);

  public:
    /**
     * \brief A light that direct lighting samples explicitly.
     *
     * Spherical, because that is the shape the scene format already describes and it is
     * cheap to sample uniformly. Emissive geometry of other shapes still lights the scene
     * through ordinary path tracing, just with more noise.
     */
    struct Emitter
    {
        Vec3 center;
        float radius = 0.0f;
        Vec3 emission;
    };

  private:
    struct Keywords
    {
        std::string ambientLighting = "ambientLighting";
        std::string camera = "camera";
        std::string cameraOrg = "org";
        std::string cameraLookAt = "lookAt";
        std::string materials = "materials";
        std::string objects = "objects";
        std::string lights = "lights";
        std::string type = "type";
    };
    const Keywords m_keywords;

    Vec3 m_AmbientLighting;

    Geometry m_Geometry{};

    // Materials live in one array and are referenced by index. The name map is used only
    // while loading; nothing looks a material up by string once rendering starts.
    std::vector<Material> m_Materials{};
    std::unordered_map<std::string, uint32_t> m_MaterialIndices{};

    // Textures are owned here rather than by the materials that use them, so a material
    // stays a plain block of numbers. Two materials naming the same file share one.
    std::vector<Texture> m_Textures{};
    std::unordered_map<std::string, int32_t> m_TextureIndices{};

    std::vector<Emitter> m_Emitters{};

    std::shared_ptr<BVH> m_AcceleratedStructure{};
    Camera m_Camera;

  public:
    /** the scene's primitives, in the flat arrays the renderer traverses */
    auto getGeometry() const -> const Geometry &
    {
        return m_Geometry;
    }

    /**
     * \brief Registers a material and returns the index it can be referenced by.
     *
     * The name is kept here rather than on the material itself; nothing looks a material
     * up by string once rendering starts.
     */
    auto registerMaterial(const Material &material, const std::string &name) -> uint32_t;

    /** loads a texture, or returns the index of one already loaded from the same file */
    auto registerTexture(const std::string &path) -> int32_t;

    /**
     * \brief The surface colour of a material at a point, texture included.
     *
     * Lives here because the textures do. The material contributes the flat colour or
     * the procedural checker; this applies the image on top of it.
     *
     * \param texCoord Surface texture coordinates; only x and y are used.
     * \param position Where the point is in the world, for the procedural checker.
     */
    auto albedoAt(const Material &material, const Vec3 &texCoord, const Vec3 &position) const -> Vec3
    {
        const Vec3 base = material.baseAlbedo(position);

        if (material.textureIndex < 0)
        {
            return base;
        }

        return base * m_Textures[static_cast<size_t>(material.textureIndex)].getTexel(texCoord.x, texCoord.y);
    }

    /** looks a material index up by name; load-time only */
    auto materialIndexByName(const std::string &name) const -> uint32_t;

    auto getMaterialByIndex(uint32_t index) const -> const Material &
    {
        return m_Materials[index];
    }

    /** the emitters direct lighting samples explicitly */
    auto getEmitters() const -> const std::vector<Emitter> &
    {
        return m_Emitters;
    }

    /**
     * radiance arriving from every direction where no geometry is hit
     *
     * Under the path integrator this is a uniform environment light rather than a
     * constant added to every surface: it is occluded by geometry and bounces like any
     * other light.
     */
    auto inline getAmbientLighting() const -> Vec3
    {
        return m_AmbientLighting;
    };

    void inline setAmbientLighting(Vec3 al)
    {
        m_AmbientLighting = al;
    };

    // Returned by reference: this is read once per light per shade, and copying the
    // camera (and, before, the whole light vector) on every call was pure overhead.
    auto inline getCamera() const -> const Camera &
    {
        return m_Camera;
    }

    /** replaces the camera, for interactive navigation */
    void inline setCamera(const Camera &camera)
    {
        m_Camera = camera;
    }

    /**
     * builds the acceleration structure over the scene's objects
     *
     * \param objectsInLeaf Maximum primitives per leaf node.
     *
     *        Small is better here, and by more than it looks. Every leaf a ray reaches
     *        costs a test against each primitive inside it, so a generous leaf multiplies
     *        the work of every visit. Measured on the Stanford bunny, going from 25 down
     *        to 2 cut primitive tests per ray from 1343 to 108 and render time by nearly
     *        three times, with no measurable cost in build time or memory.
     */
    void createAcceleratedStructure(unsigned int objectsInLeaf = 2);
    auto getAccelerationStructure() const -> const BVH *
    {
        return m_AcceleratedStructure.get();
    };
};
