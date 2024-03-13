#pragma once

#include <atomic>
#include <thread>
// #include <semaphore>
#include <mutex>
#include <condition_variable>
#include <boost/circular_buffer.hpp>

#include <thread_pool/handler.hpp>


namespace tp
{

/**
 * @brief The Worker class owns task queue and executing thread.
 * In thread it tries to poqp task from queue. If queue is empty then it tries
 * to steal task from the sibling worker. If steal was unsuccessful then spins
 * with one millisecond delay.
 */
template <typename Task>
class Worker
{
public:
    /**
     * @brief Worker Constructor.
     * @param queue_size Length of undelaying task queue.
     */
    explicit Worker(size_t queue_size, ActiveWorkers* handler_ptr);

    /**
     * @brief Move ctor implementation.
     */
    Worker(Worker&& rhs) noexcept;

    /**
     * @brief Move assignment implementaion.
     */
    Worker& operator=(Worker&& rhs) noexcept;

    /**
     * @brief start Create the executing thread and start tasks execution.
     * @param id Worker ID.
     * @param steal_donor Sibling worker to steal task from it.
     */
    void start(size_t id, Worker* steal_donor);

    /**
     * @brief stop Stop all worker's thread and stealing activity.
     * Waits until the executing thread became finished.
     */
    void stop();

    /**
     * @brief post Post task to queue.
     * @param handler Handler to be executed in executing thread.
     * @return true on success.
     */
    template <typename Handler>
    bool post(Handler&& handler);

    /**
     * @brief steal Steal one task from this worker queue.
     * @param task Place for stealed task to be stored.
     * @return true on success.
     */
    bool steal(Task& task);

    /**
     * @brief getWorkerIdForCurrentThread Return worker ID associated with
     * current thread if exists.
     * @return Worker ID.
     */
    static size_t getWorkerIdForCurrentThread();

private:
    /**
     * @brief threadFunc Executing thread function.
     * @param id Worker ID to be associated with this thread.
     * @param steal_donor Sibling worker to steal task from it.
     */
    void threadFunc(size_t id, Worker* steal_donor);

    template <typename T>
    bool pop(T & val)
    {
        if (m_cb.empty())
            return false;
        else
        {
            val = std::move(m_cb.front());
            m_cb.pop_front();
            return true;
        }
    }

    // Queue<Task> m_queue;
    boost::circular_buffer<Task> m_cb;

    std::atomic<bool> m_running_flag;
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cond_var;

    ActiveWorkers* m_handler_ptr;
};


/// Implementation

namespace detail
{
    inline size_t* thread_id()
    {
        static thread_local size_t tss_id = -1u;
        return &tss_id;
    }
}

template <typename Task>
inline Worker<Task>::Worker(size_t queue_size, ActiveWorkers* handler_ptr)
    : m_cb(queue_size)
    , m_running_flag(true)
    , m_handler_ptr(handler_ptr)
{
}

template <typename Task>
inline Worker<Task>::Worker(Worker&& rhs) noexcept
{
    *this = rhs;
}

template <typename Task>
inline Worker<Task>& Worker<Task>::operator=(Worker&& rhs) noexcept
{
    if (this != &rhs)
    {
        m_cb = std::move(rhs.m_cb);
        m_running_flag = rhs.m_running_flag.load();
        m_handler_ptr->m_active_workers = rhs.m_handler_ptr->m_active_workers.load();
        m_thread = std::move(rhs.m_thread);
    }
    return *this;
}

template <typename Task>
inline void Worker<Task>::stop()
{
    m_running_flag.store(false, std::memory_order_relaxed);
    // m_sema.release();
    m_cond_var.notify_one();
    m_thread.join();
}

template <typename Task>
inline void Worker<Task>::start(size_t id, Worker* steal_donor)
{
    m_thread = std::thread(&Worker<Task>::threadFunc, this, id, steal_donor);
}

template <typename Task>
inline size_t Worker<Task>::getWorkerIdForCurrentThread()
{
    return *detail::thread_id();
}

template <typename Task>
template <typename Handler>
inline bool Worker<Task>::post(Handler&& handler)
{
    bool ret = true;
    {
        std::unique_lock lock(m_mutex);
        m_cb.push_back(std::forward<Handler>(handler));
    }

    // m_sema.release();
    m_cond_var.notify_one();

    return ret;
}

template <typename Task>
inline bool Worker<Task>::steal(Task& task)
{
    std::lock_guard lock(m_mutex);
    return pop(task);
}

template <typename Task>
inline void Worker<Task>::threadFunc(size_t id, Worker* steal_donor)
{
    *detail::thread_id() = id;

    Task handler;

    while (m_running_flag.load(std::memory_order_relaxed))
    {
        std::unique_lock lock(m_mutex);

        if (pop(handler) || steal_donor->steal(handler))
        {
            lock.unlock();  // too late in case of steal
            try
            {
                ++m_handler_ptr->m_active_workers;
                handler();
            }
            catch(...)
            {
                // suppress all exceptions
            }
            --m_handler_ptr->m_active_workers;
        }
        else
        {
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
            // m_sema.acquire();
            m_cond_var.wait(lock);
        }
    }
}

}
