#include "Window.h"
#include "PixelBuffer.h"
#include "Scene.h"
#include "RayTracer.h"
#include "ModelLoader.h"

#include <iostream>
#include <random>
#include <thread>
#include <algorithm>
#include <functional>

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
    //std::string modelFilePath = "C:\\Users\\Daniel Kane\\Development\\Projects\\Path_Tracer\\res\\models\\lowpolytree.obj";
    //std::string modelFilePath = "C:\\Users\\Daniel Kane\\Development\\Projects\\Path_Tracer\\res\\models\\cone.obj";
    std::string modelFilePath = "C:\\Users\\Daniel Kane\\Development\\Projects\\Path_Tracer\\res\\models\\cube.obj";
    ModelLoader::LoadModel(modelFilePath, std::string("Red"), scene);

    Camera camera({ 0.0f, 0.0f, 2.0f }, { 0,0,-1.0f });

    Material red(
        std::string("Red"), 
        Vec3(0.8f, 0.1f, 0.3f),
        0.75f,
        0.2f,
        0.25f,
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
    double FPS = 60;
    double secondsPerFrame = 1.0 / FPS;

    std::default_random_engine randomGenerator;
    std::uniform_real_distribution<float> dist(0.0, 1.0);

    const unsigned int NUM_THREADS = std::thread::hardware_concurrency();
    std::vector<std::thread> threads(NUM_THREADS);
    std::mutex consoleMutex;

    unsigned int totalSampleCount = 0;
    std::mutex totalSampleCountMutex;

    while (!window.ShouldClose())
    {
        auto curSize = window.GetFrameBufferSize();
        unsigned int curWidth = curSize.first;
        unsigned int curHeight = curSize.second;
        for (size_t i = 0; i < NUM_THREADS; i++)
        {
            if (curWidth > 0 && curHeight > 0)
            {
                threads[i] = std::thread([&](){

                    float x = dist(randomGenerator);
                    float y = dist(randomGenerator);

                    double curTime = glfwGetTime();
                    double endingTime = curTime + secondsPerFrame;
                    int sampleCount = 0;

                    while (curTime < endingTime)
                    {
                        rayTracer.SampleScene(x, y);
                        curTime = glfwGetTime();
                        sampleCount++;
                    }

                    totalSampleCountMutex.lock();
                    totalSampleCount += sampleCount;
                    totalSampleCountMutex.unlock();
                });
            }
        }

        for (std::thread& t : threads)
        {
            if (t.joinable()) t.join();
        }

        window.Update();
    }
}
