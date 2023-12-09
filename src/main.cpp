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
    // Triangle tri(Vec3(-0.5, -0.5, 0), Vec3(0.5, -0.5, 0), Vec3(0, 0.5, 0));
    // Ray r(Vec3(0, 0, 2), Vec3(-0.00249222224, 0.0079478249, -0.99996531));
    // Hit hit = tri.rayIntersect(r);

    // load config file
    Config config("/mnt/c/Users/dpk20/Dev/Path_Tracer/res/configs/test_config.json");

    // create window
    Window window("Path Tracer", config.windowWidth, config.windowHeight);

    // framebuffer size sometimes different than window size
    auto fbSize = window.getFrameBufferSize();
    PixelBuffer pixelBuffer(fbSize.first, fbSize.second);
    window.setPixelBuffer(&pixelBuffer);

    // create scene
    Scene scene("/mnt/c/Users/dpk20/Dev/Path_Tracer/res/scenes/single_sphere.json");

    scene.createAcceleratedStructure();

    for (std::shared_ptr<Light> l : scene.getLightList())
    {
        Vec3 p = l->getPos();
        std::cout << "light pos: " << p.x() << ", " << p.y() << ", " << p.z() << std::endl;
    }

    RayTracer rayTracer(&pixelBuffer, &scene);

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
                threads.emplace_back(std::thread([&]() {
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
                }));
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
