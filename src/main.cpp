#include "Window.h"
#include "PixelBuffer.h"
#include "Scene.h"
#include "RayTracer.h"

#include <iostream>
#include <cstdlib>

int main()
{
    const unsigned int width = 500;
    const unsigned int height = 500;
    
    Window window("Path Tracer", width, height);

    auto fbSize = window.GetFrameBufferSize();
    PixelBuffer pixelBuffer(fbSize.first, fbSize.second);
    window.SetPixelBuffer(&pixelBuffer);

    Scene scene;

    Triangle tri(
        { -1.0f, -1.0f, -2.0f }, 
        {  1.0f, -1.0f, -2.0f }, 
        { -1.0f,  1.0f, -2.0f }
    );

    Triangle tri2(
        { -1.0f,  1.0f, -2.0f },
        {  1.0f,  1.0f, -2.0f },
        {  1.0f, -1.0f, -2.0f }
    );

    Sphere sp({0, 0, -2.0f}, 0.5f);

    Material blue(std::string("Blue"), Vec3(0.1f, 0.3f, 0.8f));
    scene.RegisterMaterial(&blue);

    Material green(std::string("Green"), Vec3(0.1f, 0.8f, 0.3f));
    scene.RegisterMaterial(&green);

    Material red(std::string("Red"), Vec3(0.8f, 0.1f, 0.3f));
    scene.RegisterMaterial(&red);

    scene.AddObject(&tri, std::string("Blue"));
    scene.AddObject(&tri2, std::string("Green"));
    scene.AddObject(&sp, std::string("Red"));

    Camera camera({0.5, 0.3, 0.3}, {0,0,-2.0f});
    RayTracer rayTracer(&pixelBuffer, &scene, camera, 45.0f, 45.0f);

    size_t numPixPerCycle = 1000;

    srand(0);

    while (!window.ShouldClose())
    {
        auto curSize = window.GetFrameBufferSize();
        unsigned int curWidth = curSize.first;
        unsigned int curHeight = curSize.second;
        for (size_t i = 0; i < numPixPerCycle; i++)
        {
            if (curWidth > 0 && curHeight > 0)
            {
                float x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                float y = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

                rayTracer.SampleScene(x, y);
            }
        }

        window.Update();
    }
}
