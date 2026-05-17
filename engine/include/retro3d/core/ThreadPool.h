#ifndef INC_3D_TEST_THREADPOOL_H
#define INC_3D_TEST_THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    ThreadPool(uint32_t num_threads);
    ~ThreadPool();

    void enqueue(std::function<void()> task);
    void wait_all();

    uint32_t get_num_threads() const { return static_cast<uint32_t>(workers.size()); }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable wait_condition;
    
    std::atomic<uint32_t> active_tasks{0};
    std::atomic<bool> stop{false};
};

#endif //INC_3D_TEST_THREADPOOL_H
