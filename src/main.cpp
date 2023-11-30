#include "RayTracer/RayTracer.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Utils/Config.h"
#include "Utils/ObjModel.h"
#include "Window/PixelBuffer.h"
#include "Window/Window.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <thread>

auto main() -> int
{
    std::cout << "Loading config file..." << std::endl;
    Config config("/mnt/c/Users/dpk20/Dev/Path_Tracer/res/configs/test_config.json");

    Window window("Path Tracer", config.windowWidth, config.windowHeight);

    // framebuffer size sometimes different than window size
    auto fbSize = window.getFrameBufferSize();
    PixelBuffer pixelBuffer(fbSize.first, fbSize.second);
    window.setPixelBuffer(&pixelBuffer);

    // create scene
    Scene scene("/mnt/c/Users/dpk20/Dev/Path_Tracer/res/scenes/test_scene.json");

    // Material red("Red", Vec3(0.8f, 0.9f, 0.3f), 0.75f, 0.2f, 0.25f, 3.0f);
    // scene.registerMaterial(&red);

    // ObjModel objModel("/mnt/c/Users/dpk20/Dev/Path_Tracer/res/models/cone.obj");
    // // ObjModel objModel("/mnt/c/Users/dpk20/Dev/Path_Tracer/res/models/Eagle.obj");
    // std::vector<std::shared_ptr<SceneObject>> modelObjects = objModel.getSceneObjects();
    // scene.addObjects(modelObjects, "Red");

    // Camera camera({0.0f, 0.0f, 100.0f}, objModel.getCenterPoint());

    // scene.setAmbientLighting({0.2f, 0.2f, 0.2f});

    // PointLight light({0.5f, 0.5f, 1.0f}, {0.75f, 0.75f, 0.75f}, 0.8f, 0.25f);
    // scene.addLight(&light);

    scene.createAcceleratedStructure();
    // end scene

    RayTracer rayTracer(&pixelBuffer, &scene);

    // set up thread safe random number generator [0.0, 1.0)
    std::default_random_engine randomGenerator;
    std::uniform_real_distribution<float> dist(0.0, 1.0);

    // set up thread list for ray shooting
    std::vector<std::thread> threads(config.numThreads);
    std::mutex consoleMutex;

    // ray shooting meta data
    unsigned int totalSampleCount = 0;
    std::mutex totalSampleCountMutex;

    // application loop
    while (!window.shouldClose())
    {
        auto curSize = window.getFrameBufferSize();
        unsigned int curWidth = curSize.first;
        unsigned int curHeight = curSize.second;

        // initialize threads for ray shooting
        for (size_t i = 0; i < threads.size(); i++)
        {
            if (curWidth > 0 && curHeight > 0)
            {
                threads[i] = std::thread([&]() {
                    double curTime = glfwGetTime();
                    double endingTime = curTime + (1.0F / static_cast<float>(config.fps));

                    // shoot rays until ready to display next frame
                    while (curTime < endingTime)
                    {
                        float x = dist(randomGenerator);
                        float y = dist(randomGenerator);
                        rayTracer.sampleScene(x, y);
                        curTime = glfwGetTime();
                    }
                });
            }
        }

        // wait for all threads to finish
        for (std::thread &t : threads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }

        window.update();
    }

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
