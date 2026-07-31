#pragma once

#include <string>

class Texture
{
  public:
    // These are initialized here on purpose: shading code branches on `data` to decide
    // whether a material is textured, so a default-constructed Texture must reliably
    // read as "no texture" rather than as an indeterminate pointer.
    int width = 0;
    int height = 0;
    int numChannels = 0;
    float *data = nullptr;

    Texture(std::string &path);
};