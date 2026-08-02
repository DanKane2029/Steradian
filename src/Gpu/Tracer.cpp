#include "Tracer.h"

#include "Gpu/Nvrtc.h"

#include "Utils/Microfacet.h"

#include <cmath>
#include <cstring>
#include <sstream>
#include <vector>

#include <cuda.h>
#include <nvrtc.h>

#include <optix.h>
#include <optix_stack_size.h>
#include <optix_stubs.h>

#include "DeviceSources.h" // generated: the device code, embedded

namespace
{

/** \brief Records the first thing that went wrong, so the caller learns the cause. */
class ErrorSink
{
  public:
    explicit ErrorSink(std::string &target) : m_Target(&target)
    {
    }

    auto cuda(CUresult status, const char *what) -> bool
    {
        if (status == CUDA_SUCCESS)
        {
            return true;
        }

        const char *message = nullptr;
        cuGetErrorString(status, &message);
        set(std::string(what) + ": " + ((message != nullptr) ? message : "unknown CUDA error"));

        return false;
    }

    auto optix(OptixResult status, const char *what) -> bool
    {
        if (status == OPTIX_SUCCESS)
        {
            return true;
        }

        set(std::string(what) + ": " + optixGetErrorName(status));
        return false;
    }

    void set(std::string message)
    {
        if (m_Target->empty())
        {
            *m_Target = std::move(message);
        }
    }

  private:
    std::string *m_Target;
};

/** \brief A shader binding table record: the required header, and nothing after it. */
template <typename T> struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) SbtRecord
{
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    T data;
};

struct EmptyData
{
    int unused;
};

using EmptyRecord = SbtRecord<EmptyData>;

/** \brief Copies a host array into a fresh device allocation. */
template <typename T> auto upload(const T *data, size_t count, CUdeviceptr &out) -> bool
{
    const size_t bytes = count * sizeof(T);

    if (bytes == 0)
    {
        out = 0;
        return true;
    }

    if (cuMemAlloc(&out, bytes) != CUDA_SUCCESS)
    {
        return false;
    }

    return cuMemcpyHtoD(out, data, bytes) == CUDA_SUCCESS;
}

template <typename T> auto upload(const std::vector<T> &data, CUdeviceptr &out) -> bool
{
    return upload(data.data(), data.size(), out);
}

} // namespace

namespace Gpu
{

namespace
{

/** \brief Fills the part of the launch that every launch shape shares. */
template <typename StateType> void fillSceneParams(const StateType &state, LaunchParams &params)
{
    params.handle = state.handle;

    params.geometry.positions = reinterpret_cast<const Vec3 *>(state.vertices);
    params.geometry.normals = reinterpret_cast<const Vec3 *>(state.normals);
    params.geometry.texCoords = reinterpret_cast<const Vec3 *>(state.texCoords);
    params.geometry.triangles = reinterpret_cast<const Triangle *>(state.triangles);
    params.geometry.spheres = reinterpret_cast<const Sphere *>(state.spheres);
    params.geometry.triangleCount = state.triangleCount;
    params.geometry.sphereCount = state.sphereCount;

    params.materials = reinterpret_cast<const Material *>(state.materials);
    params.emitters = reinterpret_cast<const Emitter *>(state.emitters);
    params.emitterCount = state.emitterCount;
    params.albedoTable = reinterpret_cast<const float *>(state.albedoTable);
    params.textures = reinterpret_cast<const DeviceTexture *>(state.textures);
    params.texturePixels = reinterpret_cast<const float *>(state.texturePixels);
    params.ambient = state.ambient;
}

} // namespace

/**
 * \brief Everything the tracer owns on the device.
 *
 * Kept out of the header so that including Tracer.h does not require the OptiX and CUDA
 * headers, which only this file and the SDK-aware targets have.
 */
struct Tracer::State
{
    CUcontext cuda = nullptr;
    OptixDeviceContext optix = nullptr;

