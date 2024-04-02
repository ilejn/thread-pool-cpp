#include <gtest/gtest.h>

#include <thread_pool/thread_pool.hpp>

#include <thread>
#include <future>
#include <functional>
#include <memory>
#include <chrono>
#include <iostream>

namespace TestLinkage {
size_t getWorkerIdForCurrentThread()
{
    return *tp::detail::thread_id();
}

size_t getWorkerIdForCurrentThread2()
{
    return tp::Worker<std::function<void()>>::getWorkerIdForCurrentThread();
}
}

TEST(ThreadPool, poolGrow)
{

    const size_t NUM_TASKS = 200;

    tp::ThreadPool pool(tp::ThreadPoolOptions().setMaxThreads(50).setMaxFreeThreads(10));
    std::vector<std::packaged_task<int()>> task_vector;
    std::vector<std::future<int>> future_vector;

    for (size_t i = 0; i < NUM_TASKS; ++i)
    {
        task_vector.emplace_back([]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return 42;
        });
    }

    for (size_t i = 0; i < NUM_TASKS; ++i)
    {
        future_vector.emplace_back(task_vector[i].get_future());
    }


    auto begin_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_TASKS; ++i)
    {
        pool.post(task_vector[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }


    std::cout << "active_threads " << pool.getActiveThreads() << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    std::packaged_task<int()> t([]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return 42;
        });

    std::future<int> r = t.get_future();

    pool.post(t);

    ASSERT_EQ(42, r.get());


    std::cout << "active_threads after sleep " << pool.getActiveThreads() << std::endl;


    for (size_t i = 0; i < NUM_TASKS; ++i)
    {
        ASSERT_EQ(42, future_vector[i].get());
    }
    auto end_time = std::chrono::high_resolution_clock::now();


    std::cout << "elapsed milliseconds " << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time).count() << std::endl;

    pool.wait();

    std::cout << "after wait, elapsed milliseconds " << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time).count() << std::endl;


}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
