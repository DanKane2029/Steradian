#include "Window.h"
#include "PixelBuffer.h"
#include "Scene.h"
#include "RayTracer.h"

#include <iostream>
#include <cstdlib>

int main()
{
    const unsigned int width = 300;
    const unsigned int height = 300;
    
    Window window("Path Tracer", width, height);

    auto fbSize = window.GetFrameBufferSize();
    PixelBuffer pixelBuffer(fbSize.first, fbSize.second);
    window.SetPixelBuffer(&pixelBuffer);

    Scene scene;

    Camera camera({ 0.0f, 0.5f, 0.0f }, { 0,0,-1.0f });

    Sphere sphere({0.0f, 0.0f, -1.0f}, 0.3f);
    Sphere sphere2({ 0.0, 0.5f, -1.0f}, 0.3f);

    Triangle tri1(
        {-10, 0,  10},
        { 10, 0,  10},
        {-10, 0, -10}
    );
    Triangle tri2(
        {  10, 0, -10 },
        { -10, 0, -10 },
        {  10, 0,  10 }
    );

    Material blue(
        std::string("Blue"), 
        Vec3(0.1f, 0.3f, 0.8f),
        0.75,
        0.2,
        0.25,
        3.0f
    );
    scene.RegisterMaterial(&blue);

    Material red(
        std::string("Red"),
        Vec3(0.8f, 0.3f, 0.1f),
        0.75,
        0.2,
        0.25,
        3.0f
    );
    scene.RegisterMaterial(&red);

    Material green(
        std::string("Green"),
        Vec3(0.1f, 0.8f, 0.3f),
        0.5,
        0.25,
        0.25,
        1.0f
    );
    scene.RegisterMaterial(&green);

    scene.AddObject(&sphere, std::string("Blue"));
    scene.AddObject(&sphere2, std::string("Red"));
    scene.AddObject(&tri1, std::string("Green"));
    scene.AddObject(&tri2, std::string("Green"));

    scene.SetAmbientLighting({0.2f, 0.2f, 0.2f});

    PointLight light(
        {0.5f, 0.25f, -1.5f},
        {0.75f, 0.75f, 0.75f},
        0.5f,
        0.25f
    );
    scene.AddLight(&light);

    RayTracer rayTracer(&pixelBuffer, &scene, camera);

    size_t numPixPerCycle = 500;

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
