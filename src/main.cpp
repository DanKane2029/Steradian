#include "Window.h"
#include "PixelBuffer.h"
#include "Scene.h"
#include "RayTracer.h"
#include "ModelLoader.h"

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
    
    //std::string modelFilePath = "C:\\Users\\Daniel Kane\\Development\\Projects\\Path_Tracer\\res\\models\\Eagle.obj";
    std::string modelFilePath = "C:\\Users\\Daniel Kane\\Development\\Projects\\Path_Tracer\\res\\models\\lowpolytree.obj";
    ModelLoader::LoadModel(modelFilePath, std::string("Red"), scene);

    Camera camera({ 0.0f, 0.0f, -5.0f }, { 0,0,-1.0f });

    Material red(
        std::string("Red"), 
        Vec3(0.8f, 0.1f, 0.3f),
        0.75,
        0.2,
        0.25,
        3.0f
    );
    scene.RegisterMaterial(&red);

    scene.SetAmbientLighting({0.2f, 0.2f, 0.2f});

    PointLight light(
        {0.5f, 0.25f, -1.5f},
        {0.75f, 0.75f, 0.75f},
        0.5f,
        0.25f
    );
    scene.AddLight(&light);

    RayTracer rayTracer(&pixelBuffer, &scene, camera);

    size_t numPixPerCycle = 10;

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
