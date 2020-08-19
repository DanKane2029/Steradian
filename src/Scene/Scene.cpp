#include "Scene.h"

std::vector<SceneObject*> Scene::GetObjectList()
{
	return m_ObjectList;
}

void Scene::AddObject(SceneObject* sceneObject)
{
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
