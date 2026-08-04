#include "ObjModel.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

#include "Utils/StringUtils.h"

ObjModel::ObjModel(std::string filePath)
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
        // Empty fields are kept: in "2//2" the gap is what says the corner has a normal
        // and no texture coordinate. Dropping it makes the normal index look like a
        // texture index, and the model silently loses every vertex normal it had.
        std::vector<std::string> faceIndexSplit = splitString(s, std::string("/"), true);

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

namespace
{

/**
 * resolves an .obj index against a list
 *
 * Indices are 1-based, and negative values refer back from the end of the list.
 */
auto resolveIndex(int index, size_t count) -> uint32_t
{
    const long long resolved = (index < 0) ? static_cast<long long>(count) + index : index - 1;

    if (resolved < 0 || resolved >= static_cast<long long>(count))
    {
        throw std::runtime_error("OBJ index " + std::to_string(index) + " is out of range (list holds " +
                                 std::to_string(count) + ")");
    }

    return static_cast<uint32_t>(resolved);
}

} // namespace

auto ObjModel::resolveVertex(const FaceIndices &corner, bool useNormal, bool useTexture, Geometry &geometry,
                             VertexCache &cache) -> uint32_t
{
    VertexKey key;
    key.position = resolveIndex(static_cast<int>(corner.position), m_vertexList.size());

    if (useNormal)
    {
        key.normal = resolveIndex(static_cast<int>(corner.normal), m_normalList.size());
    }

    if (useTexture)
    {
        key.texture = resolveIndex(static_cast<int>(corner.texture), m_textureCoordList.size());
    }

    const auto existing = cache.find(key);
    if (existing != cache.end())
    {
        return existing->second;
    }

    // A vertex with no normal is stored with a zero one, which makes interpolation fall
    // back to the triangle's own plane. Absent texture coordinates likewise store zero,
    // and interpolate to zero, which is what an untextured triangle reported before.
    const Vec3 normal = useNormal ? m_normalList[key.normal] : Vec3();
    const Vec3 texCoord = useTexture ? m_textureCoordList[key.texture] : Vec3();

    const uint32_t index = geometry.addVertex(m_vertexList[key.position], normal, texCoord);
    cache.emplace(key, index);

    return index;
}

/**
 * splits an n-gon into a triangle fan around its first vertex
 *
 * An n-gon yields n-2 triangles. The loop bound was once size-2, which produced n-3 and
 * so dropped the last triangle of every face: a quad became a single triangle, leaving
 * half of every quad mesh missing.
 */
void ObjModel::appendFace(const std::vector<FaceIndices> &faceIndices, Geometry &geometry, VertexCache &cache,
                          uint32_t materialIndex)
{
    for (size_t i = 1; i + 1 < faceIndices.size(); i++)
    {
        const FaceIndices &corner0 = faceIndices[0];
        const FaceIndices &corner1 = faceIndices[i];
        const FaceIndices &corner2 = faceIndices[i + 1];

        // All three corners are checked. fi1 was once omitted (the first was tested
        // twice), so a face with a missing middle index passed validation and then
        // indexed the vertex list out of bounds.
        if (!corner0.positionSet || !corner1.positionSet || !corner2.positionSet)
        {
            throw std::runtime_error("OBJ face is missing vertex position indices");
        }

        // Normals and texture coordinates are all-or-nothing per triangle: interpolating
        // between a supplied normal and a missing one has no meaning.
        const bool useNormal = corner0.normalSet && corner1.normalSet && corner2.normalSet;
        const bool useTexture =
            corner0.textureSet && corner1.textureSet && corner2.textureSet && !m_textureCoordList.empty();

        // Resolved one at a time, and in order, because each call may append a vertex.
        // As arguments to one call the order would be unspecified, and the vertex buffer
        // would come out laid out differently from one compiler to the next.
        const uint32_t vertex0 = resolveVertex(corner0, useNormal, useTexture, geometry, cache);
        const uint32_t vertex1 = resolveVertex(corner1, useNormal, useTexture, geometry, cache);
        const uint32_t vertex2 = resolveVertex(corner2, useNormal, useTexture, geometry, cache);

        geometry.addTriangle(vertex0, vertex1, vertex2, materialIndex);
    }
}

void ObjModel::appendTo(Geometry &geometry, uint32_t materialIndex)
{
    VertexCache cache;
    cache.reserve(m_vertexList.size() * 2);

    geometry.reserveVertices(m_vertexList.size());
    geometry.reserveTriangles(m_faceIndicesList.size());

    for (const std::vector<FaceIndices> &faceIndices : m_faceIndicesList)
    {
        // A face needs at least three vertices. Anything not larger than three was once
        // assumed to be exactly three and indexed directly, so a malformed two-vertex
        // face read out of bounds.
        if (faceIndices.size() < 3)
        {
            std::cerr << "Skipping OBJ face with only " << faceIndices.size() << " vertices" << std::endl;
            continue;
        }

        appendFace(faceIndices, geometry, cache, materialIndex);
    }
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
