#include "Scene.h"

#include <iostream>

std::vector<SceneObject*>& Scene::GetObjectList()
{
	return m_ObjectList;
}

void Scene::AddObject(SceneObject* sceneObject, std::string materialName)
{
	sceneObject->SetMaterialName(materialName);
	m_ObjectList.push_back(sceneObject);
}

std::vector<Light*> Scene::GetLightList()
{
	return m_LightList;
}

void Scene::AddLight(Light* light)
{
	m_LightList.push_back(light);
}

void Scene::RegisterMaterial(Material* material)
{
	m_MaterialStore[material->name] = material;
}

Material* Scene::GetMaterial(std::string materialName)
{
	auto it = m_MaterialStore.find(materialName);

	if (it == m_MaterialStore.end())
	{
		std::cout << "Material Not Found!" << std::endl;
		exit(EXIT_FAILURE);
	} else
	{
		return m_MaterialStore.at(materialName);
	}
}
