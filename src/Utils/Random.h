#pragma once

#include "Utils/DeviceCompat.h"

/**
 * \brief A small, fast, seekable pseudo random number generator (PCG32).
 *
 * Each render worker owns its own generator instance, seeded deterministically from the
 * render seed and the region it is responsible for. That gives three properties the
 * renderer needs: no shared mutable state between threads, no lock contention, and
 * output that is reproducible for a given seed regardless of thread count.
 *
 * Compiled unchanged as device code: it is integer arithmetic and nothing else, which is
 * also why the same seed produces the same stream on either backend.
 *
 * Reference: https://www.pcg-random.org/
 */
class Rng
{
  public:
    // No annotation: a defaulted trivial constructor is usable on either side already,
    // and marking it draws a warning from NVRTC rather than doing anything.
    Rng() = default;

    /**
     * \brief Creates a generator from a seed and a stream identifier.
     *
     * Two generators with the same seed but different sequence values produce distinct,
     * non-overlapping streams, which is what makes per-thread seeding safe.
     *
     * \param seed The base seed for the render.
     * \param sequence The stream selector, typically derived from the pixel row or tile.
     */
    PT_HOST_DEVICE Rng(uint64_t seed, uint64_t sequence)
    {
        m_State = 0U;
        m_Increment = (sequence << 1U) | 1U;
        nextUInt();
        m_State += seed;
        nextUInt();
    }

    /**
     * \brief Returns the next uniformly distributed 32 bit value.
     */
    PT_HOST_DEVICE auto nextUInt() -> uint32_t
    {
        const uint64_t oldState = m_State;
        m_State = (oldState * 6364136223846793005ULL) + m_Increment;

        const auto xorShifted = static_cast<uint32_t>(((oldState >> 18U) ^ oldState) >> 27U);
        const auto rot = static_cast<uint32_t>(oldState >> 59U);

        return (xorShifted >> rot) | (xorShifted << ((~rot + 1U) & 31U));
    }

    /**
     * \brief Returns the next uniformly distributed float in [0, 1).
     */
    PT_HOST_DEVICE auto nextFloat() -> float
    {
        // 24 bits of mantissa keeps the result exactly representable as a float.
        return static_cast<float>(nextUInt() >> 8U) * 0x1.0p-24f;
    }

    /**
     * \brief Returns the next uniformly distributed float in [-1, 1).
     */
    PT_HOST_DEVICE auto nextFloatSigned() -> float
    {
        return (nextFloat() * 2.0f) - 1.0f;
    }

  private:
    uint64_t m_State = 0x853c49e6748fea9bULL;
    uint64_t m_Increment = 0xda3e39cb94b95bdbULL;
};
