// Checks that the shared maths really is shared.
//
// Stage 2 of the GPU work claims that Vec3, Rng, Sampling and Microfacet compile
// unchanged as device code and compute the same things there. That claim is worth exactly
// as much as its test, because the failure mode is silent: a header that quietly stops
// being device-portable is not noticed until someone tries to build a kernel months
// later, and a routine that computes something subtly different on the device produces an
// image that looks plausible and is wrong.
//
// So this compiles the actual header files -- read from the source tree, not copies --
// with NVRTC, runs them on the device, and compares against the host's own answers.
// Editing Vec3.h to use std::vector again breaks this test rather than Stage 3.
//
// Two different standards are applied, because two different things are being claimed:
//
//   - The generator must agree BIT FOR BIT. It is integer arithmetic, so there is no
//     excuse for divergence, and everything else depends on both sides drawing the same
//     numbers before any comparison means anything.
//   - The floating point quantities must agree CLOSELY, not exactly. CUDA's sinf and
//     sqrtf are not the host's, fused multiply-add contracts differently, and no amount
//     of care makes them identical. The tolerance below was measured rather than guessed.
//
// Usage: device_parity [caseCount]

#include "Gpu/Nvrtc.h"
#include "Utils/Microfacet.h"
#include "Utils/Random.h"
#include "Utils/Sampling.h"
#include "Utils/Vec3.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <cuda.h>
#include <nvrtc.h>

namespace
{

/**
 * \brief The quantities compared, in the order the kernel writes them.
 *
 * Named rather than indexed by a bare number so a failure says which routine disagreed.
 */
const char *const quantityNames[] = {
    "roughnessToAlpha",  "distribution",   "maskingG1",      "maskingG2",    "pdf",
    "evaluate",          "vnWeight",       "sampleVN.x",     "sampleVN.y",   "sampleVN.z",
    "directionalAlbedo", "fresnelSchlick", "cosineHemi.pdf", "cosineHemi.x", "cosineHemi.y",
    "cosineHemi.z",      "reflect.z",      "refract.z",      "basis.t.x",    "vec3.normalize",
};

constexpr int quantityCount = static_cast<int>(sizeof(quantityNames) / sizeof(quantityNames[0]));

/** headers handed to NVRTC, under the names the sources include them by */
const char *const sharedHeaders[] = {
    "Utils/DeviceCompat.h", "Utils/Vec3.h", "Utils/Random.h", "Utils/Sampling.h", "Utils/Microfacet.h",
};

constexpr int sharedHeaderCount = static_cast<int>(sizeof(sharedHeaders) / sizeof(sharedHeaders[0]));

/**
 * \brief The device side of the comparison.
 *
 * Deliberately contains no maths of its own. Every value it writes comes from a header
 * shared with the host, so a disagreement is a disagreement about the shared code rather
 * than about this kernel.
 */
const char *const kernelSource = R"CUDA(
#include "Utils/Microfacet.h"
#include "Utils/Random.h"
#include "Utils/Sampling.h"
#include "Utils/Vec3.h"

extern "C" __global__ void evaluateAll(const float *albedoTable, unsigned long long seed, int caseCount,
                                       float *out, unsigned int *rngOut)
{
    const int i = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (i >= caseCount)
    {
        return;
    }

    Rng rng(seed, (unsigned long long)i);

    // Drawn in exactly the order the host draws them.
    rngOut[i] = rng.nextUInt();

    const float roughness = rng.nextFloat();
    const float u1 = rng.nextFloat();
    const float u2 = rng.nextFloat();
    const float r0 = rng.nextFloat();

    // Drawn into named locals, one statement each. As constructor arguments the order
    // would be unspecified, and the two compilers do not choose the same one -- which is
    // how this test first "found" a disagreement that was entirely its own fault.
    const float wox = rng.nextFloatSigned();
    const float woy = rng.nextFloatSigned();
    const float woz = rng.nextFloat() + 0.05f;
    const float wix = rng.nextFloatSigned();
    const float wiy = rng.nextFloatSigned();
    const float wiz = rng.nextFloat() + 0.05f;

    const Vec3 wo = Vec3(wox, woy, woz).normalized();
    const Vec3 wi = Vec3(wix, wiy, wiz).normalized();

    const float alpha = Microfacet::roughnessToAlpha(roughness);
    const Vec3 h = (wo + wi).normalized();
    const Vec3 sampled = Microfacet::sampleVisibleNormal(wo, alpha, u1, u2);

    float pdf = 0.0f;
    Rng hemiRng(seed ^ 0x9e3779b9ull, (unsigned long long)i);
    const Vec3 hemi = Sampling::cosineHemisphere(hemiRng, pdf);

    const Vec3 reflected = Sampling::reflect(-wo, h);

    Vec3 refracted;
    Sampling::refract(-wo, Vec3(0.0f, 0.0f, 1.0f), 0.66f, refracted);

    Vec3 t;
    Vec3 b;
    Sampling::buildBasis(wo, t, b);

    float *o = out + ((long)i * 20);

    o[0]  = alpha;
    o[1]  = Microfacet::distribution(h.z, alpha);
    o[2]  = Microfacet::maskingG1(wo, alpha);
    o[3]  = Microfacet::maskingG2(wo, wi, alpha);
    o[4]  = Microfacet::pdf(wo, wi, alpha);
    o[5]  = Microfacet::evaluate(wo, wi, alpha);
    o[6]  = Microfacet::visibleNormalWeight(wo, wi, alpha);
    o[7]  = sampled.x;
    o[8]  = sampled.y;
    o[9]  = sampled.z;
    o[10] = Microfacet::directionalAlbedo(albedoTable, wo.z, roughness);
    o[11] = Sampling::fresnelSchlick(wo.z, r0);
    o[12] = pdf;
    o[13] = hemi.x;
    o[14] = hemi.y;
    o[15] = hemi.z;
    o[16] = reflected.z;
    o[17] = refracted.z;
    o[18] = t.x;
    o[19] = (wo * 3.7f).normalized().z;
}
)CUDA";

