#include "Scene.h"

#include <iostream>

/**
 * returns the list of scene object that are present in the scene
 * 
 * \return - the vector of scene object pointers in the scene
 */
std::vector<std::shared_ptr<SceneObject>> Scene::GetObjectList()
{
	return m_ObjectList;
}

/**
 * add a scene object to the scene
 * 
 * \param sceneObject - the pointer of the scene object to be added
 * \param materialName - the name of the registered material of the added scene object
 */
void Scene::AddObject(std::shared_ptr<SceneObject> sceneObject, std::string materialName)
{
	sceneObject->SetMaterialName(materialName);
	m_ObjectList.push_back(sceneObject);
}

/**
 * adds multiple scene objects to the scene
 * 
 * \param sceneObjectList - a list of scene object to be added
 * \param materialNmae - the name of the registered material of the added scene objects
*/
void Scene::AddObjects(std::vector<std::shared_ptr<SceneObject>> sceneObjectList, std::string materialName)
{
	for (std::shared_ptr<SceneObject> sceneObject : sceneObjectList)
	{
		AddObject(sceneObject, materialName);
	}
}

/**
 * returns the list of lights in the scene
 * 
 * \return - the list of light pointers in the scene
 */
std::vector<Light*> Scene::GetLightList()
{
	return m_LightList;
}

/**
 * adds a light to the scene
 * 
 * \param light - the pointer to the light to be added to the scene
 */
void Scene::AddLight(Light* light)
{
	m_LightList.push_back(light);
}

/**
 * adds a material to the scene's material store to be used and reused 
 * in the scene
 * 
 * \param material - the pointer to the material to be registered
 */
void Scene::RegisterMaterial(Material* material)
{
	m_MaterialStore[material->name] = material;
}

/**
 * returns an already registered material in the scene's material store
 * 
 * \param materialName - the name of the desired material
 * \return - a pointer to the desired material 
 */
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

/**
 * creates a bounding volume heirarchy to accelerate ray scene intersections
 * TODO: finish function
 * 
 */
void Scene::CreateAcceleratedStructure()
{
	m_AcceleratedStructure = new BVH(m_ObjectList);
}
