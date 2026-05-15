#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <retro3d/core/ThreadPool.h>
#include <atomic>

TEST_CASE("ThreadPool Operations") {
    ThreadPool pool(4);
    
    SUBCASE("Stress Test Task Execution and Wait") {
        std::atomic<int> counter{0};
        const int iterations = 1000;
        const int num_tasks = 20;
        
        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < num_tasks; ++i) {
                pool.enqueue([&counter]() {
                    counter++;
                });
            }
            pool.wait_all();
        }
        CHECK(counter == iterations * num_tasks);
    }
}
