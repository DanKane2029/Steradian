#include "RayTracer/RayTracer.h"
#include "Scene/Scene.h"
#include "Utils/Config.h"
#include "Utils/ImageIO.h"
#include "Utils/Random.h"
#include "Window/PixelBuffer.h"

#ifdef PT_HAVE_VIEWER
#include "Window/Window.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

struct Options
{
    std::string configPath;
    std::string scenePath;
    std::string outputPath;

    unsigned int samplesPerPixel = 16;
    uint64_t seed = 1;
    unsigned int numThreads = 0; // 0 means "use the value from the config file"

    bool headless = false;
    bool showHelp = false;
};

void printUsage(const char *program)
{
    std::cout << "Usage: " << program << " [options] [<config.json> <scene.json>]\n"
              << "\n"
              << "Options:\n"
              << "  --config <path>    Config JSON file (required)\n"
              << "  --scene <path>     Scene JSON file (required)\n"
              << "  --out <path>       Write the render to a PNG file\n"
              << "  --samples <n>      Samples per pixel for a headless render (default 16)\n"
              << "  --seed <n>         Base seed; the same seed and sample count reproduce\n"
              << "                     the same image regardless of thread count (default 1)\n"
              << "  --threads <n>      Worker threads (default: numThreads from the config)\n"
              << "  --headless         Render once and exit without opening a window\n"
              << "  --help             Show this message\n"
              << "\n"
              << "Passing --out implies --headless. The two positional arguments are kept\n"
              << "for backwards compatibility and mean <config> <scene>.\n";
}

/**
 * reads the value that follows a flag, reporting a clear error if it is missing
 */
auto takeValue(int argc, char *argv[], int &i, const char *flag) -> std::string
{
    if (i + 1 >= argc)
    {
        throw std::invalid_argument(std::string("Missing value after ") + flag);
    }

    i++;
    return argv[i];
}

auto parseArgs(int argc, char *argv[]) -> Options
{
    Options options;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; i++)
    {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            options.showHelp = true;
        }
        else if (arg == "--headless")
        {
            options.headless = true;
        }
        else if (arg == "--config")
        {
            options.configPath = takeValue(argc, argv, i, "--config");
        }
        else if (arg == "--scene")
        {
            options.scenePath = takeValue(argc, argv, i, "--scene");
        }
        else if (arg == "--out")
        {
            options.outputPath = takeValue(argc, argv, i, "--out");
        }
        else if (arg == "--samples")
        {
            options.samplesPerPixel = static_cast<unsigned int>(std::stoul(takeValue(argc, argv, i, "--samples")));
        }
        else if (arg == "--seed")
        {
            options.seed = std::stoull(takeValue(argc, argv, i, "--seed"));
        }
        else if (arg == "--threads")
        {
            options.numThreads = static_cast<unsigned int>(std::stoul(takeValue(argc, argv, i, "--threads")));
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            throw std::invalid_argument("Unknown option: " + arg);
        }
        else
        {
            positional.push_back(arg);
        }
    }

    // Backwards compatible positional form: <config> <scene>
    if (options.configPath.empty() && !positional.empty())
    {
        options.configPath = positional[0];
    }
    if (options.scenePath.empty() && positional.size() > 1)
    {
        options.scenePath = positional[1];
    }

    // Asking for a file implies a headless render.
    if (!options.outputPath.empty())
    {
        options.headless = true;
    }

    if (options.samplesPerPixel == 0)
    {
        throw std::invalid_argument("--samples must be at least 1");
    }

    return options;
}

/**
 * renders the whole image deterministically across a fixed number of worker threads
 *
 * The image is split into contiguous bands of rows, each owned exclusively by one
 * thread, so no locking is needed on the pixel buffer. Seeding is per row rather than
 * per thread, which is what keeps the result identical no matter how many threads are
 * used.
 */
void renderHeadless(RayTracer &rayTracer, PixelBuffer &pixelBuffer, const Options &options, unsigned int numThreads)
{
    const auto size = pixelBuffer.getSize();
    const int height = size.second;

    const unsigned int workerCount = std::max(1U, std::min(numThreads, static_cast<unsigned int>(height)));

    std::vector<std::thread> workers;
    workers.reserve(workerCount);

    const int rowsPerWorker = (height + static_cast<int>(workerCount) - 1) / static_cast<int>(workerCount);

    for (unsigned int w = 0; w < workerCount; w++)
    {
        const int yStart = static_cast<int>(w) * rowsPerWorker;
        const int yEnd = std::min(yStart + rowsPerWorker, height);

        if (yStart >= yEnd)
        {
            break;
        }

        workers.emplace_back([&rayTracer, yStart, yEnd, &options]() {
            rayTracer.renderRows(yStart, yEnd, options.samplesPerPixel, options.seed);
        });
    }

    for (std::thread &worker : workers)
    {
        worker.join();
    }
}

} // namespace

