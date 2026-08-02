#pragma once

/**
 * \brief What lets one body of maths be compiled by two very different compilers.
 *
 * The integrator's arithmetic -- vectors, sampling, the microfacet model, the generator --
 * has to exist in exactly one place. Two copies of a BSDF drift apart, and the drift shows
 * up as an image that is subtly wrong on one backend in a way no test written against
 * either backend alone would catch.
 *
 * The obstacle is that device code is compiled by NVRTC, which has no standard library at
 * all: no <cmath>, no <cstdint>, no std::max. So the shared headers are written against
 * this file instead, which supplies the same small vocabulary from whichever source the
 * current compiler actually has.
 *
 * Three compilations are possible and all three are exercised:
 *
 *   - the host compiler, which is what the CPU renderer and every test use;
 *   - NVRTC, at run time, for device code;
 *   - nvcc, which nothing uses today but which costs nothing to keep working.
 */

#if defined(__CUDACC_RTC__)

// NVRTC. No headers exist to include, and the fixed width types have to be declared.
// These match the CUDA ABI on every platform this targets.
using int8_t = signed char;
using uint8_t = unsigned char;
using int16_t = short;
using uint16_t = unsigned short;
using int32_t = int;
using uint32_t = unsigned int;
using int64_t = long long;
using uint64_t = unsigned long long;

#define PT_HOST_DEVICE __host__ __device__

#else

#include <cstdint>
#include <math.h>

#if defined(__CUDACC__)
#define PT_HOST_DEVICE __host__ __device__
#else
// The host compiler has no idea what these annotations are, so they vanish.
#define PT_HOST_DEVICE
#endif

#endif

/**
 * \brief The handful of <algorithm> functions the shared maths needs.
 *
 * Written out rather than taken from std:: because NVRTC has no std::. Each is defined to
 * do exactly what the standard one does for ordinary values, so replacing a call changes
 * nothing about the result -- which is the property that lets the golden images keep
 * guarding this code after it moved.
 *
 * The scalar maths functions themselves -- sqrtf, sinf, fabsf and so on -- need no shim:
 * the C names exist unqualified in both worlds. They are not necessarily bit-identical
 * between the two, and are not expected to be; see the device parity test for what is
 * actually asserted across backends.
 */
namespace Math
{

inline PT_HOST_DEVICE auto min(float a, float b) -> float
{
    return (b < a) ? b : a;
}

inline PT_HOST_DEVICE auto max(float a, float b) -> float
{
    return (a < b) ? b : a;
}

inline PT_HOST_DEVICE auto min(int a, int b) -> int
{
    return (b < a) ? b : a;
}

inline PT_HOST_DEVICE auto max(int a, int b) -> int
{
    return (a < b) ? b : a;
}

/** matches std::clamp, including its behaviour when the value is already in range */
inline PT_HOST_DEVICE auto clamp(float v, float lo, float hi) -> float
{
    return (v < lo) ? lo : ((hi < v) ? hi : v);
}

inline PT_HOST_DEVICE auto clamp(int v, int lo, int hi) -> int
{
    return (v < lo) ? lo : ((hi < v) ? hi : v);
}

} // namespace Math