/** the host side, written to mirror the kernel line for line */
void evaluateOnHost(uint64_t seed, int index, float *out, uint32_t &rngOut)
{
    Rng rng(seed, static_cast<uint64_t>(index));

    rngOut = rng.nextUInt();

    const float roughness = rng.nextFloat();
    const float u1 = rng.nextFloat();
    const float u2 = rng.nextFloat();
    const float r0 = rng.nextFloat();

    const float wox = rng.nextFloatSigned();
    const float woy = rng.nextFloatSigned();
    const float woz = rng.nextFloat() + 0.05f;
    const float wix = rng.nextFloatSigned();
    const float wiy = rng.nextFloatSigned();
    const float wiz = rng.nextFloat() + 0.05f;

    const Vec3 wo = Vec3(wox, woy, woz).normalized();
    const Vec3 wi = Vec3(wix, wiy, wiz).normalized();

    const float alpha = Microfacet::roughnessToAlpha(roughness);
    const Vec3 h = (wo + wi).normalized();
    const Vec3 sampled = Microfacet::sampleVisibleNormal(wo, alpha, u1, u2);

    float pdf = 0.0f;
    Rng hemiRng(seed ^ 0x9e3779b9ULL, static_cast<uint64_t>(index));
    const Vec3 hemi = Sampling::cosineHemisphere(hemiRng, pdf);

    const Vec3 reflected = Sampling::reflect(-wo, h);

    Vec3 refracted;
    Sampling::refract(-wo, Vec3(0.0f, 0.0f, 1.0f), 0.66f, refracted);

    Vec3 t;
    Vec3 b;
    Sampling::buildBasis(wo, t, b);

    out[0] = alpha;
    out[1] = Microfacet::distribution(h.z, alpha);
    out[2] = Microfacet::maskingG1(wo, alpha);
    out[3] = Microfacet::maskingG2(wo, wi, alpha);
    out[4] = Microfacet::pdf(wo, wi, alpha);
    out[5] = Microfacet::evaluate(wo, wi, alpha);
    out[6] = Microfacet::visibleNormalWeight(wo, wi, alpha);
    out[7] = sampled.x;
    out[8] = sampled.y;
    out[9] = sampled.z;
    out[10] = Microfacet::directionalAlbedo(Microfacet::hostAlbedoTable(), wo.z, roughness);
    out[11] = Sampling::fresnelSchlick(wo.z, r0);
    out[12] = pdf;
    out[13] = hemi.x;
    out[14] = hemi.y;
    out[15] = hemi.z;
    out[16] = reflected.z;
    out[17] = refracted.z;
    out[18] = t.x;
    out[19] = (wo * 3.7f).normalized().z;
}

auto readFile(const std::string &path) -> std::string
{
    std::ifstream file(path);
    if (!file)
    {
        std::fprintf(stderr, "could not read %s\n", path.c_str());
        std::exit(1);
    }

    std::ostringstream contents;
    contents << file.rdbuf();

    return contents.str();
}

auto check(CUresult status, const char *what) -> bool
{
    if (status == CUDA_SUCCESS)
    {
        return true;
    }

    const char *message = nullptr;
    cuGetErrorString(status, &message);
    std::fprintf(stderr, "%s failed: %s\n", what, (message != nullptr) ? message : "unknown");

    return false;
}

} // namespace

