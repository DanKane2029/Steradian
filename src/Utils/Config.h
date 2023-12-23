#pragma once
#include <string>
#include <vector>

#include "Scene/Scene.h"

class Config
{
  private:
    struct Keywords
    {
        const std::string windowHeight = "windowHeight";
        const std::string windowWidth = "windowWidth";
        const std::string fps = "fps";
        const std::string numThreads = "numThreads";
        const std::string numChildrenInBVHLeafNodes = "numChildrenInBVHLeafNodes";
    };
    Keywords m_keywrods;

  public:
    int windowHeight{};
    int windowWidth{};
    unsigned int fps;
    unsigned int numThreads;
    unsigned int numChildrenInBVHLeafNodes;

  public:
    Config(std::string filePath);

  private:
    void loadConfig(std::string filePath);
};