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
    m_MaterialStore[material.name] = std::make_shared<Material>(material);
}

/**
 * returns an already registered material in the scene's material store
 *
 * \param materialName - the name of the desired material
 * \return - a pointer to the desired material
 */
auto Scene::getMaterial(std::string materialName) -> std::shared_ptr<Material>
{
    auto it = m_MaterialStore.find(materialName);

    if (it == m_MaterialStore.end())
    {
        std::cout << "Material Not Found!" << std::endl;
        exit(EXIT_FAILURE);
    }
    else
    {
        return m_MaterialStore.at(materialName);
    }
}

/**
 * creates a bounding volume heirarchy to accelerate ray scene intersections
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

        Vec3 ambient = materialData.at("ambient").get<std::vector<float>>();
        Vec3 diffuse = materialData.at("diffuse").get<std::vector<float>>();
        Vec3 specular = materialData.at("specular").get<std::vector<float>>();

        float specularExponent = materialData.at("specularExponent");
        float transparency = materialData.at("transparency");
        float refraction = materialData.at("refraction");
        float reflection = materialData.at("reflection");

        Material material(name, ambient, diffuse, specular, specularExponent, transparency, refraction, reflection);
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

            if (object.contains("normal0") && object.contains("normal1") && object.contains("normal2"))
            {
                Vec3 normal0(object.at("normal0").get<std::vector<float>>());
                Vec3 normal1(object.at("normal1").get<std::vector<float>>());
                Vec3 normal2(object.at("normal2").get<std::vector<float>>());

                addObject(std::make_shared<Triangle>(point0, normal0, point1, normal1, point2, normal2), materialId);
            }
            else
            {
                addObject(std::make_shared<Triangle>(point0, point1, point2), materialId);
            }
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