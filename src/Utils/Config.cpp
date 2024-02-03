#include "Config.h"

#include <fstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

Config::Config(std::string filePath) : fps(30), numThreads(1), numChildrenInBVHLeafNodes(25)
{
    loadConfig(filePath);
}

void Config::loadConfig(std::string filePath)
{
    std::ifstream f(filePath);
    json data = json::parse(f);
    f.close();

    windowHeight = data[m_keywrods.windowHeight];
    windowWidth = data[m_keywrods.windowWidth];
    fps = data[m_keywrods.fps];
    numThreads = data[m_keywrods.numThreads];
    numChildrenInBVHLeafNodes = data[m_keywrods.numChildrenInBVHLeafNodes];
    numShadowRays = data[m_keywrods.numShadowRays];
    maxRecurseLevel = data[m_keywrods.maxRecurseLevel];
}