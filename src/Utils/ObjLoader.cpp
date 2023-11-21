#include "ObjLoader.h"
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

	std::ifstream objFile(filePath.c_str());

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
				FaceIndices faceIndices = parseFace(splitLine);
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
ObjModel::FaceIndices ObjModel::parseFace(std::vector<std::string> &faceIndices)
{
	FaceIndices face;

	// iterate through all vertices of the face
	for (std::string s : faceIndices)
	{
		// parse out vertex index, texture coord, & vertex normal
		std::vector<std::string> faceIndexSplit = splitString(s, std::string("/"));

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
	}

	return face;
}
