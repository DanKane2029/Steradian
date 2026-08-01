#include "Scene.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
using json = nlohmann::json;

#include "Scene/SceneObject.h"
#include "Utils/ObjModel.h"

namespace
{

/**
 * reads an optional float from a json object, falling back to a default
 *
 * Several scene files in res/scenes predate later material fields. Treating those
 * fields as optional keeps older scenes loadable instead of throwing on parse.
 */
auto optionalFloat(const json &data, const char *key, float fallback) -> float
{
    if (!data.contains(key) || data.at(key).is_null())
    {
        return fallback;
    }

    return data.at(key).get<float>();
}

/**
 * resolves an asset path referenced by a scene file
 *
 * Relative paths are resolved against the directory containing the scene file, so a
 * scene and its models/textures can be moved or checked out anywhere. Absolute paths
 * are honoured as-is for backwards compatibility.
 */
auto resolveAssetPath(const std::string &assetPath, const std::filesystem::path &sceneDir) -> std::string
{
    std::filesystem::path path(assetPath);

    if (!path.is_absolute())
    {
        path = sceneDir / path;
    }

    if (!std::filesystem::exists(path))
    {
        throw std::runtime_error("Scene references a file that does not exist: " + path.string());
    }

    return path.string();
}

/**
 * reads an object's placement from a scene file
 *
 * All three parts are optional, and they are applied in the order scale, rotate,
 * translate, which is what makes a rotation turn an object in place rather than sweeping
 * it around the origin.
 */
auto parseTransform(const json &object) -> Transform
{
    Transform transform;

    if (object.contains("translate"))
    {
        transform.translation = Vec3(object.at("translate").get<std::vector<float>>());
    }

    if (object.contains("rotate"))
    {
        transform.rotationDegrees = Vec3(object.at("rotate").get<std::vector<float>>());
    }

    if (object.contains("scale"))
    {
        const json &scale = object.at("scale");

        // A single number is the common case and reads better than repeating it three
        // times, so both forms are accepted.
        transform.scale = scale.is_number() ? Vec3(scale.get<float>()) : Vec3(scale.get<std::vector<float>>());
    }

    return transform;
}

} // namespace

/**
 * returns the list of scene object that are present in the scene
 *
 * \return - the vector of scene object pointers in the scene
 */
auto Scene::getObjectList() -> const std::vector<std::shared_ptr<SceneObject>> &
{
    return m_ObjectList;
}

/**
 * adds a scene object to the scene
 *
 * \param sceneObject - the object to add
 * \param materialIndex - index of its material in the scene's material array
 */
void Scene::addObject(std::shared_ptr<SceneObject> sceneObject, uint32_t materialIndex)
{
    sceneObject->setMaterialIndex(materialIndex);
    m_ObjectList.push_back(sceneObject);
}

/**
 * adds multiple scene objects sharing one material
 */
void Scene::addObjects(const std::vector<std::shared_ptr<SceneObject>> &sceneObjectList, uint32_t materialIndex)
{
    for (const std::shared_ptr<SceneObject> &sceneObject : sceneObjectList)
    {
        addObject(sceneObject, materialIndex);
    }
}

/**
 * registers a material and returns the index used to refer to it
 */
auto Scene::registerMaterial(const Material &material) -> uint32_t
{
    const auto index = static_cast<uint32_t>(m_Materials.size());

    m_Materials.push_back(material);
    m_MaterialIndices[material.name] = index;

    return index;
}

/**
 * resolves a material name to its index
 *
 * Used only while loading a scene. A missing material is a scene authoring error, so it
 * reports the name rather than failing anonymously.
 */
auto Scene::materialIndexByName(const std::string &name) const -> uint32_t
{
    const auto it = m_MaterialIndices.find(name);

    if (it == m_MaterialIndices.end())
    {
        throw std::runtime_error("Scene refers to unknown material '" + name + "'");
    }

    return it->second;
}

/**
 * creates a bounding volume heirarchy to accelerate ray scene intersections
 */
void Scene::createAcceleratedStructure(unsigned int objectsInLeaf)
{
    if (m_ObjectList.size() <= 0)
    {
        std::cout << "Scene has no objects in it!" << std::endl;
    }
    else
    {
        // Honour the configured leaf size, which was parsed and then ignored.
        m_AcceleratedStructure = std::make_shared<BVH>(m_ObjectList, objectsInLeaf);
    }
}

