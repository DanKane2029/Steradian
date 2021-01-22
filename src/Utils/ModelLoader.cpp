#include "ModelLoader.h"

/**
 * loads an obj model as a list of triangles into the scene
 * 
 * \param filePath - the file path to the model obj file
 * \param materialName - the name of the registered material the model uses
 * \param scene - the scene to add the model to
 * \return - true if the model was sucessfully loaded, false if there was an error
 */
bool ModelLoader::LoadModel(std::string filePath, std::string materialName, Scene& scene)
{
	std::cout << "Loading Model..." << std::endl;

	std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
	std::vector<Vec3> tempVertices;
	std::vector<Vec3> tempNormals;
	std::vector<SceneObject*>& objList = scene.GetObjectList();

	std::fstream objFile;
	objFile.open(filePath.c_str(), std::ios::in);
	if (objFile.is_open())
	{
		std::string line;
		while (std::getline(objFile, line))
		{
			std::vector<std::string> splitLine;
			SplitString(line, std::string(" "), splitLine);

			std::string first = splitLine[0];
			splitLine.erase(splitLine.begin());

			if (first == "v")
			{
				ParseVec3(splitLine, tempVertices);

			} else if (first == "vt")
			{
				std::cout << "Texture coord" << std::endl;

			} else if (first == "vn")
			{
				ParseVec3(splitLine, tempNormals);

			} else if (first == "f")
			{
				ParseFace(splitLine, objList, tempVertices, tempNormals);
			}
		}

		objFile.close();

		for (SceneObject* f : objList)
		{
			f->SetMaterialName(materialName);
		}

		std::cout << "Model loaded sucessfully!" << std::endl;
		return true;

	} else
	{
		std::cout << "Model failed to load!" << std::endl;
		return false;
	}
}

/**
 * splits a string into a vector of substrings delimited by the delimiter parameter
 * 
 * \param inputString - the string to be split
 * \param delimiter - the string that splits the input string
 * \param splitString - the vector of substrings that were extracted from the input string
 */
void ModelLoader::SplitString(std::string& inputString, std::string delimiter, std::vector<std::string>& splitString)
{
	size_t last = 0;
	size_t next = 0;

	std::string token;

	while ((next = inputString.find(delimiter, last)) != std::string::npos)
	{
		splitString.push_back(inputString.substr(last, next - last));
		last = next + delimiter.length();
	}

	splitString.push_back(inputString.substr(last, inputString.length()));
}

/**
 * creates a vector Vec3 from a vector of vectors of strings of size 3
 * the strings must be able to be converted into a float
 * 
 * \param vertexData - the vector of strings to be converted into floats
 * \param vertices - the vector of Vec3 to add the newly created Vec3s to
 */
void ModelLoader::ParseVec3(std::vector<std::string>& vertexData, std::vector<Vec3>& vertices)
{
	if (vertexData.size() < 3)
	{
		std::cout << "Vec3 size less than 3!" << std::endl;
	}

	float x = std::stof(vertexData[0]);
	float y = std::stof(vertexData[1]);
	float z = std::stof(vertexData[2]);

	vertices.push_back(Vec3(x, y, z));
}

/**
 * reads vector of vertices information and adds the necessary amount of traingle
 * to accurately describe the face
 * 
 * face vertex string format: "vertexIndex/textureCoord/vertexNormal"
 * 
 * \param faceIndices - vector of vertex information as a string
 * \param objectList - the object list to add the triangles to
 * \param vertices - the list of all vertex positions in the model
 * \param normals - the list of all vertex normals in the model
 */
void ModelLoader::ParseFace(std::vector<std::string>& faceIndices, std::vector<SceneObject*>& objectList,
	std::vector<Vec3>& vertices, std::vector<Vec3>& normals)
{
	std::vector<unsigned int> faceVertexIndices;

	// iterate through all vertices of the face
	for (std::string s : faceIndices)
	{
		// parse out vertex index, texture coord, & vertex normal
		std::vector<std::string> faceIndexSplit;
		SplitString(s, std::string("/"), faceIndexSplit);

		int vertexIndex = -1;
		int uvCoordIndex = -1;
		int normalIndex = -1;

		// assign indices if available
		for (int i = 0; i < faceIndexSplit.size(); i++)
		{
			if (!faceIndexSplit[i].empty())
			{
				unsigned int index = std::stoi(faceIndexSplit[i]);

				switch (i)
				{
					case 0:
						vertexIndex = index;
						break;

					case 1:
						uvCoordIndex = index;
						break;

					case 2:
						normalIndex = index;
						break;

					default:
						std::cout << "Unexpected face index data!" << std::endl;
						exit(EXIT_FAILURE);
						break;
				}
			}
		}

		if (vertexIndex != -1)
		{
			faceVertexIndices.push_back(vertexIndex - 1);
		}

		//TODO: use normal and texture coord data for more detailed models
	}

	if (faceVertexIndices.size() < 3)
	{
		std::cout << "Can't process face with less than 3 vertices!" << std::endl;
	} else
	{
		/**
		 * all faces in obj format are assumed to be convex so
		 * the fan triangulation method is used
		 * https://en.wikipedia.org/wiki/Fan_triangulation
		 */
		const unsigned int anchorVertex = 0;

		for (unsigned int i = 1; i < faceVertexIndices.size() - 1; i++)
		{
			objectList.push_back(
				new Triangle(
					vertices[faceVertexIndices[anchorVertex]],
					vertices[faceVertexIndices[anchorVertex + i]],
					vertices[faceVertexIndices[anchorVertex + i + 1]]
				)
			);
		}
	}
}
