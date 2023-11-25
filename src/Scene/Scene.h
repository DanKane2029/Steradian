#pragma once
#include "SceneObject.h"
#include "Light.h"
#include "Material.h"
#include "BVH.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

/**
 * a collection of primitive scene objects, lights, and materials 
 * that describe a scene for the ray tracer to render
 */
class Scene
{
public:
	std::vector<std::shared_ptr<SceneObject>> GetObjectList();
	void AddObject(std::shared_ptr<SceneObject> sceneObject, std::string materialName);
	void AddObjects(std::vector<std::shared_ptr<SceneObject>> sceneObjectList, std::string materialName);
	
	std::vector<Light*> GetLightList();
	void AddLight(Light* light);

	void RegisterMaterial(Material* material);
	Material* GetMaterial(std::string materialName);

	Vec3 inline GetAmbientLighting() { return m_AmbientLighting; };
	void inline SetAmbientLighting(Vec3 al) { m_AmbientLighting = al; };

	void CreateAcceleratedStructure();

private:
	Vec3 m_AmbientLighting;

	std::vector<std::shared_ptr<SceneObject>> m_ObjectList;
	std::vector<Light*> m_LightList;
	std::unordered_map<std::string, Material*> m_MaterialStore;
	BVH* m_AcceleratedStructure;
};
