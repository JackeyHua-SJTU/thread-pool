#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <type_traits>
#include <vector>
#include <queue>
#include <thread>
#include <functional>
#include <utility>
#include <memory>
#include <optional>

// We need a queue that supports multi-thread SAFE push and pop
template<typename T>
class Queue {
private:
    std::mutex mtx_;
    std::queue<T> q_;

public:
    void push(T&& t) {
        std::lock_guard lock(this->mtx_);
        q_.push(std::move(t));
    }

    std::optional<T> pop() {
        std::lock_guard(this->mtx_);
        if (q_.empty()) return std::nullopt;
        t = q_.front();
        q_.pop();
        return t;
    }

    auto size() const {
        return q_.size();
    }

    bool empty() const {
        return 0 == this->size();
    }

};

// We need a task that keeps running until the pool is killed

// ? How to dispatch work to worker
class thread_pool {
private:
    class worker {
    private:
        const int id_;
        thread_pool* const pool_ptr_;

    public:
        explicit worker(const int id, thread_pool *ptr) : id_(id), pool_ptr_(ptr) {}

        void operator()() {
            while (!pool_ptr_->killed_) {
                // wait for pool to wake the thread up
                std::unique_lock<std::mutex> lock(pool_ptr_->mtx_);
                pool_ptr_->cv.wait(lock);
                const auto t = pool_ptr_->q_.pop();
                if (!t.has_value()) continue;
                t.value()();
            }
        }

    };

public:
    explicit thread_pool(int n) : thd_vc_(n), killed_(false) {};

    // launch the threads and ensure running until killed
    void init() {
        int n = thd_vc_.size();
        for (int i = 0; i < n; ++i) {
            thd_vc_[i] = std::thread(worker(i, this));
        }
    }

    template<typename F, typename... Arg>
    auto submit(F f, Arg... args) {
        
    }

private:
    std::vector<std::thread> thd_vc_;
    std::mutex mtx_;                    // control the visit of private var
    std::condition_variable cv;
    bool killed_;
    Queue<std::function<void()>> q_;
    

public:
    


};


#endif