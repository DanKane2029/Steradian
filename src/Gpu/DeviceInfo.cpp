#include "DeviceInfo.h"

#include <cstdio>

#include <cuda.h>
#include <nvrtc.h>

#include <optix.h>
#include <optix_function_table_definition.h>
#include <optix_stubs.h>

namespace
{

/**
 * reports a driver API failure with the message the driver itself gives
 *
 * Every one of these calls can fail for a reason worth reading -- no device, a driver
 * mismatch, insufficient permissions -- and a bare error code sends the reader to a
 * header to look it up.
 */
auto check(CUresult status, const char *what) -> bool
{
    if (status == CUDA_SUCCESS)
    {
        return true;
    }

    const char *message = nullptr;
    cuGetErrorString(status, &message);

    std::printf("  %s failed: %s\n", what, (message != nullptr) ? message : "unknown error");
    return false;
}

auto attribute(CUdevice device, CUdevice_attribute which) -> int
{
    int value = 0;
    cuDeviceGetAttribute(&value, which, device);

    return value;
}

/** OptiX reports its own diagnostics through a callback rather than return codes */
void logCallback(unsigned int level, const char *tag, const char *message, void * /*data*/)
{
    std::printf("    [optix %u] %s: %s\n", level, tag, message);
}

/**
 * prints what the driver and the device say about themselves
 *
 * The choice of what to print is not arbitrary. Compute capability decides what NVRTC is
 * asked to target; resident threads is the number that actually bounds how many rays can
 * be in flight, and so how much memory latency the scheduler has to hide; and creating a
 * context proves the device is usable by this process rather than merely enumerable,
 * which is the distinction that matters when something is wrong.
 */
auto reportDevice(CUdevice device, CUcontext &context) -> bool
{
    char name[256] = {};
    cuDeviceGetName(name, sizeof(name), device);

    size_t memory = 0;
    cuDeviceTotalMem(&memory, device);

    const int multiprocessors = attribute(device, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT);

    std::printf("  %s\n", name);
    std::printf("    compute capability   %d.%d\n", attribute(device, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR),
                attribute(device, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR));
    std::printf("    memory               %.1f GiB, %d-bit bus at %.1f GHz\n",
                static_cast<double>(memory) / (1024.0 * 1024.0 * 1024.0),
                attribute(device, CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH),
                attribute(device, CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE) / 1e6);
    std::printf("    L2 cache             %.1f MiB\n",
                attribute(device, CU_DEVICE_ATTRIBUTE_L2_CACHE_SIZE) / (1024.0 * 1024.0));
    std::printf("    multiprocessors      %d\n", multiprocessors);
    std::printf("    threads per block    %d\n", attribute(device, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK));
    std::printf("    shared memory/block  %d KiB\n",
                attribute(device, CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK) / 1024);

    // Ampere runs up to 48 warps of 32 threads per multiprocessor. This is the honest
    // measure of how many paths can genuinely be in flight at once.
    const int warpsPerMultiprocessor = attribute(device, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR) / 32;
    std::printf("    resident threads     %d\n", multiprocessors * warpsPerMultiprocessor * 32);

    if (!check(cuCtxCreate(&context, 0, device), "cuCtxCreate"))
    {
        return false;
    }

    std::printf("    context              created\n");
    return true;
}

/**
 * prints what the OptiX implementation reports about itself
 *
 * optixInit is the step that either finds the driver's implementation or does not. There
 * is no SDK to install: libnvoptix.so.1 arrives with the display driver, and these
 * headers are the only part that has to be fetched.
 */
auto reportOptix(CUcontext cuContext) -> bool
{
    std::printf("\nOptiX\n");
    std::printf("    headers              %d.%d.%d\n", OPTIX_VERSION / 10000, (OPTIX_VERSION % 10000) / 100,
                OPTIX_VERSION % 100);

    if (const OptixResult status = optixInit(); status != OPTIX_SUCCESS)
    {
        std::printf("    optixInit failed: %d\n", status);
        std::printf("\n  The implementation ships with the NVIDIA display driver as\n"
                    "  libnvoptix.so.1. A driver too old for these headers, or a system\n"
                    "  without one, both land here.\n");
        return false;
    }

    OptixDeviceContextOptions options{};
    options.logCallbackFunction = &logCallback;

    // Errors and warnings only. Level 4 adds a running commentary about knob defaults
    // and the disk cache, which says nothing about whether the machine works and
    // interleaves itself with the properties below.
    options.logCallbackLevel = 2;

    OptixDeviceContext context = nullptr;
    if (const OptixResult status = optixDeviceContextCreate(cuContext, &options, &context); status != OPTIX_SUCCESS)
    {
        std::printf("    optixDeviceContextCreate failed: %d\n", status);
        return false;
    }

    const auto property = [&](OptixDeviceProperty which, const char *label) {
        unsigned int value = 0;
        if (optixDeviceContextGetProperty(context, which, &value, sizeof(value)) == OPTIX_SUCCESS)
        {
            std::printf("    %-20s %u\n", label, value);
        }
    };

    // RT core version is the one that decides the whole approach: it is the fixed
    // function ray/box and ray/triangle hardware, and traversal is where this renderer
    // spends its time. Zero would mean software traversal and a much weaker case for
    // targeting OptiX at all.
    property(OPTIX_DEVICE_PROPERTY_RTCORE_VERSION, "RT core version");
    property(OPTIX_DEVICE_PROPERTY_LIMIT_MAX_TRACE_DEPTH, "max trace depth");
    property(OPTIX_DEVICE_PROPERTY_LIMIT_MAX_TRAVERSABLE_GRAPH_DEPTH, "max graph depth");
    property(OPTIX_DEVICE_PROPERTY_LIMIT_MAX_PRIMITIVES_PER_GAS, "max primitives/GAS");
    property(OPTIX_DEVICE_PROPERTY_LIMIT_MAX_INSTANCES_PER_IAS, "max instances/IAS");

    optixDeviceContextDestroy(context);
    return true;
}

/**
 * prints the NVRTC version
 *
 * Device code is compiled from source at run time rather than by nvcc, so NVRTC is not
 * an optional extra here: without it there is nothing to turn the integrator into PTX.
 * Reporting it makes a half-installed toolchain visible before it fails obscurely.
 */
void reportNvrtc()
{
    int major = 0;
    int minor = 0;

    std::printf("\nNVRTC\n");

    if (nvrtcVersion(&major, &minor) == NVRTC_SUCCESS)
    {
        std::printf("    version              %d.%d  (device code is compiled at run time, so no nvcc)\n", major,
                    minor);
    }
    else
    {
        std::printf("    unavailable\n");
    }
}

} // namespace

