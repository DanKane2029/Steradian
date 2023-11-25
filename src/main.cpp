#include "Window/Window.h"
#include "Window/PixelBuffer.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "RayTracer/RayTracer.h"
#include "Utils/ObjModel.h"

#include <iostream>
#include <random>
#include <thread>
#include <algorithm>
#include <functional>
#include <memory>

int main()
{
	// initial size of window
	const unsigned int width = 800;
	const unsigned int height = 800;

	Window window("Path Tracer", width, height);

	// framebuffer size sometimes different than window size
	auto fbSize = window.GetFrameBufferSize();
	PixelBuffer pixelBuffer(fbSize.first, fbSize.second);
	window.SetPixelBuffer(&pixelBuffer);

	// create scene
	Scene scene;

	Material red(
		"Red",
		Vec3(0.8f, 0.1f, 0.3f),
		0.75f,
		0.2f,
		0.25f,
		3.0f
	);
	scene.RegisterMaterial(&red);

	ObjModel objModel("res/models/lowpolytree.obj");
	std::vector<std::shared_ptr<SceneObject>> modelObjects = objModel.getSceneObjects();
	scene.AddObjects(modelObjects, "Red");

	// Sphere s(Vec3(0, 0, 0), 1);
	// scene.AddObject(std::make_shared<Sphere>(s), "Red");

	Camera camera({0.2f, 0.2f, 2.0f}, {0.0f, 0.0f, 0.0f});

	scene.SetAmbientLighting({0.2f, 0.2f, 0.2f});

	PointLight light(
		{0.5f, 0.5f, 1.0f},
		{0.75f, 0.75f, 0.75f},
		0.8f,
		0.25f
	);
	scene.AddLight(&light);

	scene.CreateAcceleratedStructure();
	// end scene

	RayTracer rayTracer(&pixelBuffer, &scene, camera);

	// framerate determines how much time is given ray shooting
	double FPS = 30;
	double secondsPerFrame = 1.0 / FPS;

	// set up thread safe random number generator [0.0, 1.0)
	std::default_random_engine randomGenerator;
	std::uniform_real_distribution<float> dist(0.0, 1.0);

	// set up thread list for ray shooting
	const unsigned int NUM_THREADS = std::thread::hardware_concurrency();
	std::vector<std::thread> threads(NUM_THREADS);
	std::mutex consoleMutex;

	// ray shooting meta data
	unsigned int totalSampleCount = 0;
	std::mutex totalSampleCountMutex;

	// application loop
	while (!window.ShouldClose())
	{
		auto curSize = window.GetFrameBufferSize();
		unsigned int curWidth = curSize.first;
		unsigned int curHeight = curSize.second;

		// initialize threads for ray shooting
		for (size_t i = 0; i < NUM_THREADS; i++)
		{
			if (curWidth > 0 && curHeight > 0)
			{
				threads[i] = std::thread(
					[&]() {
						double curTime = glfwGetTime();
						double endingTime = curTime + secondsPerFrame;
						int sampleCount = 0;

						// shoot rays until ready to display next frame
						while (curTime < endingTime)
						{
							float x = dist(randomGenerator);
							float y = dist(randomGenerator);
							rayTracer.SampleScene(x, y);
							curTime = glfwGetTime();
							sampleCount++;
						}

						totalSampleCountMutex.lock();
						totalSampleCount += sampleCount;
						totalSampleCountMutex.unlock();
					}
				);
			}
		}

		// wait for all threads to finish
		for (std::thread &t : threads)
		{
			if (t.joinable())
				t.join();
		}

		window.Update();
	}
}
