#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

/**
 * \brief A fixed set of worker threads that cooperatively drain a range of work items.
 *
 * Threads are created once and reused. The renderer previously spawned a fresh set of
 * std::threads every frame and never cleared the vector holding them, so the cost of
 * thread creation was paid per frame and the vector grew for the lifetime of the process.
 *
 * Work is claimed with a single atomic counter rather than handed out in equal shares.
 * Tiles vary enormously in cost — a tile of empty background is far cheaper than one full
 * of geometry — so a static division leaves threads idle waiting for the slowest one.
 */
class ThreadPool
{
  public:
    /**
     * \brief Starts the pool.
     *
     * \param threadCount Number of workers. Zero or one runs everything on the calling
     *        thread, which keeps single-threaded runs free of synchronization.
     */
    explicit ThreadPool(unsigned int threadCount) : m_ThreadCount(threadCount)
    {
        if (m_ThreadCount <= 1)
        {
            return;
        }

        m_Workers.reserve(m_ThreadCount);
        for (unsigned int i = 0; i < m_ThreadCount; i++)
        {
            m_Workers.emplace_back([this]() { workerLoop(); });
        }
    }

    ~ThreadPool()
    {
        {
            const std::lock_guard<std::mutex> lock(m_Mutex);
            m_Stopping = true;
        }
        m_WakeWorkers.notify_all();

        for (std::thread &worker : m_Workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    ThreadPool(const ThreadPool &) = delete;
    auto operator=(const ThreadPool &) -> ThreadPool & = delete;
    ThreadPool(ThreadPool &&) = delete;
    auto operator=(ThreadPool &&) -> ThreadPool & = delete;

    /**
     * \brief Runs `task` for every index in [0, itemCount) and returns once all are done.
     *
     * \param itemCount Number of work items.
     * \param task Called with each item index, concurrently from several threads.
     */
    void parallelFor(uint32_t itemCount, const std::function<void(uint32_t)> &task)
    {
        if (itemCount == 0)
        {
            return;
        }

        if (m_Workers.empty())
        {
            for (uint32_t i = 0; i < itemCount; i++)
            {
                task(i);
            }
            return;
        }

        {
            const std::lock_guard<std::mutex> lock(m_Mutex);
            m_Task = &task;
            m_ItemCount = itemCount;
            m_NextItem.store(0, std::memory_order_relaxed);
            m_ActiveWorkers = m_ThreadCount;
            m_Generation++;
        }
        m_WakeWorkers.notify_all();

        // The calling thread joins in rather than idling while the workers run.
        runItems();

        std::unique_lock<std::mutex> lock(m_Mutex);
        m_WorkDone.wait(lock, [this]() { return m_ActiveWorkers == 0; });
        m_Task = nullptr;
    }

    auto threadCount() const -> unsigned int
    {
        return std::max(1U, m_ThreadCount);
    }

  private:
    /** claims and runs work items until the queue is empty */
    void runItems()
    {
        while (true)
        {
            const uint32_t item = m_NextItem.fetch_add(1, std::memory_order_relaxed);
            if (item >= m_ItemCount)
            {
                return;
            }

            (*m_Task)(item);
        }
    }

    void workerLoop()
    {
        uint64_t lastGeneration = 0;

        while (true)
        {
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                m_WakeWorkers.wait(lock, [&]() { return m_Stopping || m_Generation != lastGeneration; });

                if (m_Stopping)
                {
                    return;
                }

                lastGeneration = m_Generation;
            }

            runItems();

            {
                const std::lock_guard<std::mutex> lock(m_Mutex);
                m_ActiveWorkers--;
            }
            m_WorkDone.notify_all();
        }
    }

    unsigned int m_ThreadCount;
    std::vector<std::thread> m_Workers;

    std::mutex m_Mutex;
    std::condition_variable m_WakeWorkers;
    std::condition_variable m_WorkDone;

    const std::function<void(uint32_t)> *m_Task = nullptr;
    std::atomic<uint32_t> m_NextItem{0};
    uint32_t m_ItemCount = 0;
    unsigned int m_ActiveWorkers = 0;
    uint64_t m_Generation = 0;
    bool m_Stopping = false;
};
