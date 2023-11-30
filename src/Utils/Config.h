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
    };
    Keywords m_keywrods;

  public:
    unsigned int windowHeight{};
    unsigned int windowWidth{};
    unsigned int fps;
    unsigned int numThreads;

  public:
    Config(std::string filePath);

  private:
    void loadConfig(std::string filePath);
};