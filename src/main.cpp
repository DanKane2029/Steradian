#include "Window.h"
#include "PixelBuffer.h"

int main()
{
    const unsigned int width = 500;
    const unsigned int height = 500;
    
    Window window("Path Tracer", width, height);

    auto fbSize = window.GetFrameBufferSize();
    PixelBuffer pixelBuffer(fbSize.first, fbSize.second);
    window.SetPixelBuffer(&pixelBuffer);

    size_t numPixPerCycle = 100;
    float white[3] = {1.0f, 1.0f, 1.0f};

    while (!window.ShouldClose())
    {
        auto curSize = window.GetFrameBufferSize();
        unsigned int curWidth = curSize.first;
        unsigned int curHeight = curSize.second;
        for (size_t i = 0; i < numPixPerCycle; i++)
        {
            if (curWidth > 0 && curHeight > 0)
            {
                unsigned int x = rand() % curWidth;
                unsigned int y = rand() % curHeight;
                float p[3] = { 1.0f, 1.0f, 1.0f };
                pixelBuffer.SetPixel(x, y, p);
            }
        }

        window.Update();
    }
}
