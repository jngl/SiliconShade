#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <retro3d/core/ThreadPool.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

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

    SUBCASE("wait_all returns only after all results are written") {
        const int N = 16;
        std::vector<int> results(N, 0);

        for (int i = 0; i < N; ++i) {
            pool.enqueue([&results, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                results[i] = i + 1;
            });
        }

        pool.wait_all();

        // All tasks must have finished before wait_all returned.
        for (int i = 0; i < N; ++i) {
            CHECK(results[i] == i + 1);
        }
    }
}