    OptixModule module = nullptr;
    OptixProgramGroup raygenGroup = nullptr;
    OptixProgramGroup renderGroup = nullptr;
    OptixProgramGroup missGroup = nullptr;
    OptixProgramGroup triangleGroup = nullptr;
    OptixProgramGroup sphereGroup = nullptr;
    OptixPipeline pipeline = nullptr;

    OptixShaderBindingTable sbt = {};

    // Acceleration structures, and the buffers they were built over. OptiX keeps
    // referring to the vertex and AABB data, so none of it can be freed after the build.
    CUdeviceptr vertices = 0;
    CUdeviceptr indices = 0;
    CUdeviceptr aabbs = 0;
    CUdeviceptr triangleGas = 0;
    CUdeviceptr sphereGas = 0;
    CUdeviceptr instances = 0;
    CUdeviceptr ias = 0;

    CUdeviceptr spheres = 0;
    CUdeviceptr triangles = 0;
    CUdeviceptr normals = 0;
    CUdeviceptr texCoords = 0;
    CUdeviceptr materials = 0;
    CUdeviceptr emitters = 0;
    CUdeviceptr albedoTable = 0;
    CUdeviceptr textures = 0;
    CUdeviceptr texturePixels = 0;
    CUdeviceptr sbtRecords = 0;

    CUdeviceptr film = 0;
    CUdeviceptr filmAlbedo = 0;
    CUdeviceptr filmNormal = 0;
    size_t filmCapacity = 0;

    // Ray and result buffers, grown as needed and reused between launches.
    CUdeviceptr rayOrigins = 0;
    CUdeviceptr rayDirections = 0;
    CUdeviceptr hits = 0;
    CUdeviceptr launchParams = 0;
    size_t rayCapacity = 0;

    OptixTraversableHandle handle = 0;
    unsigned int triangleCount = 0;
    unsigned int sphereCount = 0;
    unsigned int emitterCount = 0;
    Vec3 ambient;
    Camera camera;

