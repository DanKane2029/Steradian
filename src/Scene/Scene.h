#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "BVH.h"
#include "Light.h"
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
    std::vector<std::shared_ptr<Light>> m_LightList{};
    std::unordered_map<std::string, std::shared_ptr<Material>> m_MaterialStore{};

    std::shared_ptr<BVH> m_AcceleratedStructure{};
    Camera m_Camera;

  public:
    auto getObjectList() -> const std::vector<std::shared_ptr<SceneObject>> &;
    void addObject(std::shared_ptr<SceneObject> sceneObject, std::string materialName);
    void addObjects(std::vector<std::shared_ptr<SceneObject>> sceneObjectList, std::string materialName);

    auto getLightList() -> const std::vector<std::shared_ptr<Light>> &;
    void addLight(std::shared_ptr<Light> light);

    void registerMaterial(Material material);
    auto getMaterial(std::string materialName) -> std::shared_ptr<Material>;

    static Scene loadFromJson(std::string filePath);

    auto inline getAmbientLighting() -> Vec3
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
