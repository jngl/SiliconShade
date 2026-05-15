#include <retro3d/core/ThreadPool.h>

ThreadPool::ThreadPool(uint32_t num_threads) {
    for (uint32_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();
                    });
                    if (this->stop && this->tasks.empty()) return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
                {
                    std::lock_guard<std::mutex> lock(this->queue_mutex);
                    active_tasks--;
                }
                wait_condition.notify_all();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers) {
        worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks.emplace(std::move(task));
        active_tasks++;
    }
    condition.notify_one();
}

void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    wait_condition.wait(lock, [this] {
        return tasks.empty() && active_tasks == 0;
    });
}