    ~State()
    {
        if (pipeline != nullptr)
        {
            optixPipelineDestroy(pipeline);
        }
        for (OptixProgramGroup group : {raygenGroup, renderGroup, missGroup, triangleGroup, sphereGroup})
        {
            if (group != nullptr)
            {
                optixProgramGroupDestroy(group);
            }
        }
        if (module != nullptr)
        {
            optixModuleDestroy(module);
        }
        if (optix != nullptr)
        {
            optixDeviceContextDestroy(optix);
        }

        for (CUdeviceptr pointer :
             {vertices,   indices,    aabbs,         triangleGas, sphereGas,    instances,   ias,        spheres,
              triangles,  normals,    texCoords,     materials,   emitters,     albedoTable, textures,   texturePixels,
              sbtRecords, rayOrigins, rayDirections, hits,        launchParams, film,        filmAlbedo, filmNormal})
        {
            if (pointer != 0)
            {
                cuMemFree(pointer);
            }
        }

        if (cuda != nullptr)
        {
            cuCtxDestroy(cuda);
        }
    }
};

namespace
{

/**
 * \brief Compiles the embedded device sources to PTX.
 *
 * The sources are baked into the binary rather than read from disk, so a built renderer
 * does not depend on the checkout it came from. Only the OptiX headers come from the
 * filesystem, through an include path, since they are a fetched dependency already and
 * embedding a few hundred kilobytes of them would be silly.
 */
auto compileToPtx(int computeMajor, int computeMinor, std::string &ptx, ErrorSink &errors) -> bool
{
    prepareNvrtc();

    const EmbeddedSource *kernel = nullptr;
    std::vector<const char *> headerSources;
    std::vector<const char *> headerNames;

    for (int i = 0; i < embeddedSourceCount; i++)
    {
        const EmbeddedSource &source = embeddedSources[i];

        // The .cu is the translation unit; everything else is included by name.
        if (std::strstr(source.name, ".cu") != nullptr)
        {
            kernel = &source;
        }
        else
        {
            headerSources.push_back(source.source);
            headerNames.push_back(source.name);
        }
    }

    if (kernel == nullptr)
    {
        errors.set("no device kernel was embedded in this build");
        return false;
    }

    nvrtcProgram program{};
    if (nvrtcCreateProgram(&program, kernel->source, kernel->name, static_cast<int>(headerSources.size()),
                           headerSources.data(), headerNames.data()) != NVRTC_SUCCESS)
    {
        errors.set("nvrtcCreateProgram failed");
        return false;
    }

    std::ostringstream architecture;
    architecture << "--gpu-architecture=compute_" << computeMajor << computeMinor;

    const std::string architectureOption = architecture.str();
    const std::string includeOption = std::string("-I") + PT_GPU_SDK_INCLUDE_DIR;

    // -rdc and -default-device are required by OptiX: its programs are relocatable device
    // code, and everything in the translation unit belongs on the device.
    //
    // No --use_fast_math. It would swap the elementary functions for cheaper
    // approximations and widen the gap against the CPU that the tests measure.
    const char *options[] = {
        architectureOption.c_str(), includeOption.c_str(), "--std=c++17", "-rdc=true", "-default-device",
    };

    const nvrtcResult compiled = nvrtcCompileProgram(program, 5, options);

    size_t logSize = 0;
    nvrtcGetProgramLogSize(program, &logSize);

    std::string log;
    if (logSize > 1)
    {
        log.resize(logSize);
        nvrtcGetProgramLog(program, log.data());
    }

    if (compiled != NVRTC_SUCCESS)
    {
        errors.set("the device kernel did not compile:\n" + log);
        nvrtcDestroyProgram(&program);
        return false;
    }

    size_t ptxSize = 0;
    nvrtcGetPTXSize(program, &ptxSize);
    ptx.resize(ptxSize);
    nvrtcGetPTX(program, ptx.data());

    nvrtcDestroyProgram(&program);
    return true;
}

/**
 * \brief Builds one acceleration structure and returns a handle to it.
 *
 * The scratch space OptiX asks for is freed straight away; the output is not, since the
 * handle points into it.
 */
auto buildAccel(OptixDeviceContext context, const OptixBuildInput &input, CUdeviceptr &output,
                OptixTraversableHandle &handle, ErrorSink &errors) -> bool
{
    OptixAccelBuildOptions options = {};
    options.buildFlags = OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
    options.operation = OPTIX_BUILD_OPERATION_BUILD;

    OptixAccelBufferSizes sizes = {};
    if (!errors.optix(optixAccelComputeMemoryUsage(context, &options, &input, 1, &sizes),
                      "optixAccelComputeMemoryUsage"))
    {
        return false;
    }

    CUdeviceptr scratch = 0;
    if (!errors.cuda(cuMemAlloc(&scratch, sizes.tempSizeInBytes), "cuMemAlloc (accel scratch)") ||
        !errors.cuda(cuMemAlloc(&output, sizes.outputSizeInBytes), "cuMemAlloc (accel output)"))
    {
        return false;
    }

    const bool built =
        errors.optix(optixAccelBuild(context, nullptr, &options, &input, 1, scratch, sizes.tempSizeInBytes, output,
                                     sizes.outputSizeInBytes, &handle, nullptr, 0),
                     "optixAccelBuild");

    cuMemFree(scratch);
    return built;
}

} // namespace

Tracer::~Tracer() = default;

auto Tracer::create(const Scene &scene, std::string &error) -> std::unique_ptr<Tracer>
{
    const Geometry &geometry = scene.getGeometry();

    error.clear();
    ErrorSink errors(error);

    auto tracer = std::unique_ptr<Tracer>(new Tracer());
    tracer->m_State = std::make_unique<State>();
    State &state = *tracer->m_State;

    state.triangleCount = geometry.triangleCount();
    state.sphereCount = geometry.sphereCount();
    state.emitterCount = static_cast<unsigned int>(scene.getEmitters().size());
    state.ambient = scene.getAmbientLighting();
    state.camera = scene.getCamera();

    // ---- device and contexts ---------------------------------------------------------
    if (!errors.cuda(cuInit(0), "cuInit"))
    {
        return nullptr;
    }

    CUdevice device = 0;
    if (!errors.cuda(cuDeviceGet(&device, 0), "cuDeviceGet") ||
        !errors.cuda(cuCtxCreate(&state.cuda, 0, device), "cuCtxCreate"))
    {
        return nullptr;
    }

    int computeMajor = 0;
    int computeMinor = 0;
    cuDeviceGetAttribute(&computeMajor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device);
    cuDeviceGetAttribute(&computeMinor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device);

    if (!errors.optix(optixInit(), "optixInit"))
    {
        return nullptr;
    }

    OptixDeviceContextOptions contextOptions = {};
    if (!errors.optix(optixDeviceContextCreate(state.cuda, &contextOptions, &state.optix), "optixDeviceContextCreate"))
    {
        return nullptr;
    }

    // ---- the programs ----------------------------------------------------------------
    std::string ptx;
    if (!compileToPtx(computeMajor, computeMinor, ptx, errors))
    {
        return nullptr;
    }

    OptixPipelineCompileOptions pipelineOptions = {};
    pipelineOptions.usesMotionBlur = 0;

    // One instancing level: two acceleration structures, one of triangles and one of
    // spheres, gathered under a single instance array. A structure cannot mix primitive
    // kinds, which is why there are two.
    pipelineOptions.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
    pipelineOptions.numPayloadValues = 4;
    pipelineOptions.numAttributeValues = 2;
    pipelineOptions.pipelineLaunchParamsVariableName = "params";
    pipelineOptions.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE | OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;

    OptixModuleCompileOptions moduleOptions = {};
    moduleOptions.optLevel = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
    moduleOptions.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;

    char log[4096];
    size_t logSize = sizeof(log);

    if (!errors.optix(optixModuleCreate(state.optix, &moduleOptions, &pipelineOptions, ptx.c_str(), ptx.size(), log,
                                        &logSize, &state.module),
                      "optixModuleCreate"))
    {
        errors.set(std::string("optixModuleCreate: ") + log);
        return nullptr;
    }

    const auto makeGroup = [&](OptixProgramGroupDesc desc, OptixProgramGroup &group, const char *what) {
        OptixProgramGroupOptions groupOptions = {};
        logSize = sizeof(log);

        return errors.optix(optixProgramGroupCreate(state.optix, &desc, 1, &groupOptions, log, &logSize, &group), what);
    };

    OptixProgramGroupDesc raygenDesc = {};
    raygenDesc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    raygenDesc.raygen.module = state.module;
    raygenDesc.raygen.entryFunctionName = "__raygen__trace";

    OptixProgramGroupDesc renderDesc = {};
    renderDesc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    renderDesc.raygen.module = state.module;
    renderDesc.raygen.entryFunctionName = "__raygen__render";

    OptixProgramGroupDesc missDesc = {};
    missDesc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    missDesc.miss.module = state.module;
    missDesc.miss.entryFunctionName = "__miss__nothing";

    OptixProgramGroupDesc triangleDesc = {};
    triangleDesc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    triangleDesc.hitgroup.moduleCH = state.module;
    triangleDesc.hitgroup.entryFunctionNameCH = "__closesthit__triangle";

    OptixProgramGroupDesc sphereDesc = {};
    sphereDesc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    sphereDesc.hitgroup.moduleCH = state.module;
    sphereDesc.hitgroup.entryFunctionNameCH = "__closesthit__sphere";
    sphereDesc.hitgroup.moduleIS = state.module;
    sphereDesc.hitgroup.entryFunctionNameIS = "__intersection__sphere";

    if (!makeGroup(raygenDesc, state.raygenGroup, "raygen program group") ||
        !makeGroup(renderDesc, state.renderGroup, "render program group") ||
        !makeGroup(missDesc, state.missGroup, "miss program group") ||
        !makeGroup(triangleDesc, state.triangleGroup, "triangle hit group") ||
        !makeGroup(sphereDesc, state.sphereGroup, "sphere hit group"))
    {
        return nullptr;
    }

    OptixProgramGroup groups[] = {state.raygenGroup, state.renderGroup, state.missGroup, state.triangleGroup,
                                  state.sphereGroup};

    OptixPipelineLinkOptions linkOptions = {};

    // The path loop iterates rather than recursing, and a shadow ray is traced from the
    // raygen program rather than from a hit program, so one level is all that is ever
    // nested.
    linkOptions.maxTraceDepth = 1;

    logSize = sizeof(log);
    if (!errors.optix(
            optixPipelineCreate(state.optix, &pipelineOptions, &linkOptions, groups, 4, log, &logSize, &state.pipeline),
            "optixPipelineCreate"))
    {
        return nullptr;
    }

    OptixStackSizes stackSizes = {};
    for (OptixProgramGroup group : groups)
    {
        if (!errors.optix(optixUtilAccumulateStackSizes(group, &stackSizes, state.pipeline),
                          "optixUtilAccumulateStackSizes"))
        {
            return nullptr;
        }
    }

    unsigned int directFromTraversal = 0;
    unsigned int directFromState = 0;
    unsigned int continuation = 0;
    if (!errors.optix(optixUtilComputeStackSizes(&stackSizes, linkOptions.maxTraceDepth, 0, 0, &directFromTraversal,
                                                 &directFromState, &continuation),
                      "optixUtilComputeStackSizes") ||
        !errors.optix(optixPipelineSetStackSize(state.pipeline, directFromTraversal, directFromState, continuation, 2),
                      "optixPipelineSetStackSize"))
    {
        return nullptr;
    }

    // ---- everything shading reads ----------------------------------------------------
    //
    // Uploaded once. Materials are already a plain block of numbers, so they cross as
    // bytes with no conversion; that was the point of making them one.
    {
        std::vector<DeviceTexture> textureDescriptions;
        std::vector<float> texturePixels;

        for (const Texture &texture : scene.getTextures())
        {
            DeviceTexture description = {};
            description.offset = static_cast<unsigned int>(texturePixels.size());
            description.width = texture.getWidth();
            description.height = texture.getHeight();
            description.channels = texture.getChannels();

            textureDescriptions.push_back(description);
            texturePixels.insert(texturePixels.end(), texture.getData().begin(), texture.getData().end());
        }

        constexpr size_t albedoEntries = Microfacet::albedoResolution * Microfacet::albedoResolution;

        if (!upload(geometry.getNormals(), state.normals) || !upload(geometry.getTexCoords(), state.texCoords) ||
            !upload(geometry.getTriangles(), state.triangles) || !upload(scene.getMaterials(), state.materials) ||
            !upload(scene.getEmitters(), state.emitters) ||
            !upload(Microfacet::hostAlbedoTable(), albedoEntries, state.albedoTable) ||
            !upload(textureDescriptions, state.textures) || !upload(texturePixels, state.texturePixels))
        {
            errors.set("could not upload the scene");
            return nullptr;
        }
    }

    // ---- acceleration structures -----------------------------------------------------
    std::vector<OptixTraversableHandle> children;

    if (state.triangleCount > 0)
    {
        // OptiX wants positions and indices as flat arrays. The vertex buffer is already
        // exactly that; the indices need extracting, because a Triangle carries its
        // material alongside them and OptiX expects three indices with nothing between.
        const std::vector<Triangle> &triangles = geometry.getTriangles();

        std::vector<unsigned int> indices;
        indices.reserve(triangles.size() * 3);
        for (const Triangle &triangle : triangles)
        {
            indices.push_back(triangle.vertex0);
            indices.push_back(triangle.vertex1);
            indices.push_back(triangle.vertex2);
        }

        if (!upload(geometry.getPositions(), state.vertices) || !upload(indices, state.indices))
        {
            errors.set("could not upload the triangle mesh");
            return nullptr;
        }

        OptixBuildInput input = {};
        input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
        input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
        input.triangleArray.vertexStrideInBytes = sizeof(Vec3);
        input.triangleArray.numVertices = geometry.vertexCount();
        input.triangleArray.vertexBuffers = &state.vertices;
        input.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
        input.triangleArray.indexStrideInBytes = 3 * sizeof(unsigned int);
        input.triangleArray.numIndexTriplets = state.triangleCount;
        input.triangleArray.indexBuffer = state.indices;

        // Back faces are hit as readily as front ones, matching the CPU test, which does
        // not cull either.
        const unsigned int flags[1] = {OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT};
        input.triangleArray.flags = flags;
        input.triangleArray.numSbtRecords = 1;

        OptixTraversableHandle handle = 0;
        if (!buildAccel(state.optix, input, state.triangleGas, handle, errors))
        {
            return nullptr;
        }

        children.push_back(handle);
    }

    if (state.sphereCount > 0)
    {
        const std::vector<Sphere> &spheres = geometry.getSpheres();

        std::vector<OptixAabb> boxes;
        boxes.reserve(spheres.size());
        for (const Sphere &sphere : spheres)
        {
            boxes.push_back(OptixAabb{sphere.center.x - sphere.radius, sphere.center.y - sphere.radius,
                                      sphere.center.z - sphere.radius, sphere.center.x + sphere.radius,
                                      sphere.center.y + sphere.radius, sphere.center.z + sphere.radius});
        }

        if (!upload(boxes, state.aabbs) || !upload(spheres, state.spheres))
        {
            errors.set("could not upload the spheres");
            return nullptr;
        }

        OptixBuildInput input = {};
        input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
        input.customPrimitiveArray.aabbBuffers = &state.aabbs;
        input.customPrimitiveArray.numPrimitives = state.sphereCount;

        const unsigned int flags[1] = {OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT};
        input.customPrimitiveArray.flags = flags;
        input.customPrimitiveArray.numSbtRecords = 1;

        OptixTraversableHandle handle = 0;
        if (!buildAccel(state.optix, input, state.sphereGas, handle, errors))
        {
            return nullptr;
        }

        children.push_back(handle);
    }

    if (children.empty())
    {
        errors.set("the scene has no geometry");
        return nullptr;
    }

    // One instance per structure, both at the origin. The instance's SBT offset is what
    // selects the hit group, which is how a triangle ends up in the triangle program and
    // a sphere in its own.
    std::vector<OptixInstance> instances;
    for (size_t i = 0; i < children.size(); i++)
    {
        OptixInstance instance = {};
        instance.instanceId = static_cast<unsigned int>(i);
        instance.visibilityMask = 255;
        instance.traversableHandle = children[i];
        instance.flags = OPTIX_INSTANCE_FLAG_NONE;

        // Which hit group this instance's primitives use. Triangles are built first when
        // present, so the offset follows the order the structures were created in.
        instance.sbtOffset = static_cast<unsigned int>(i);

        const float identity[12] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
        std::memcpy(instance.transform, identity, sizeof(identity));

        instances.push_back(instance);
    }

    if (!upload(instances, state.instances))
    {
        errors.set("could not upload the instance array");
        return nullptr;
    }

    OptixBuildInput instanceInput = {};
    instanceInput.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
    instanceInput.instanceArray.instances = state.instances;
    instanceInput.instanceArray.numInstances = static_cast<unsigned int>(instances.size());

    if (!buildAccel(state.optix, instanceInput, state.ias, state.handle, errors))
    {
        return nullptr;
    }

    // ---- shader binding table --------------------------------------------------------
    //
    // Four records, none of which carries any data of its own: everything the programs
    // read comes from the launch parameters instead. That is what keeps this the simple
    // shape rather than the intimidating one.
    // Five records: two raygen programs, one miss, and one hit group per primitive kind.
    // Both raygen records live in the table and the launch points at whichever it wants,
    // which is cheaper than keeping two tables that differ in one entry.
    std::vector<EmptyRecord> records(5);
    if (!errors.optix(optixSbtRecordPackHeader(state.raygenGroup, &records[0]), "packing the raygen record") ||
        !errors.optix(optixSbtRecordPackHeader(state.renderGroup, &records[1]), "packing the render record") ||
        !errors.optix(optixSbtRecordPackHeader(state.missGroup, &records[2]), "packing the miss record") ||
        !errors.optix(optixSbtRecordPackHeader(state.triangleGroup, &records[3]), "packing the triangle record") ||
        !errors.optix(optixSbtRecordPackHeader(state.sphereGroup, &records[4]), "packing the sphere record"))
    {
        return nullptr;
    }

    // The hit group records are ordered to match the instances, so an instance's SBT
    // offset selects its own program. With no triangles, the sphere record moves first.
    if (state.triangleCount == 0)
    {
        records[3] = records[4];
    }

    if (!upload(records, state.sbtRecords))
    {
        errors.set("could not upload the shader binding table");
        return nullptr;
    }

    state.sbt.raygenRecord = state.sbtRecords;
    state.sbt.missRecordBase = state.sbtRecords + (2 * sizeof(EmptyRecord));
    state.sbt.missRecordStrideInBytes = sizeof(EmptyRecord);
    state.sbt.missRecordCount = 1;
    state.sbt.hitgroupRecordBase = state.sbtRecords + (3 * sizeof(EmptyRecord));
    state.sbt.hitgroupRecordStrideInBytes = sizeof(EmptyRecord);
    state.sbt.hitgroupRecordCount = 2;

    if (!errors.cuda(cuMemAlloc(&state.launchParams, sizeof(LaunchParams)), "cuMemAlloc (launch parameters)"))
    {
        return nullptr;
    }

    return tracer;
}

auto Tracer::trace(const std::vector<Vec3> &origins, const std::vector<Vec3> &directions, float tMin, float tMax,
                   std::vector<DeviceHit> &hits) -> bool
{
    State &state = *m_State;

    const size_t count = origins.size();
    if (count == 0 || directions.size() != count)
    {
        return false;
    }

    hits.resize(count);

    // Buffers are grown rather than reallocated per launch, since the interesting use is
    // many launches of a similar size.
    if (count > state.rayCapacity)
    {
        for (CUdeviceptr *pointer : {&state.rayOrigins, &state.rayDirections, &state.hits})
        {
            if (*pointer != 0)
            {
                cuMemFree(*pointer);
                *pointer = 0;
            }
        }

        if (cuMemAlloc(&state.rayOrigins, count * sizeof(Vec3)) != CUDA_SUCCESS ||
            cuMemAlloc(&state.rayDirections, count * sizeof(Vec3)) != CUDA_SUCCESS ||
            cuMemAlloc(&state.hits, count * sizeof(DeviceHit)) != CUDA_SUCCESS)
        {
            return false;
        }

        state.rayCapacity = count;
    }

    cuMemcpyHtoD(state.rayOrigins, origins.data(), count * sizeof(Vec3));
    cuMemcpyHtoD(state.rayDirections, directions.data(), count * sizeof(Vec3));

    LaunchParams params = {};
    fillSceneParams(state, params);
    params.rayOrigins = reinterpret_cast<const Vec3 *>(state.rayOrigins);
    params.rayDirections = reinterpret_cast<const Vec3 *>(state.rayDirections);
    params.hits = reinterpret_cast<DeviceHit *>(state.hits);
    params.tMin = tMin;
    params.tMax = tMax;
    params.rayCount = static_cast<unsigned int>(count);

    cuMemcpyHtoD(state.launchParams, &params, sizeof(params));

    state.sbt.raygenRecord = state.sbtRecords;

    if (optixLaunch(state.pipeline, nullptr, state.launchParams, sizeof(LaunchParams), &state.sbt,
                    static_cast<unsigned int>(count), 1, 1) != OPTIX_SUCCESS)
    {
        return false;
    }

    if (cuCtxSynchronize() != CUDA_SUCCESS)
    {
        return false;
    }

    return cuMemcpyDtoH(hits.data(), state.hits, count * sizeof(DeviceHit)) == CUDA_SUCCESS;
}

auto Tracer::render(int width, int height, unsigned int samplesPerPixel, unsigned long long seed, unsigned int maxDepth,
                    std::vector<Vec3> &colour, std::vector<Vec3> &albedo, std::vector<Vec3> &normal) -> bool
{
    State &state = *m_State;

    if (width <= 0 || height <= 0 || samplesPerPixel == 0)
    {
        return false;
    }

    const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);

