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
        const std::string numThreads = "numThreads";
        const std::string numChildrenInBVHLeafNodes = "numChildrenInBVHLeafNodes";
        const std::string maxRecurseLevel = "maxRecurseLevel";
    };
    Keywords m_keywrods;

  public:
    int windowHeight{};
    int windowWidth{};

    // Note: `fps` and `numShadowRays` used to live here. The first bounded how long the
    // viewer sampled before presenting a frame, which the progressive renderer replaced;
    // the second set the shadow ray count of the direct lighting model, which the path
    // integrator replaced with next event estimation. Both are ignored if present in a
    // config file.
    // Defaulted rather than left indeterminate. A config built in code, as the tests do,
    // otherwise starts with a garbage path depth and a garbage thread count.
    unsigned int numThreads = 8;
    unsigned int numChildrenInBVHLeafNodes = 2;
    unsigned int maxRecurseLevel = 10;

  public:
    /** \brief A config with the defaults above, for callers that set what they need. */
    Config() = default;

    Config(std::string filePath);

  private:
    void loadConfig(std::string filePath);
};