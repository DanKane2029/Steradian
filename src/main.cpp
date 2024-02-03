#include "RayTracer/RayTracer.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Utils/Config.h"
#include "Utils/ObjModel.h"
#include "Window/PixelBuffer.h"
#include "Window/Window.h"

#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <thread>

auto main(int argc, char *argv[]) -> int
{
    if (argc < 2)
    {
        throw std::invalid_argument("Too few arguments! Need config file path and scene file path.");
    }

    std::string configPath = std::string(argv[1]);
    std::string scenePath = std::string(argv[2]);

    // load config file
    Config config(configPath);

    // create window
    Window window("Path Tracer", config.windowWidth, config.windowHeight);

    // framebuffer size sometimes different than window size
    auto fbSize = window.getFrameBufferSize();
    PixelBuffer pixelBuffer(fbSize.first, fbSize.second);
    window.setPixelBuffer(&pixelBuffer);

    // create scene
    Scene scene(scenePath);
    scene.createAcceleratedStructure();

    RayTracer rayTracer(&pixelBuffer, &scene, config);
    window.setRayTracer(&rayTracer);

    // set up thread safe random number generator [0.0, 1.0)
    std::default_random_engine randomGenerator;
    std::uniform_real_distribution<float> dist(0.0, 1.0);

    // set up thread list for ray shooting
    std::vector<std::thread> threads;

    // application loop
    while (!window.shouldClose())
    {
        auto curSize = window.getFrameBufferSize();
        unsigned int curWidth = curSize.first;
        unsigned int curHeight = curSize.second;

        // initialize threads for ray shooting
        for (size_t i = 0; i < config.numThreads; i++)
        {
            if (curWidth > 0 && curHeight > 0)
            {
                threads.emplace_back([&]() {
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