auto main(int argc, char *argv[]) -> int
{
    const int caseCount = (argc > 1) ? std::atoi(argv[1]) : 65536;

    // Where the shared headers live. Passed in by CMake so the test compiles the same
    // files the renderer does rather than a copy that could drift.
    const std::string sourceDir = PT_SOURCE_DIR;

    // ---- compile the shared headers as device code -------------------------------
    std::vector<std::string> headerSources;
    headerSources.reserve(sharedHeaderCount);
    for (const char *name : sharedHeaders)
    {
        headerSources.push_back(readFile(sourceDir + "/" + name));
    }

    std::vector<const char *> headerPointers;
    for (const std::string &source : headerSources)
    {
        headerPointers.push_back(source.c_str());
    }

    // Before any NVRTC call: the builtins have to be reachable or compilation fails
    // with a message that points at the library rather than at the cause.
    Gpu::prepareNvrtc();

    nvrtcProgram program{};
    if (nvrtcCreateProgram(&program, kernelSource, "device_parity.cu", sharedHeaderCount, headerPointers.data(),
                           sharedHeaders) != NVRTC_SUCCESS)
    {
        std::fprintf(stderr, "nvrtcCreateProgram failed\n");
        return 1;
    }

    // No --use_fast_math. It would replace sinf and friends with cheaper approximations
    // and widen the gap being measured here, which is the opposite of the point.
    const char *options[] = {"--gpu-architecture=compute_86", "--std=c++17"};
    const nvrtcResult compiled = nvrtcCompileProgram(program, 2, options);

    size_t logSize = 0;
    nvrtcGetProgramLogSize(program, &logSize);
    if (logSize > 1)
    {
        std::string log(logSize, '\0');
        nvrtcGetProgramLog(program, log.data());
        std::fprintf(stderr, "%s\n", log.c_str());
    }

    if (compiled != NVRTC_SUCCESS)
    {
        std::fprintf(stderr, "the shared headers did not compile as device code\n");
        return 1;
    }

    size_t ptxSize = 0;
    nvrtcGetPTXSize(program, &ptxSize);
    std::string ptx(ptxSize, '\0');
    nvrtcGetPTX(program, ptx.data());

    std::printf("shared headers compiled to %zu bytes of PTX\n", ptxSize);

    // ---- run it ------------------------------------------------------------------
    if (!check(cuInit(0), "cuInit"))
    {
        return 1;
    }

    CUdevice device = 0;
    if (!check(cuDeviceGet(&device, 0), "cuDeviceGet"))
    {
        return 1;
    }

    CUcontext context = nullptr;
    if (!check(cuCtxCreate(&context, 0, device), "cuCtxCreate"))
    {
        return 1;
    }

    CUmodule module = nullptr;
    if (!check(cuModuleLoadData(&module, ptx.c_str()), "cuModuleLoadData"))
    {
        return 1;
    }

    CUfunction kernel = nullptr;
    if (!check(cuModuleGetFunction(&kernel, module, "evaluateAll"), "cuModuleGetFunction"))
    {
        return 1;
    }

    constexpr int tableEntries = Microfacet::albedoResolution * Microfacet::albedoResolution;

    CUdeviceptr deviceTable = 0;
    CUdeviceptr deviceOut = 0;
    CUdeviceptr deviceRng = 0;
    cuMemAlloc(&deviceTable, tableEntries * sizeof(float));
    cuMemAlloc(&deviceOut, static_cast<size_t>(caseCount) * quantityCount * sizeof(float));
    cuMemAlloc(&deviceRng, static_cast<size_t>(caseCount) * sizeof(uint32_t));

    // The device is given the host's measured table rather than measuring its own, so
    // this compares the lookup and not the measurement.
    cuMemcpyHtoD(deviceTable, Microfacet::hostAlbedoTable(), tableEntries * sizeof(float));

    uint64_t seed = 0x5eed'0f9a'1234'5678ULL;
    int count = caseCount;

    void *arguments[] = {&deviceTable, &seed, &count, &deviceOut, &deviceRng};

    const int threads = 128;
    const int blocks = (caseCount + threads - 1) / threads;

    if (!check(cuLaunchKernel(kernel, blocks, 1, 1, threads, 1, 1, 0, nullptr, arguments, nullptr), "cuLaunchKernel") ||
        !check(cuCtxSynchronize(), "cuCtxSynchronize"))
    {
        return 1;
    }

    std::vector<float> deviceValues(static_cast<size_t>(caseCount) * quantityCount);
    std::vector<uint32_t> deviceRandom(static_cast<size_t>(caseCount));
    cuMemcpyDtoH(deviceValues.data(), deviceOut, deviceValues.size() * sizeof(float));
    cuMemcpyDtoH(deviceRandom.data(), deviceRng, deviceRandom.size() * sizeof(uint32_t));

    // ---- compare ------------------------------------------------------------------
    //
    // Both a relative and an absolute difference are tracked, and a quantity passes on
    // either. That is not a way of being lenient; it is the only defensible reading.
    //
    // Some of these expressions are genuinely ill-conditioned at particular inputs, and
    // the two backends evaluate them with different elementary functions. The GGX
    // denominator is (cos^2)(alpha^2 - 1) + 1, which cancels almost completely as a
    // surface approaches a mirror, and cosineHemisphere's z is sqrt(1 - x^2 - y^2), which
    // cancels at the edge of the disc. At those points a last-bit difference in sinf or
    // sqrtf becomes a large *relative* difference in the result while remaining a
    // vanishing absolute one. Judging such a case only relatively would say the two
    // backends disagree about a number that is, for rendering purposes, the same number.
    // Both figures come from measurement across 65,536 cases, not from taste.
    //
    // Seventeen of the twenty quantities agree to 6e-5 absolute or better, which is
    // last-bit. The other three -- distribution, pdf and evaluate -- are unbounded, and a
    // near-mirror GGX lobe evaluates into the thousands, so their absolute difference
    // reaches 1.2e-1 while their relative difference stays at 5.4e-4. Each threshold
    // therefore sits about an order of magnitude above the worst thing observed.
    //
    // That still leaves the test with teeth. A genuine defect -- a dropped term, a wrong
    // constant, a cosine on the wrong side -- moves a result by a fraction of itself,
    // which is two to three orders of magnitude outside either bound. Verified by
    // injecting exactly that and watching this fail.
    constexpr float relativeTolerance = 5e-3f;
    constexpr float absoluteTolerance = 1e-3f;

    std::vector<float> worstRelative(quantityCount, 0.0f);
    std::vector<float> worstAbsolute(quantityCount, 0.0f);
    std::vector<float> hostValues(quantityCount);

    int rngMismatches = 0;
    int failures = 0;

    for (int i = 0; i < caseCount; i++)
    {
        uint32_t hostRandom = 0;
        evaluateOnHost(seed, i, hostValues.data(), hostRandom);

        if (hostRandom != deviceRandom[static_cast<size_t>(i)])
        {
            if (rngMismatches < 3)
            {
                std::fprintf(stderr, "case %d: generator disagrees, host %u device %u\n", i, hostRandom,
                             deviceRandom[static_cast<size_t>(i)]);
            }
            rngMismatches++;
        }

        for (int q = 0; q < quantityCount; q++)
        {
            const float host = hostValues[static_cast<size_t>(q)];
            const float dev = deviceValues[(static_cast<size_t>(i) * quantityCount) + static_cast<size_t>(q)];

            if (std::isnan(host) != std::isnan(dev))
            {
                std::fprintf(stderr, "case %d, %s: one side is NaN (host %f, device %f)\n", i, quantityNames[q], host,
                             dev);
                failures++;
                continue;
            }

            if (std::isnan(host))
            {
                continue;
            }

            const float absolute = std::fabs(host - dev);
            const float scale = std::max(std::fabs(host), std::fabs(dev));
            const float relative = (scale > 0.0f) ? (absolute / scale) : 0.0f;

            worstAbsolute[static_cast<size_t>(q)] = std::max(worstAbsolute[static_cast<size_t>(q)], absolute);

            // The relative figure is only recorded where the absolute one is already
            // beyond what a rendered image could notice; below that it says nothing.
            if (absolute > absoluteTolerance)
            {
                worstRelative[static_cast<size_t>(q)] = std::max(worstRelative[static_cast<size_t>(q)], relative);
            }
        }
    }

    std::printf("\n%d cases, %d quantities each\n\n", caseCount, quantityCount);
    std::printf("  %-20s %12s %12s\n", "quantity", "worst abs", "worst rel");

    for (int q = 0; q < quantityCount; q++)
    {
        const float absolute = worstAbsolute[static_cast<size_t>(q)];
        const float relative = worstRelative[static_cast<size_t>(q)];

        const bool bad = absolute > absoluteTolerance && relative > relativeTolerance;

        std::printf("  %-20s %12.3e %12.3e %s\n", quantityNames[q], static_cast<double>(absolute),
                    static_cast<double>(relative), bad ? " EXCEEDS BOTH TOLERANCES" : "");

        if (bad)
        {
            failures++;
        }
    }

    std::printf("\n  tolerances           %12.3e %12.3e\n", static_cast<double>(absoluteTolerance),
                static_cast<double>(relativeTolerance));
    std::printf("  generator            %s\n", (rngMismatches == 0) ? "bit identical" : "MISMATCHED");

    cuMemFree(deviceTable);
    cuMemFree(deviceOut);
    cuMemFree(deviceRng);
    cuModuleUnload(module);
    cuCtxDestroy(context);

    if (rngMismatches != 0 || failures != 0)
    {
        std::fprintf(stderr, "\ndevice and host disagree\n");
        return 1;
    }

    std::printf("\nThe shared maths agrees on both backends.\n");
    return 0;
}
