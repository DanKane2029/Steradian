#pragma once
#include "SceneObject.h"
#include "Light.h"
#include "Material.h"
#include "BVH.h"

#include <string>
#include <vector>
#include <unordered_map>

class Scene
{
public:
	std::vector<SceneObject*> GetObjectList();
	void AddObject(SceneObject* sceneObject, std::string materialName);
	
	std::vector<Light*> GetLightList();
	void AddLight(Light* light);

	void RegisterMaterial(Material* material);
	Material* GetMaterial(std::string materialName);

private:
	std::vector<SceneObject*> m_ObjectList;
	std::vector<Light*> m_LightList;
	std::unordered_map<std::string, Material*> m_MaterialStore;
};