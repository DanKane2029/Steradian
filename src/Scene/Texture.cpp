#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION

#include <Utils/stb_image.h>
#include <iostream>

float *loadImage(std::string &path, int *width, int *height, int *channels)
{
    return stbi_loadf(path.c_str(), width, height, channels, 0);
}

Texture::Texture(std::string &path)
{
    data = loadImage(path, &width, &height, &numChannels);
}