auto main(int argc, char *argv[]) -> int
{
    try
    {
        const Options options = parseArgs(argc, argv);

        if (options.showHelp)
        {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        }

        if (options.configPath.empty() || options.scenePath.empty())
        {
            std::cerr << "Error: both a config file and a scene file are required.\n\n";
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }

        Config config(options.configPath);

        const unsigned int numThreads = (options.numThreads > 0) ? options.numThreads : config.numThreads;

#ifndef PT_HAVE_VIEWER
        if (!options.headless)
        {
            std::cerr << "This build has no viewer (configured without GLFW/OpenGL).\n"
                      << "Re-run with --headless --out <file.png>.\n";
            return EXIT_FAILURE;
        }
#endif

        if (options.headless)
        {
            PixelBuffer pixelBuffer(config.windowWidth, config.windowHeight);

            Scene scene(options.scenePath);
            scene.createAcceleratedStructure();

            RayTracer rayTracer(&pixelBuffer, &scene, config);

            std::cout << "Rendering " << config.windowWidth << "x" << config.windowHeight << " at "
                      << options.samplesPerPixel << " spp on " << numThreads << " thread(s), seed " << options.seed
                      << "..." << std::endl;

            const auto start = std::chrono::steady_clock::now();
            renderHeadless(rayTracer, pixelBuffer, options, numThreads);
            const auto end = std::chrono::steady_clock::now();

            const double seconds = std::chrono::duration<double>(end - start).count();
            const double samples = static_cast<double>(config.windowWidth) * static_cast<double>(config.windowHeight) *
                                   static_cast<double>(options.samplesPerPixel);

            std::cout << "Rendered in " << seconds << " s (" << (samples / seconds / 1e6)
                      << " M primary samples/sec)" << std::endl;

            if (!options.outputPath.empty())
            {
                if (!ImageIO::writePNG(options.outputPath, config.windowWidth, config.windowHeight,
                                       pixelBuffer.getPixels()))
                {
                    std::cerr << "Error: failed to write " << options.outputPath << std::endl;
                    return EXIT_FAILURE;
                }

                std::cout << "Wrote " << options.outputPath << std::endl;
            }

            return EXIT_SUCCESS;
        }

#ifdef PT_HAVE_VIEWER
        Window window("Path Tracer", config.windowWidth, config.windowHeight);

        // framebuffer size sometimes differs from window size
        auto fbSize = window.getFrameBufferSize();
        PixelBuffer pixelBuffer(fbSize.first, fbSize.second);
        window.setPixelBuffer(&pixelBuffer);

        Scene scene(options.scenePath);
        scene.createAcceleratedStructure();

        RayTracer rayTracer(&pixelBuffer, &scene, config);
        window.setRayTracer(&rayTracer);

        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        while (!window.shouldClose())
        {
            auto curSize = window.getFrameBufferSize();

            if (curSize.first > 0 && curSize.second > 0)
            {
                for (unsigned int i = 0; i < numThreads; i++)
                {
                    threads.emplace_back([&rayTracer, &config]() {
                        // Each worker owns its generator; a shared engine here was a
                        // data race.
                        static thread_local Rng rng(0x243f6a8885a308d3ULL, reinterpret_cast<uintptr_t>(&rng));

                        double curTime = glfwGetTime();
                        const double endingTime = curTime + (1.0 / static_cast<double>(config.fps));

                        // shoot rays until ready to display next frame
                        while (curTime < endingTime)
                        {
                            rayTracer.sampleScene(rng.nextFloat(), rng.nextFloat());
                            curTime = glfwGetTime();
                        }
                    });
                }
            }

            for (std::thread &t : threads)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }

            // Threads are joined and finished; drop them so the vector does not grow
            // without bound for the lifetime of the program.
            threads.clear();

            window.update();
        }

        return EXIT_SUCCESS;
#endif
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
