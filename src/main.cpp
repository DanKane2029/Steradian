#include "Window.h"
#include "PixelBuffer.h"

#include <iostream>

int main()
{
    const unsigned int width = 500;
    const unsigned int height = 500;
    
    Window window("Path Tracer", width, height);

    auto fbSize = window.GetFrameBufferSize();
    PixelBuffer pixelBuffer(fbSize.first, fbSize.second);

    window.SetPixelBuffer(&pixelBuffer);

    float white[3] = { 1.0f, 1.0f, 1.0f };
    size_t numPixPerCycle = 100;

    while (!window.ShouldClose())
    {
        auto curSize = window.GetFrameBufferSize();
        unsigned int curWidth = curSize.first;
        unsigned int curHeight = curSize.second;
        for (size_t i = 0; i < numPixPerCycle; i++)
        {
            unsigned int x = rand() % curWidth;
            unsigned int y = rand() % curHeight;
            pixelBuffer.SetPixel(x, y, white);
        }

        window.UpdatePixels(pixelBuffer.GetPixels());
        window.Update();
    }
}