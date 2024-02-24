#pragma once

#include <string>

class Texture
{
  public:
    int width;
    int height;
    int numChannels;
    float *data;

    Texture(std::string &path);

    
};