#include "Scene.h"

std::vector<SceneObject*> Scene::GetObjectList()
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
	return m_MaterialStore[materialName];
}
