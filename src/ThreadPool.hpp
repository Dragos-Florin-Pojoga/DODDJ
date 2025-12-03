#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    ThreadPool(size_t threads = std::thread::hardware_concurrency()) : stop(false), active_tasks(0) {
        for (size_t i = 0; i < threads; ++i)
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();

                    int remaining = --active_tasks;
                    if (remaining == 0) {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        wait_condition.notify_all();
                    }
                }
            });
    }

    template <class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
            ++active_tasks;
        }
        condition.notify_one();
    }

    void wait_all() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        wait_condition.wait(lock, [this] { return active_tasks == 0 && tasks.empty(); });
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers)
            worker.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable wait_condition;
    bool stop;
    std::atomic<int> active_tasks;
};