    colour.resize(pixels);
    albedo.resize(pixels);
    normal.resize(pixels);

    if (pixels > state.filmCapacity)
    {
        for (CUdeviceptr *pointer : {&state.film, &state.filmAlbedo, &state.filmNormal})
        {
            if (*pointer != 0)
            {
                cuMemFree(*pointer);
                *pointer = 0;
            }
        }

        if (cuMemAlloc(&state.film, pixels * sizeof(Vec3)) != CUDA_SUCCESS ||
            cuMemAlloc(&state.filmAlbedo, pixels * sizeof(Vec3)) != CUDA_SUCCESS ||
            cuMemAlloc(&state.filmNormal, pixels * sizeof(Vec3)) != CUDA_SUCCESS)
        {
            return false;
        }

        state.filmCapacity = pixels;
    }

    LaunchParams params = {};
    fillSceneParams(state, params);

    // The film plane at unit distance, as RayTracer::makeCameraRay computes it.
    const float halfHeight = std::tan(state.camera.fovY * 0.5f);

    params.camera.origin = state.camera.org;
    params.camera.direction = state.camera.dir;
    params.camera.right = state.camera.right;
    params.camera.up = state.camera.up;
    params.camera.halfHeight = halfHeight;
    params.camera.halfWidth = halfHeight * (static_cast<float>(width) / static_cast<float>(height));

