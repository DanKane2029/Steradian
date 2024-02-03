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
    std::vector<std::shared_ptr<SceneObject>> getObjectList();
    void addObject(std::shared_ptr<SceneObject> sceneObject, std::string materialName);
    void addObjects(std::vector<std::shared_ptr<SceneObject>> sceneObjectList, std::string materialName);

    std::vector<std::shared_ptr<Light>> getLightList();
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

    Camera inline getCamera()
    {
        return m_Camera;
    }

    void createAcceleratedStructure();
    std::shared_ptr<BVH> getAccelerationStructure()
    {
        return m_AcceleratedStructure;
    };
};
