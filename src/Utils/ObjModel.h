#pragma once
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Scene/Material.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
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
    std::vector<std::shared_ptr<SceneObject>> getSceneObjects();
    inline std::vector<Vec3> getVertexList()
    {
        return m_vertexList;
    };
    auto getCenterPoint() -> Vec3;

  private:
    auto loadModel(std::string filePath) -> bool;
    std::vector<std::string> splitString(std::string &inputString, std::string delimiter);
    auto parseVec3(std::vector<std::string> &vertexData) -> Vec3;
    std::vector<FaceIndices> parseFace(std::vector<std::string> &faceIndices);
    std::vector<Triangle> triangulateFace(std::vector<ObjModel::FaceIndices> faceIndices);
    Triangle createTriangleFromFaceIndices(FaceIndices fi0, FaceIndices fi1, FaceIndices fi2);
};