    params.film = reinterpret_cast<Vec3 *>(state.film);
    params.filmAlbedo = reinterpret_cast<Vec3 *>(state.filmAlbedo);
    params.filmNormal = reinterpret_cast<Vec3 *>(state.filmNormal);
    params.width = width;
    params.height = height;
    params.samplesPerPixel = samplesPerPixel;
    params.maxDepth = maxDepth;
    params.seed = seed;

    cuMemcpyHtoD(state.launchParams, &params, sizeof(params));

    // The render raygen rather than the bare traversal one.
    state.sbt.raygenRecord = state.sbtRecords + sizeof(EmptyRecord);

    if (optixLaunch(state.pipeline, nullptr, state.launchParams, sizeof(LaunchParams), &state.sbt,
                    static_cast<unsigned int>(width), static_cast<unsigned int>(height), 1) != OPTIX_SUCCESS)
    {
        return false;
    }

    if (cuCtxSynchronize() != CUDA_SUCCESS)
    {
        return false;
    }

    return cuMemcpyDtoH(colour.data(), state.film, pixels * sizeof(Vec3)) == CUDA_SUCCESS &&
           cuMemcpyDtoH(albedo.data(), state.filmAlbedo, pixels * sizeof(Vec3)) == CUDA_SUCCESS &&
           cuMemcpyDtoH(normal.data(), state.filmNormal, pixels * sizeof(Vec3)) == CUDA_SUCCESS;
}

} // namespace Gpu
