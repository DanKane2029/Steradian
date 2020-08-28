#include "ModelLoader.h"

bool ModelLoader::LoadModel(std::string filePath, std::string materialName, Scene& scene)
{
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

        return true;

    } else
    {
        return false;
    }
}

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

void ModelLoader::ParseVec3(std::vector<std::string>& vertexData, std::vector<Vec3>& vertices)
{
    if (vertexData.size() < 3)
        std::cout << "Vec3 size less than 3!" << std::endl;

    float x = std::stof(vertexData[0]);
    float y = std::stof(vertexData[1]);
    float z = std::stof(vertexData[2]);

    vertices.push_back(Vec3(x, y, z));
}

void ModelLoader::ParseFace(std::vector<std::string>& faceIndices, std::vector<SceneObject*>& objectList, 
    std::vector<Vec3>& vertices, std::vector<Vec3>& normals)
{
    
    std::vector<unsigned int> faceVertexIndices;

    for (std::string s : faceIndices)
    {
        std::vector<std::string> faceIndexSplit;
        SplitString(s, std::string("/"), faceIndexSplit);

        unsigned int vertexIndex = NULL;
        unsigned int uvCoordIndex = NULL;
        unsigned int normalIndex = NULL;

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

        if (vertexIndex != NULL)
        {
            faceVertexIndices.push_back(vertexIndex - 1);
        }
    }

    if (faceVertexIndices.size() == 3)
    {
        objectList.push_back(
            new Triangle(
                vertices[faceVertexIndices[0]],
                vertices[faceVertexIndices[1]], 
                vertices[faceVertexIndices[2]]
            )
        );

    } else if (faceVertexIndices.size() == 4)
    {        
        objectList.push_back(
            new Triangle(
                vertices[faceVertexIndices[0]],
                vertices[faceVertexIndices[1]],
                vertices[faceVertexIndices[2]]
            )
        );

        objectList.push_back(
            new Triangle(
                vertices[faceVertexIndices[0]],
                vertices[faceVertexIndices[2]],
                vertices[faceVertexIndices[3]]
            )
        );
    }

}
