#include "Scene.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "Scene/Light.h"
#include "Scene/SceneObject.h"
#include "Utils/ObjModel.h"

/**
 * returns the list of scene object that are present in the scene
 *
 * \return - the vector of scene object pointers in the scene
 */
auto Scene::getObjectList() -> std::vector<std::shared_ptr<SceneObject>>
{
    return m_ObjectList;
}

/**
 * add a scene object to the scene
 *
 * \param sceneObject - the pointer of the scene object to be added
 * \param materialName - the name of the registered material of the added scene
 * object
 */
void Scene::addObject(std::shared_ptr<SceneObject> sceneObject, std::string materialName)
{
    sceneObject->setMaterialName(materialName);
    m_ObjectList.push_back(sceneObject);
}

/**
 * adds multiple scene objects to the scene
 *
 * \param sceneObjectList - a list of scene object to be added
 * \param materialNmae - the name of the registered material of the added scene
 * objects
 */
void Scene::addObjects(std::vector<std::shared_ptr<SceneObject>> sceneObjectList, std::string materialName)
{
    for (std::shared_ptr<SceneObject> sceneObject : sceneObjectList)
    {
        addObject(sceneObject, materialName);
    }
}

/**
 * returns the list of lights in the scene
 *
 * \return - the list of light pointers in the scene
 */
auto Scene::getLightList() -> std::vector<std::shared_ptr<Light>>
{
    return m_LightList;
}

/**
 * adds a light to the scene
 *
 * \param light - the pointer to the light to be added to the scene
 */
void Scene::addLight(std::shared_ptr<Light> light)
{
    m_LightList.push_back(light);
}

/**
 * adds a material to the scene's material store to be used and reused
 * in the scene
 *
 * \param material - the pointer to the material to be registered
 */
void Scene::registerMaterial(Material material)
{
    m_MaterialStore[material.name] = material;
}

/**
 * returns an already registered material in the scene's material store
 *
 * \param materialName - the name of the desired material
 * \return - a pointer to the desired material
 */
auto Scene::getMaterial(std::string materialName) -> Material *
{
    auto it = m_MaterialStore.find(materialName);

    if (it == m_MaterialStore.end())
    {
        std::cout << "Material Not Found!" << std::endl;
        exit(EXIT_FAILURE);
    }
    else
    {
        return &m_MaterialStore.at(materialName);
    }
}

/**
 * creates a bounding volume heirarchy to accelerate ray scene intersections
 * TODO: finish function
 *
 */
void Scene::createAcceleratedStructure()
{
    if (m_ObjectList.size() <= 0)
    {
        std::cout << "Scene has no objects in it!" << std::endl;
    }
    else
    {
        m_AcceleratedStructure = std::make_shared<BVH>(m_ObjectList);
    }
}

Scene::Scene(std::string filePath)
{
    std::ifstream f(filePath);
    json data = json::parse(f);
    f.close();

    m_AmbientLighting = Vec3(data.at(m_keywords.ambientLighting).get<std::vector<float>>());
    m_Camera = Camera(Vec3(data.at(m_keywords.camera).at(m_keywords.cameraOrg).get<std::vector<float>>()),
                      Vec3(data.at(m_keywords.camera).at(m_keywords.cameraLookAt).get<std::vector<float>>()));

    json materials = data.at(m_keywords.materials);
    for (auto it = materials.begin(); it != materials.end(); ++it)
    {
        std::string name = it.key();
        json materialData = it.value();
        float ambient = materialData.at("ambient");
        float diffuse = materialData.at("diffuse");
        float specular = materialData.at("specular");
        float shininess = materialData.at("shininess");
        std::vector<float> color = materialData.at("color");
        Material material(name, color, ambient, diffuse, specular, shininess);
        registerMaterial(material);
    }

    std::vector<json> objectList = data.at(m_keywords.objects);
    for (json object : objectList)
    {
        std::string type = object.at("type");
        std::string materialId = object.at("material");
        if (type == "objModel")
        {
            std::string path = object.at("path");
            ObjModel objModel(path);
            addObjects(objModel.getSceneObjects(), materialId);
        }
        else if (type == "triangle")
        {
            Vec3 point0(object.at("point0").get<std::vector<float>>());
            Vec3 point1(object.at("point1").get<std::vector<float>>());
            Vec3 point2(object.at("point2").get<std::vector<float>>());
            addObject(std::make_shared<Triangle>(point0, point1, point2), materialId);
        }
        else if (type == "sphere")
        {
            Vec3 center(object.at("center").get<std::vector<float>>());
            float radius = object.at("radius");
            addObject(std::make_shared<Sphere>(center, radius), materialId);
        }
    }

    std::vector<json> lightList = data.at(m_keywords.lights);
    for (json light : lightList)
    {
        std::string type = light.at("type");
        if (type == "point")
        {
            Vec3 pos(light.at("pos").get<std::vector<float>>());
            Vec3 color(light.at("color").get<std::vector<float>>());
            float intensity = light.at("intensity");
            float radius = light.at("radius");
            PointLight pointLight(pos, color, intensity, radius);
            addLight(std::make_shared<Light>(pointLight));
        }
    }
}