#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "BVH.h"
#include "Material.h"
#include "RayTracer/Camera.h"
#include "SceneObject.h"

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

    std::vector<std::shared_ptr<SceneObject>> m_ObjectList{};

    // Materials live in one array and are referenced by index. The name map is used only
    // while loading; nothing looks a material up by string once rendering starts.
    std::vector<Material> m_Materials{};
    std::unordered_map<std::string, uint32_t> m_MaterialIndices{};

    std::vector<Emitter> m_Emitters{};

    std::shared_ptr<BVH> m_AcceleratedStructure{};
    Camera m_Camera;

  public:
    auto getObjectList() -> const std::vector<std::shared_ptr<SceneObject>> &;
    void addObject(std::shared_ptr<SceneObject> sceneObject, uint32_t materialIndex);
    void addObjects(const std::vector<std::shared_ptr<SceneObject>> &sceneObjectList, uint32_t materialIndex);

    /** registers a material and returns the index it can be referenced by */
    auto registerMaterial(const Material &material) -> uint32_t;

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

    /**
     * builds the acceleration structure over the scene's objects
     *
     * \param objectsInLeaf Maximum primitives per leaf node. Comes from the config,
     *        which previously parsed the value and then discarded it.
     */
    void createAcceleratedStructure(unsigned int objectsInLeaf = 10);
    auto getAccelerationStructure() const -> const BVH *
    {
        return m_AcceleratedStructure.get();
    };
};
