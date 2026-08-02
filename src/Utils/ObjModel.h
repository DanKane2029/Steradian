#pragma once
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Scene/Geometry.h"
#include "Scene/Material.h"
#include "Utils/Vec3.h"

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

    std::vector<Vec3> m_vertexList{};
    std::vector<Vec3> m_normalList{};
    std::vector<Vec3> m_textureCoordList{};
    Material material{"default_obj_material_name"};

    struct FaceIndices
    {
        unsigned int position{};
        bool positionSet = false;

        unsigned int texture{};
        bool textureSet = false;

        unsigned int normal{};
        bool normalSet = false;
    };
    std::vector<std::vector<FaceIndices>> m_faceIndicesList{};

  public:
    ObjModel(std::string filePath);

    /**
     * \brief Adds this model's triangles to a scene's geometry.
     *
     * Vertices are shared: a position, normal and texture coordinate triple that several
     * faces refer to is stored once and indexed three times. That is how an .obj file
     * already describes a mesh, so honouring it costs a lookup at load and saves storing
     * the Stanford dragon's vertices six times over.
     *
     * \param geometry Destination for the vertices and triangles.
     * \param materialIndex The material every triangle of this model uses.
     */
    void appendTo(Geometry &geometry, uint32_t materialIndex);

    inline std::vector<Vec3> getVertexList()
    {
        return m_vertexList;
    };
    auto getCenterPoint() -> Vec3;

  private:
    /** marks a component the .obj file did not give this corner */
    static constexpr uint32_t noIndex = 0xFFFFFFFFu;

    /**
     * \brief One corner of a face, as the .obj file indexes it.
     *
     * The three lists are indexed independently in the file format, so a vertex is
     * identified by the triple rather than by any one of them. Two corners are the same
     * vertex only when all three agree.
     */
    struct VertexKey
    {
        uint32_t position = noIndex;
        uint32_t normal = noIndex;
        uint32_t texture = noIndex;

        auto operator==(const VertexKey &other) const -> bool
        {
            return position == other.position && normal == other.normal && texture == other.texture;
        }
    };

    struct VertexKeyHash
    {
        auto operator()(const VertexKey &key) const -> size_t
        {
            size_t hash = key.position;
            hash = (hash * 0x9e3779b97f4a7c15ULL) ^ key.normal;
            hash = (hash * 0x9e3779b97f4a7c15ULL) ^ key.texture;
            return hash;
        }
    };

    using VertexCache = std::unordered_map<VertexKey, uint32_t, VertexKeyHash>;

    auto loadModel(std::string filePath) -> bool;
    auto parseVec3(std::vector<std::string> &vertexData) -> Vec3;
    std::vector<FaceIndices> parseFace(std::vector<std::string> &faceIndices);

    /** splits an n-gon into a fan around its first vertex and appends the triangles */
    void appendFace(const std::vector<FaceIndices> &faceIndices, Geometry &geometry, VertexCache &cache,
                    uint32_t materialIndex);

    /** finds or creates the shared vertex a face corner refers to */
    auto resolveVertex(const FaceIndices &corner, bool useNormal, bool useTexture, Geometry &geometry,
                       VertexCache &cache) -> uint32_t;
};
