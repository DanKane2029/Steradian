#include "Config.h"

#include <fstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

Config::Config(std::string filePath)
{
    loadConfig(filePath);
}

void Config::loadConfig(std::string filePath)
{
    std::ifstream f(filePath);
    json data = json::parse(f);

    windowHeight = data[m_keywrods.windowHeight];
    windowWidth = data[m_keywrods.windowWidth];
    fps = data[m_keywrods.fps];
    numThreads = data[m_keywrods.numThreads];
}