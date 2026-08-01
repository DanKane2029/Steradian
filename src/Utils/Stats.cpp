#include "Stats.h"

#include <mutex>

namespace Stats
{

namespace
{
std::mutex g_mutex;
Counters g_total;
} // namespace

#ifdef PT_ENABLE_STATS
thread_local Counters t_counters;
#endif

void resetThread()
{
#ifdef PT_ENABLE_STATS
    t_counters = Counters{};
#endif
}

void mergeThread()
{
#ifdef PT_ENABLE_STATS
    const std::lock_guard<std::mutex> lock(g_mutex);
    g_total.rays += t_counters.rays;
    g_total.nodeVisits += t_counters.nodeVisits;
    g_total.primitiveTests += t_counters.primitiveTests;
#endif
}

void resetGlobal()
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    g_total = Counters{};
}

auto total() -> Counters
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    return g_total;
}

auto enabled() -> bool
{
#ifdef PT_ENABLE_STATS
    return true;
#else
    return false;
#endif
}

} // namespace Stats