auto Gpu::printDeviceInfo() -> bool
{
    if (!check(cuInit(0), "cuInit"))
    {
        std::printf("\n  The driver API lives in libcuda.so.1 and ships with the NVIDIA\n"
                    "  display driver. This build linked against it, so it is present, but\n"
                    "  it could not be initialised.\n");
        return false;
    }

    int driverVersion = 0;
    cuDriverGetVersion(&driverVersion);

    std::printf("CUDA driver API %d.%d\n", driverVersion / 1000, (driverVersion % 1000) / 10);

    int deviceCount = 0;
    cuDeviceGetCount(&deviceCount);

    if (deviceCount == 0)
    {
        std::printf("  no CUDA capable devices found\n");
        return false;
    }

    std::printf("devices         %d\n\n", deviceCount);

    // Device 0 only. Multi-GPU is not something the backend plans to use, and reporting
    // on a device the renderer would not pick would be misleading rather than thorough.
    CUdevice device = 0;
    if (!check(cuDeviceGet(&device, 0), "cuDeviceGet"))
    {
        return false;
    }

    CUcontext context = nullptr;
    if (!reportDevice(device, context))
    {
        return false;
    }

    const bool optixReady = reportOptix(context);
    reportNvrtc();

    cuCtxDestroy(context);

    std::printf("\n%s\n", optixReady ? "Ready: the device, OptiX and NVRTC are all usable here."
                                     : "Not ready: OptiX could not be initialised (see above).");

    return optixReady;
}
