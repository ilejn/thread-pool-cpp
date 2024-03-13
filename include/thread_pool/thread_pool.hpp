#pragma once

#include <thread_pool/fixed_function.hpp>
#include <thread_pool/mpmc_bounded_queue.hpp>
#include <thread_pool/thread_pool_options.hpp>
#include <thread_pool/worker.hpp>
#include <thread_pool/handler.hpp>

#include <mutex>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <vector>
#include <iostream>

namespace tp
{

template <typename Task>
class ThreadPoolImpl;
using ThreadPool = ThreadPoolImpl<FixedFunction<void(), 128>>;

/**
 * @brief The ThreadPool class implements thread pool pattern.
 * It is highly scalable and fast.
 * It is header only.
 * It implements both work-stealing and work-distribution balancing
 * startegies.
 * It implements cooperative scheduling strategy for tasks.
 */
template <typename Task>
class ThreadPoolImpl : public ActiveWorkers {
public:
    /**
     * @brief ThreadPool Construct and start new thread pool.
     * @param options Creation options.
     */
    explicit ThreadPoolImpl(
        const ThreadPoolOptions& options = ThreadPoolOptions());

    /**
     * @brief Move ctor implementation.
     */
    ThreadPoolImpl(ThreadPoolImpl&& rhs) noexcept;

    /**
     * @brief ~ThreadPool Stop all workers and destroy thread pool.
     */
    ~ThreadPoolImpl();

    /**
     * @brief Move assignment implementaion.
     */
    ThreadPoolImpl& operator=(ThreadPoolImpl&& rhs) noexcept;

    /**
     * @brief post Try post job to thread pool.
     * @param handler Handler to be called from thread pool worker. It has
     * to be callable as 'handler()'.
     * @return 'true' on success, false otherwise.
     * @note All exceptions thrown by handler will be suppressed.
     */
    template <typename Handler>
    bool tryPost(Handler&& handler);

    /**
     * @brief post Post job to thread pool.
     * @param handler Handler to be called from thread pool worker. It has
     * to be callable as 'handler()'.
     * @throw std::overflow_error if worker's queue is full.
     * @note All exceptions thrown by handler will be suppressed.
     */
    template <typename Handler>
    void post(Handler&& handler);

    /**
     * @brief Wait for all currently active jobs to be done.
     * @note You may call schedule and wait many times in arbitrary order.
     * If any thread was throw an exception, first exception will be rethrown from this method,
     *  and exception will be cleared.
     */
    void wait();

private:
    Worker<Task>& getWorker();

    std::vector<std::unique_ptr<Worker<Task>>> m_workers;
    std::atomic<size_t> m_next_worker;
    std::atomic<size_t> m_num_workers;
    std::atomic<size_t> m_active_workers;
    ThreadPoolOptions m_options;
    std::mutex m_mutex;
};


/// Implementation

template <typename Task>
inline ThreadPoolImpl<Task>::ThreadPoolImpl(const ThreadPoolOptions& options)
    : m_workers(options.maxFreeThreads())
    , m_next_worker(0)
{
    for(auto& worker_ptr : m_workers)
    {
        worker_ptr.reset(new Worker<Task>(options.queueSize(), this));
    }

    for(size_t i = 0; i < m_workers.size(); ++i)
    {
        Worker<Task>* steal_donor =
                                m_workers[(i + 1) % m_workers.size()].get();
        m_workers[i]->start(i, steal_donor);
    }
    m_num_workers = m_workers.size();
}

template <typename Task>
inline ThreadPoolImpl<Task>::ThreadPoolImpl(ThreadPoolImpl<Task>&& rhs) noexcept
{
    *this = rhs;
}

template <typename Task>
inline ThreadPoolImpl<Task>::~ThreadPoolImpl()
{
    try
    {
        wait();
    }
    catch(...)
    {
    }
}

template <typename Task>
inline void ThreadPoolImpl<Task>::wait()
{
    std::unique_lock lock(m_mutex);
    for (auto& worker_ptr : m_workers)
    {
        worker_ptr->stop();
    }
}

template <typename Task>
inline ThreadPoolImpl<Task>&
ThreadPoolImpl<Task>::operator=(ThreadPoolImpl<Task>&& rhs) noexcept
{
    if (this != &rhs)
    {
        std::unique_lock lock(m_mutex);

        m_workers = std::move(rhs.m_workers);
        m_next_worker = rhs.m_next_worker.load();
        m_active_workers = rhs.m_active_workers.load();
        m_num_workers = rhs.m_num_workers.load();
    }
    return *this;
}

template <typename Task>
template <typename Handler>
inline bool ThreadPoolImpl<Task>::tryPost(Handler&& handler)
{

    while (/* switch to atomic */ m_active_workers == m_num_workers && m_num_workers < m_options.threadCount())
    {
        size_t new_worker_num = 0;
        {
            std::unique_lock lock(m_mutex);

            new_worker_num = m_workers.size();
            if (!(/* switch to atomic */ m_active_workers == new_worker_num && new_worker_num < m_options.threadCount()))
            {
                break;
            }

            m_workers.emplace_back(std::make_unique<Worker<Task>>(m_options.queueSize(), this));
            m_num_workers++;
        }
        Worker<Task>* steal_donor = m_workers[(new_worker_num + 1) % m_workers.size()].get();
        m_workers[new_worker_num]->start(new_worker_num, steal_donor);
        break;
    }

    return getWorker().post(std::forward<Handler>(handler));
}

template <typename Task>
template <typename Handler>
inline void ThreadPoolImpl<Task>::post(Handler&& handler)
{
    const auto ok = tryPost(std::forward<Handler>(handler));
    if (!ok)
    {
        throw std::runtime_error("thread pool queue is full");
    }
}

template <typename Task>
inline Worker<Task>& ThreadPoolImpl<Task>::getWorker()
{
    auto id = Worker<Task>::getWorkerIdForCurrentThread();

    if (id > m_workers.size())
    {
        id = m_next_worker.fetch_add(1, std::memory_order_relaxed) %
             m_workers.size();
    }

    // auto id = m_next_worker.fetch_add(1, std::memory_order_relaxed) %
    //          m_workers.size();

    // std::cerr << id << ", " << std::this_thread::get_id() << std::endl;

    return *m_workers[id];
}
}
