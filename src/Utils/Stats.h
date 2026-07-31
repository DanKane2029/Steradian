#pragma once

#include <cstdint>

/**
 * \brief Counters describing how much work a render actually did.
 *
 * These exist to make acceleration-structure work measurable. Wall-clock time alone
 * cannot distinguish "the BVH culls well" from "the machine was idle", but primitive
 * tests per ray can: a structure that culls nothing tests every primitive on every ray,
 * and the ratio falls sharply once traversal starts rejecting subtrees.
 *
 * Counting is thread-local and lock-free on the hot path. Each worker merges its totals
 * once when it finishes, so the only synchronization is one lock per thread per render.
 */
namespace Stats
{

struct Counters
{
    uint64_t rays = 0;           ///< rays traced against the scene, primary and secondary
    uint64_t nodeVisits = 0;     ///< acceleration structure nodes entered
    uint64_t primitiveTests = 0; ///< ray/primitive intersection tests performed
};

#ifdef PT_ENABLE_STATS

extern thread_local Counters t_counters;

inline void countRay()
{
    t_counters.rays++;
}

inline void countNodeVisit()
{
    t_counters.nodeVisits++;
}

inline void countPrimitiveTest()
{
    t_counters.primitiveTests++;
}

#else

inline void countRay()
{
}

inline void countNodeVisit()
{
}

inline void countPrimitiveTest()
{
}

#endif

/** zeroes the calling thread's counters */
void resetThread();

/** adds the calling thread's counters into the global total */
void mergeThread();

/** zeroes the global total */
void resetGlobal();

/** returns the accumulated global total */
auto total() -> Counters;

/** true when the build was configured with statistics enabled */
auto enabled() -> bool;

} // namespace Stats
