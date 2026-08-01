#include "RayTracer/RayTracer.h"
#include "Scene/Scene.h"
#include "Utils/Config.h"
#include "Utils/ImageIO.h"
#include "Utils/Random.h"
#include "Utils/Stats.h"
#include "Utils/ThreadPool.h"
#include "Window/PixelBuffer.h"

#ifdef PT_HAVE_VIEWER
#include "Window/CameraController.h"
#include "Window/Window.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
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
              << "for backwards compatibility and mean <config> <scene>.\n"
              << "\n"
              << "Without --out, an interactive viewer opens and refines the image as long\n"
              << "as the camera is left still. Controls:\n"
              << "  W A S D            Move forward, left, back, right\n"
              << "  Q E                Move down, up\n"
              << "  Shift (held)       Move four times faster\n"
              << "  Left mouse drag    Look around\n"
              << "  [ ]                Decrease / increase movement speed\n"
              << "  R                  Return to the camera the scene file specifies\n"
              << "  Esc                Quit\n"
              << "\n"
              << "Examples:\n"
              << "  Interactive:  steradian --config res/configs/test_config.json \\\n"
              << "                          --scene res/scenes/cornell_box.json\n"
              << "  To a file:    steradian --config res/configs/test_config.json \\\n"
              << "                          --scene res/scenes/cornell_box.json \\\n"
              << "                          --out render.png --samples 512\n";
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
void renderHeadless(RayTracer &rayTracer, PixelBuffer &pixelBuffer, const Options &options, ThreadPool &pool)
{
    const auto size = pixelBuffer.getSize();
    const int height = size.second;

    // Work is handed out in small bands rather than one equal share per thread. Bands
    // differ enormously in cost -- empty background is far cheaper than a band full of
    // geometry -- so a static division leaves most threads waiting on the slowest one.
    // Claiming bands dynamically keeps every worker busy until the frame is done.
    constexpr int rowsPerBand = 8;
    const auto bandCount = static_cast<uint32_t>((height + rowsPerBand - 1) / rowsPerBand);

    pool.parallelFor(bandCount, [&](uint32_t band) {
        const int yStart = static_cast<int>(band) * rowsPerBand;
        const int yEnd = std::min(yStart + rowsPerBand, height);

        rayTracer.renderRows(yStart, yEnd, options.samplesPerPixel, options.seed);
    });
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
            scene.createAcceleratedStructure(config.numChildrenInBVHLeafNodes);

            RayTracer rayTracer(&pixelBuffer, &scene, config);

            std::cout << "Rendering " << config.windowWidth << "x" << config.windowHeight << " at "
                      << options.samplesPerPixel << " spp on " << numThreads << " thread(s), seed " << options.seed
                      << "..." << std::endl;

            ThreadPool pool(numThreads);

            const auto start = std::chrono::steady_clock::now();
            renderHeadless(rayTracer, pixelBuffer, options, pool);
            const auto end = std::chrono::steady_clock::now();

            const double seconds = std::chrono::duration<double>(end - start).count();
            const double samples = static_cast<double>(config.windowWidth) * static_cast<double>(config.windowHeight) *
                                   static_cast<double>(options.samplesPerPixel);

            std::cout << "Rendered in " << seconds << " s (" << (samples / seconds / 1e6) << " M primary samples/sec)"
                      << std::endl;

            if (Stats::enabled())
            {
                const Stats::Counters counters = Stats::total();
                const auto perRay = [&](uint64_t n) {
                    return (counters.rays > 0) ? static_cast<double>(n) / static_cast<double>(counters.rays) : 0.0;
                };

                std::cout << "  rays traced      " << counters.rays << " (" << (counters.rays / seconds / 1e6)
                          << " M/s)\n"
                          << "  node visits      " << counters.nodeVisits << " (" << perRay(counters.nodeVisits)
                          << " per ray)\n"
                          << "  primitive tests  " << counters.primitiveTests << " (" << perRay(counters.primitiveTests)
                          << " per ray)" << std::endl;
            }

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
        Window window("Steradian", config.windowWidth, config.windowHeight);

        // framebuffer size sometimes differs from window size
        auto fbSize = window.getFrameBufferSize();
        PixelBuffer pixelBuffer(fbSize.first, fbSize.second);
        window.setPixelBuffer(&pixelBuffer);

        Scene scene(options.scenePath);
        scene.createAcceleratedStructure(config.numChildrenInBVHLeafNodes);

        RayTracer rayTracer(&pixelBuffer, &scene, config);
        window.setRayTracer(&rayTracer);

        ThreadPool pool(numThreads);

        const Camera initialCamera = scene.getCamera();
        Camera camera = initialCamera;
        CameraController controller(camera);

        std::cout << "Viewer controls:\n"
                  << "  W A S D      move, Q E down/up, hold Shift to move faster\n"
                  << "  left drag    look around\n"
                  << "  [ ]          decrease / increase movement speed\n"
                  << "  R            return to the scene's camera\n"
                  << "  Esc          quit" << std::endl;

        // Accumulated samples per pixel. The image is refined progressively: each frame
        // adds one more sample everywhere and the buffer keeps the running average, so
        // the picture converges the longer the camera is left alone.
        unsigned int accumulated = 0;

        double lastFrameTime = glfwGetTime();
        double lastTitleUpdate = lastFrameTime;

        while (!window.shouldClose())
        {
            const double now = glfwGetTime();
            const auto deltaTime = static_cast<float>(now - lastFrameTime);
            lastFrameTime = now;

            bool restart = window.consumeResized();

            if (window.resetRequested())
            {
                camera = initialCamera;
                controller = CameraController(camera);
                restart = true;
            }

            if (controller.update(window.getContext(), camera, deltaTime))
            {
                restart = true;
            }

            scene.setCamera(camera);

            // Samples average light arriving at one viewpoint, so they are only valid for
            // the camera that took them. Moving invalidates every one of them at once.
            if (restart)
            {
                pixelBuffer.clearBuffer();
                accumulated = 0;
            }

            const auto curSize = window.getFrameBufferSize();

            if (curSize.first > 0 && curSize.second > 0)
            {
                const int height = curSize.second;

                constexpr int rowsPerBand = 8;
                const auto bandCount = static_cast<uint32_t>((height + rowsPerBand - 1) / rowsPerBand);

                // Vary the seed per pass so each one contributes new samples rather than
                // repeating the previous pass exactly.
                const uint64_t passSeed = options.seed + (static_cast<uint64_t>(accumulated) * 0x9e3779b97f4a7c15ULL);

                pool.parallelFor(bandCount, [&](uint32_t band) {
                    const int yStart = static_cast<int>(band) * rowsPerBand;
                    const int yEnd = std::min(yStart + rowsPerBand, height);

                    rayTracer.renderRows(yStart, yEnd, 1, passSeed);
                });

                accumulated++;
            }

            window.update();

            // Retitling on every frame is a synchronous call into the window system, so
            // it is limited to a few times a second.
            if (now - lastTitleUpdate > 0.25)
            {
                const double fps = (deltaTime > 0.0f) ? (1.0 / deltaTime) : 0.0;

                std::ostringstream title;
                title.precision(1);
                title << std::fixed << "Steradian - " << accumulated << " spp - " << fps << " fps - speed "
                      << controller.getSpeed();

                window.setTitle(title.str());
                lastTitleUpdate = now;
            }
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
