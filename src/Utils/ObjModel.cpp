#include "ObjModel.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <unistd.h>

#include "Utils/StringUtils.h"

ObjModel::ObjModel(std::string filePath) : material(Material("obj_material"))
{
    loadModel(filePath);
}

/**
 * loads an obj model as a list of triangles into the scene
 *
 * \param filePath - the file path to the model obj file
 * \return - true if the model was sucessfully loaded, false if there was an
 * error
 */
auto ObjModel::loadModel(std::string filePath) -> bool
{
    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
    std::vector<SceneObject> objList;

    std::ifstream objFile(filePath.c_str(), std::ios::in);
    int lineCount = 0;
    if (objFile)
    {
        std::string line;

        while (std::getline(objFile, line))
        {
            if (line.empty())
            {
                continue;
            }

            std::vector<std::string> splitLine = splitString(line, std::string(" "));

            std::string lineType = splitLine[0];
            splitLine.erase(splitLine.begin());

            if (lineType == VERTEX_POS)
            {
                Vec3 vertex = parseVec3(splitLine);
                m_vertexList.push_back(vertex);
            }
            else if (lineType == VERTEX_TEX)
            {
                // OBJ stores texture coordinates as "vt u v [w]", so the optional third
                // component defaults to zero rather than being required.
                Vec3 texCoord;
                texCoord.x = std::stof(splitLine[1]);
                texCoord.y = (splitLine.size() > 2) ? std::stof(splitLine[2]) : 0.0f;
                texCoord.z = 0.0f;

                m_textureCoordList.push_back(texCoord);
            }
            else if (lineType == VERTEX_NORM)
            {
                Vec3 normal = parseVec3(splitLine);
                m_normalList.push_back(normal);
            }
            else if (lineType == FACE)
            {
                std::vector<FaceIndices> faceIndices = parseFace(splitLine);
                m_faceIndicesList.push_back(faceIndices);
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
 * creates a vector Vec3 from a vector of vectors of strings of size 3
 * the strings must be able to be converted into a float
 *
 * \param vertexData - the vector of strings to be converted into floats
 * \param vertices - the vector of Vec3 to add the newly created Vec3s to
 */
auto ObjModel::parseVec3(std::vector<std::string> &vertexData) -> Vec3
{
    if (vertexData.size() < 3)
    {
        std::cout << "Vec3 size less than 3!" << std::endl;
    }

    float x = std::stof(vertexData[0]);
    float y = std::stof(vertexData[1]);
    float z = std::stof(vertexData[2]);

    return {x, y, z};
}

/**
 * reads vector of vertices information and adds the necessary amount of
 * traingle to accurately describe the face
 *
 * face vertex string format: "vertexIndex/textureCoord/vertexNormal"
 *
 * \param faceIndices - vector of vertex information as a string
 */
auto ObjModel::parseFace(std::vector<std::string> &faceIndices) -> std::vector<ObjModel::FaceIndices>
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

auto ObjModel::createTriangleFromFaceIndices(FaceIndices fi0, FaceIndices fi1, FaceIndices fi2) -> Triangle
{
    // fi1 is included in each of these. It was previously omitted (fi0 was tested twice),
    // so a face with a missing middle index passed validation and then indexed the vertex
    // list out of bounds.
    const bool positionsSet = fi0.positionSet && fi1.positionSet && fi2.positionSet;
    const bool normalsSet = fi0.normalSet && fi1.normalSet && fi2.normalSet;
    const bool textureSet = fi0.textureSet && fi1.textureSet && fi2.textureSet;

    if (!positionsSet)
    {
        throw std::runtime_error("OBJ face is missing vertex position indices");
    }

    // OBJ indices are 1-based, and negative values refer back from the end of the list.
    const auto resolve = [](int index, size_t count) -> size_t {
        const long long resolved = (index < 0) ? static_cast<long long>(count) + index : index - 1;

        if (resolved < 0 || resolved >= static_cast<long long>(count))
        {
            throw std::runtime_error("OBJ index " + std::to_string(index) + " is out of range (list holds " +
                                     std::to_string(count) + ")");
        }

        return static_cast<size_t>(resolved);
    };

    const size_t p0 = resolve(fi0.position, m_vertexList.size());
    const size_t p1 = resolve(fi1.position, m_vertexList.size());
    const size_t p2 = resolve(fi2.position, m_vertexList.size());

    Triangle tri;

    if (normalsSet)
    {
        tri = Triangle(m_vertexList[p0], m_normalList[resolve(fi0.normal, m_normalList.size())], m_vertexList[p1],
                       m_normalList[resolve(fi1.normal, m_normalList.size())], m_vertexList[p2],
                       m_normalList[resolve(fi2.normal, m_normalList.size())]);
    }
    else
    {
        tri = Triangle(m_vertexList[p0], m_vertexList[p1], m_vertexList[p2]);
    }

    if (textureSet && !m_textureCoordList.empty())
    {
        tri.setTextureCoords(m_textureCoordList[resolve(fi0.texture, m_textureCoordList.size())],
                             m_textureCoordList[resolve(fi1.texture, m_textureCoordList.size())],
                             m_textureCoordList[resolve(fi2.texture, m_textureCoordList.size())]);
    }

    return tri;
}

auto ObjModel::getSceneObjects() -> std::vector<std::shared_ptr<SceneObject>>
{
    std::vector<std::shared_ptr<SceneObject>> sceneObjectList;

    for (const std::vector<ObjModel::FaceIndices> &faceIndices : m_faceIndicesList)
    {
        // A face needs at least three vertices. Previously anything not larger than three
        // was assumed to be exactly three and indexed directly, so a malformed two-vertex
        // face read out of bounds.
        if (faceIndices.size() < 3)
        {
            std::cerr << "Skipping OBJ face with only " << faceIndices.size() << " vertices" << std::endl;
            continue;
        }

        for (Triangle &tri : triangulateFace(faceIndices))
        {
            sceneObjectList.push_back(std::make_shared<Triangle>(tri));
        }
    }

    return sceneObjectList;
}

/**
 * splits an n-gon into a triangle fan around its first vertex
 *
 * An n-gon yields n-2 triangles. The loop bound was previously size-2, which produced
 * n-3 and so dropped the last triangle of every face: a quad became a single triangle,
 * leaving half of every quad mesh missing.
 */
auto ObjModel::triangulateFace(std::vector<ObjModel::FaceIndices> faceIndices) -> std::vector<Triangle>
{
    const FaceIndices start = faceIndices[0];
    std::vector<Triangle> triList;
    triList.reserve(faceIndices.size() - 2);

    for (size_t i = 1; i + 1 < faceIndices.size(); i++)
    {
        triList.emplace_back(createTriangleFromFaceIndices(start, faceIndices[i], faceIndices[i + 1]));
    }

    return triList;
}

auto ObjModel::getCenterPoint() -> Vec3
{
    Vec3 sum;
    for (Vec3 v : m_vertexList)
    {
        sum = sum + v;
    }

    Vec3 center = sum / static_cast<float>(m_vertexList.size());
    return center;
}
