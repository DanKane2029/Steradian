#include "Config.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace
{

/**
 * reads an optional unsigned value, falling back to a default
 *
 * Every field was previously required, and read with operator[] rather than at(), so a
 * missing one produced a null and then threw a type error naming neither the file nor the
 * key. Defaults make a minimal config usable and mistakes easier to diagnose.
 */
auto optionalUInt(const json &data, const std::string &key, unsigned int fallback) -> unsigned int
{
    if (!data.contains(key) || data.at(key).is_null())
    {
        return fallback;
    }

    return data.at(key).get<unsigned int>();
}

} // namespace

Config::Config(std::string filePath)
{
    loadConfig(std::move(filePath));
}

void Config::loadConfig(std::string filePath)
{
    std::ifstream f(filePath);
    if (!f)
    {
        throw std::runtime_error("Could not open config file: " + filePath);
    }

    json data = json::parse(f);
    f.close();

    windowWidth = static_cast<int>(optionalUInt(data, m_keywrods.windowWidth, 800));
    windowHeight = static_cast<int>(optionalUInt(data, m_keywrods.windowHeight, 600));
    numThreads = optionalUInt(data, m_keywrods.numThreads, 8);
    numChildrenInBVHLeafNodes = optionalUInt(data, m_keywrods.numChildrenInBVHLeafNodes, 4);
    maxRecurseLevel = optionalUInt(data, m_keywrods.maxRecurseLevel, 8);
}
