#pragma once
#include <Scene.h>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>

namespace ModelLoader
{
	bool LoadModel(std::string filePath, std::string materialName, Scene& scene);
	void SplitString(std::string& inputString, std::string delimiter, std::vector<std::string>& splitString);
	void ParseVec3(std::vector<std::string>& vertexData, std::vector<Vec3>& vertices);
	void ParseFace(std::vector<std::string>& faceIndices, std::vector<SceneObject*>& triangleList, std::vector<Vec3>& vertices, std::vector<Vec3>& normals);
}