#include "ObjModel.h"
#include <iostream>
#include <exception>
#include <unistd.h>

ObjModel::ObjModel(std::string filePath) : material(Material("obj_material"))
{
	loadModel(filePath);
}

/**
 * loads an obj model as a list of triangles into the scene
 *
 * \param filePath - the file path to the model obj file
 * \return - true if the model was sucessfully loaded, false if there was an error
 */
bool ObjModel::loadModel(std::string filePath)
{
	std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
	std::vector<SceneObject> objList;

	std::ifstream objFile(filePath.c_str(), std::ios::in);

	if (objFile)
	{
		std::string line;
		while (std::getline(objFile, line))
		{
			std::vector<std::string> splitLine = splitString(line, std::string(" "));

			std::string lineType = splitLine[0];
			splitLine.erase(splitLine.begin());

			if (lineType == VERTEX_POS)
			{
				Vec3 vertex = parseVec3(splitLine);
				vertexList.push_back(vertex);
			}
			else if (lineType == VERTEX_TEX)
			{
				std::cout << "Texture coord" << std::endl;
			}
			else if (lineType == VERTEX_NORM)
			{
				Vec3 normal = parseVec3(splitLine);
				normalList.push_back(normal);
			}
			else if (lineType == FACE)
			{
				std::vector<FaceIndices> faceIndices = parseFace(splitLine);
				faceIndicesList.push_back(faceIndices);
			}
		}

		objFile.close();

		std::cout << "Model loaded sucessfully!" << std::endl;
		return true;
	}
	else
	{
		std::cerr << "Model load failed!" << std::endl;
		return false;
	}
}

/**
 * splits a string into a vector of substrings delimited by the delimiter parameter
 *
 * \param inputString - the string to be split
 * \param delimiter - the string that splits the input string
 */
std::vector<std::string> ObjModel::splitString(std::string &inputString, std::string delimiter)
{
	std::vector<std::string> splitString;

	size_t last = 0;
	size_t next = 0;

	std::string token;

	while ((next = inputString.find(delimiter, last)) != std::string::npos)
	{
		splitString.push_back(inputString.substr(last, next - last));
		last = next + delimiter.length();
	}

	splitString.push_back(inputString.substr(last, inputString.length()));

	return splitString;
}

/**
 * creates a vector Vec3 from a vector of vectors of strings of size 3
 * the strings must be able to be converted into a float
 *
 * \param vertexData - the vector of strings to be converted into floats
 * \param vertices - the vector of Vec3 to add the newly created Vec3s to
 */
Vec3 ObjModel::parseVec3(std::vector<std::string> &vertexData)
{
	if (vertexData.size() < 3)
	{
		std::cout << "Vec3 size less than 3!" << std::endl;
	}

	float x = std::stof(vertexData[0]);
	float y = std::stof(vertexData[1]);
	float z = std::stof(vertexData[2]);

	return Vec3(x, y, z);
}

/**
 * reads vector of vertices information and adds the necessary amount of traingle
 * to accurately describe the face
 *
 * face vertex string format: "vertexIndex/textureCoord/vertexNormal"
 *
 * \param faceIndices - vector of vertex information as a string
 */
std::vector<ObjModel::FaceIndices> ObjModel::parseFace(std::vector<std::string> &faceIndices)
{
	std::vector<FaceIndices> faces;

	// iterate through all vertices of the face
	for (std::string s : faceIndices)
	{
		// parse out vertex index, texture coord, & vertex normal
		std::vector<std::string> faceIndexSplit = splitString(s, std::string("/"));

		// face index
		FaceIndices face;

		// assign indices if available
		for (int i = 0; i < faceIndexSplit.size(); i++)
		{
			if (!faceIndexSplit[i].empty())
			{
				unsigned int index = std::stoi(faceIndexSplit[i]);

				switch (i)
				{
				case 0:
					face.position = index;
					face.positionSet = true;
					break;

				case 1:
					face.texture = index;
					face.textureSet = true;
					break;

				case 2:
					face.normal = index;
					face.normalSet = true;
					break;

				default:
					std::cout << "Unexpected face index data!" << std::endl;
					exit(EXIT_FAILURE);
					break;
				}
			}
		}

		faces.push_back(face);
	}

	return faces;
}

std::vector<std::shared_ptr<SceneObject>> ObjModel::getSceneObjects()
{
	std::vector<std::shared_ptr<SceneObject>> sceneObjectList;

	for (std::vector<ObjModel::FaceIndices> faceIndices : faceIndicesList)
	{
		if (faceIndices.size() > 3)
		{
			std::vector<Triangle> triList = triangulateFace(faceIndices);
			for (Triangle tri : triList)
			{
				sceneObjectList.push_back(std::make_shared<Triangle>(tri));
			}
		}
		else
		{
			FaceIndices fi0 = faceIndices[0];
			FaceIndices fi1 = faceIndices[1];
			FaceIndices fi2 = faceIndices[2];

			Triangle tri = Triangle(
				vertexList[fi0.position - 1],
				vertexList[fi1.position - 1],
				vertexList[fi2.position - 1]);

			sceneObjectList.push_back(std::make_shared<Triangle>(tri));
		}
	}

	return sceneObjectList;
}

std::vector<Triangle> ObjModel::triangulateFace(std::vector<ObjModel::FaceIndices> faceIndices)
{
	FaceIndices start = faceIndices[0];
	std::vector<Triangle> triList;

	for (unsigned int i = 1; i < faceIndices.size() - 1; i++)
	{
		triList.push_back(
			Triangle(
				vertexList[start.position - 1],
				vertexList[faceIndices[i].position - 1],
				vertexList[faceIndices[i + 1].position - 1]));
	}

	return triList;
}

Vec3 ObjModel::getCenterPoint()
{
	Vec3 sum = Vec3();
	for (Vec3 v : vertexList)
	{
		sum = sum + v;
	}

	Vec3 center = sum / vertexList.size();
	return center;
}