Scene::Scene(std::string filePath)
{
    std::ifstream f(filePath);
    if (!f)
    {
        throw std::runtime_error("Could not open scene file: " + filePath);
    }

    json data = json::parse(f);
    f.close();

    // Asset paths inside the scene are resolved relative to the scene file itself.
    const std::filesystem::path sceneDir = std::filesystem::absolute(filePath).parent_path();

    m_AmbientLighting = Vec3(data.at(m_keywords.ambientLighting).get<std::vector<float>>());

    const json &cameraData = data.at(m_keywords.camera);

    // Field of view is given in degrees, which is what a person authoring a scene file
    // expects to write. Optional so existing scenes keep working.
    const float fovDegrees = optionalFloat(cameraData, "fov", 60.0f);
    const float fovY = fovDegrees * static_cast<float>(M_PI) / 180.0f;

    Vec3 worldUp(0.0f, 1.0f, 0.0f);
    if (cameraData.contains("up"))
    {
        worldUp = Vec3(cameraData.at("up").get<std::vector<float>>());
    }

    m_Camera = Camera(Vec3(cameraData.at(m_keywords.cameraOrg).get<std::vector<float>>()),
                      Vec3(cameraData.at(m_keywords.cameraLookAt).get<std::vector<float>>()), fovY, worldUp);

    json materials = data.at(m_keywords.materials);
    for (auto it = materials.begin(); it != materials.end(); ++it)
    {
        const std::string name = it.key();
        const json &materialData = it.value();

        Material material(name);

        // New-style fields describe scattering directly. Older scenes describe
        // Blinn-Phong shading instead, so those fields are mapped onto the closest
        // equivalent rather than being rejected.
        if (materialData.contains("albedo"))
        {
            material.albedo = Vec3(materialData.at("albedo").get<std::vector<float>>());
        }
        else if (materialData.contains("diffuse"))
        {
            material.albedo = Vec3(materialData.at("diffuse").get<std::vector<float>>());
        }

        if (materialData.contains("absorption"))
        {
            material.absorption = Vec3(materialData.at("absorption").get<std::vector<float>>());
        }

        if (materialData.contains("emissive"))
        {
            material.emissive = Vec3(materialData.at("emissive").get<std::vector<float>>());
        }

        material.roughness = optionalFloat(materialData, "roughness", 0.0f);
        material.ior = optionalFloat(materialData, "refraction", 1.5f);
        material.ior = optionalFloat(materialData, "ior", material.ior);

        if (materialData.contains("type"))
        {
            const std::string typeName = materialData.at("type");

            if (typeName == "metal")
            {
                material.type = Material::Type::Metal;
            }
            else if (typeName == "dielectric" || typeName == "glass")
            {
                material.type = Material::Type::Dielectric;
            }
            else
            {
                material.type = Material::Type::Diffuse;
            }
        }
        else
        {
            // Infer from the old fields: `transparency` meant glass, and `reflection`
            // meant a mirror-like surface.
            const float transparency = optionalFloat(materialData, "transparency", 0.0f);
            const float reflection = optionalFloat(materialData, "reflection", 0.0f);

            if (transparency > 0.0f)
            {
                material.type = Material::Type::Dielectric;
            }
            else if (reflection > 0.5f)
            {
                material.type = Material::Type::Metal;
                material.roughness = 1.0f - reflection;
            }
        }

        if (materialData.contains("texture"))
        {
            material.texture = Texture(resolveAssetPath(materialData.at("texture"), sceneDir));
        }

        registerMaterial(material);
    }

    std::vector<json> objectList = data.at(m_keywords.objects);
    for (json object : objectList)
    {
        std::string type = object.at("type");
        const uint32_t materialId = materialIndexByName(object.at("material"));
        const Transform transform = parseTransform(object);
        if (type == "objModel")
        {
            std::string path = resolveAssetPath(object.at("path"), sceneDir);
            ObjModel objModel(path);

            std::vector<std::shared_ptr<SceneObject>> triangles = objModel.getSceneObjects();
            for (const std::shared_ptr<SceneObject> &triangle : triangles)
            {
                triangle->applyTransform(transform);
            }

            addObjects(triangles, materialId);
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

                auto triangle = std::make_shared<Triangle>(point0, normal0, point1, normal1, point2, normal2);
                triangle->applyTransform(transform);
                addObject(triangle, materialId);
            }
            else
            {
                auto triangle = std::make_shared<Triangle>(point0, point1, point2);
                triangle->applyTransform(transform);
                addObject(triangle, materialId);
            }
        }
        else if (type == "sphere")
        {
            Vec3 center(object.at("center").get<std::vector<float>>());
            float radius = object.at("radius");

            auto sphere = std::make_shared<Sphere>(center, radius);
            sphere->applyTransform(transform);

            // The emitter is registered from the sphere's final position, so a
            // transformed light is sampled where it actually ends up.
            center = sphere->getCenterPoint();
            radius *= transform.uniformScale();

            // An emissive sphere is an area light. Registering it lets direct lighting
            // sample it, which is what keeps small bright sources from being extremely
            // noisy.
            const Material &material = m_Materials[materialId];
            if (material.isEmissive() && radius > 0.0f)
            {
                sphere->setEmitterIndex(static_cast<int32_t>(m_Emitters.size()));
                m_Emitters.push_back(Emitter{center, radius, material.emissive});
            }

            addObject(sphere, materialId);
        }
    }

    // Point lights become emissive spheres.
    //
    // A path tracer gathers light by intersecting geometry, and a point has no area for a
    // ray to land on, so a true point light can never be seen by a scattered ray. The
    // scene format already gave lights a radius (it drove soft shadows), which makes the
    // conversion natural: the light becomes a sphere that emits.
    //
    // Intensity was defined against an inverse-square falloff applied at the shading
    // point. Emitted radiance is that power spread over the sphere's surface, so the
    // conversion divides by the area to keep authored scenes looking about right.
    if (data.contains(m_keywords.lights))
    {
        for (const json &light : data.at(m_keywords.lights))
        {
            const std::string type = light.at("type");
            if (type != "point")
            {
                continue;
            }

            const Vec3 pos(light.at("pos").get<std::vector<float>>());
            const Vec3 color(light.at("color").get<std::vector<float>>());
            const float intensity = optionalFloat(light, "intensity", 1.0f);
            const float radius = std::max(optionalFloat(light, "radius", 0.1f), 1e-3f);

            const float area = 4.0f * static_cast<float>(M_PI) * radius * radius;
            const Vec3 emission = color * (intensity / area);

            Material lightMaterial("__light_" + std::to_string(m_Emitters.size()));
            lightMaterial.emissive = emission;
            lightMaterial.albedo = Vec3(0.0f, 0.0f, 0.0f);

            const uint32_t materialIndex = registerMaterial(lightMaterial);

            auto sphere = std::make_shared<Sphere>(pos, radius);
            sphere->setEmitterIndex(static_cast<int32_t>(m_Emitters.size()));
            m_Emitters.push_back(Emitter{pos, radius, emission});

            addObject(sphere, materialIndex);
        }
    }
}
