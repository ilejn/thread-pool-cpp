#include <gtest/gtest.h>

#include <thread_pool/thread_pool.hpp>

#include <thread>
#include <future>
#include <functional>
#include <memory>

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

TEST(ThreadPool, postJob)
{
    tp::ThreadPool pool;

    std::packaged_task<int()> t([]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        throw std::runtime_error("some error");

        return 42;
    });

    std::future<int> r = t.get_future();

    pool.post(t);

    try
    {
        pool.wait();
    }
    catch(std::exception&)
    {
        std::cout << "exception from wait()" << std::endl;
    }

    size_t result = 0;
    try
    {
        result = r.get();
    }
    catch(std::exception&)
    {
        std::cout << "exception from get()" << std::endl;
    }

    ASSERT_EQ(42, result);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
