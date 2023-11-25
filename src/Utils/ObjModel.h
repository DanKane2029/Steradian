#pragma once
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Scene/Material.h"
#include "Utils/Vec3.h"

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <memory>

class ObjModel
{
private:
	// .obj keywords and line types
	const std::string VERTEX_POS = "v";
	const std::string VERTEX_TEX = "vt";
	const std::string VERTEX_NORM = "vn";
	const std::string VERTEX_PARAM = "vp";
	const std::string FACE = "f";
	const std::string LINE = "l";
	const std::string OBJECT = "o";
	const std::string GROUP = "g";
	const std::string MATERIAL = "mtllib";
	const std::string USE_MATERIAL = "usemtl";
	const std::string NEW_MATERIAL = "newmtl";

	std::vector<Vec3> vertexList;
	std::vector<Vec3> normalList;
	Material material;

	struct FaceIndices
	{
		unsigned int position;
		bool positionSet = false;

		unsigned int texture;
		bool textureSet = false;

		unsigned int normal;
		bool normalSet = false;
	};
	std::vector<std::vector<FaceIndices>> faceIndicesList;

public:
	ObjModel(std::string filePath);
	std::vector<std::shared_ptr<SceneObject>> getSceneObjects();

private:
	bool loadModel(std::string filePath);
	std::vector<std::string> splitString(std::string &inputString, std::string delimiter);
	Vec3 parseVec3(std::vector<std::string> &vertexData);
	std::vector<FaceIndices> parseFace(std::vector<std::string> &faceIndices);
};
