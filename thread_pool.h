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
#include <future>
#include <iostream>

#define LOG(FORMAT, MESSAGE...)                             \
    do {                                                    \
        std::printf(FORMAT, ## MESSAGE);                    \
        std::printf("\n");                                  \
        std::fflush(stdout);                                \
    } while (0)

// We need a queue that supports multi-thread SAFE push and pop
template<typename T>
class Queue {
private:
    std::mutex mtx_;
    std::queue<T> q_;

public:
    void push(T& t) {
        std::lock_guard lock(this->mtx_);
        q_.push(t);
    }

    std::optional<T> pop() {
        std::lock_guard lock(this->mtx_);
        if (q_.empty()) return std::nullopt;
        auto t = q_.front();
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
        explicit worker(const int id, thread_pool *ptr) : id_(id), pool_ptr_(ptr) {
            LOG("[WORKER %d]: Construct success", id);
        }

        void operator()() {
            // TODO: thread is likely to be blocked 
            while (!pool_ptr_->killed_) {
                // wait for pool to wake the thread up
                std::optional<std::function<void()>> func;
                // RAII style lock management
                {
                std::unique_lock<std::mutex> lock(pool_ptr_->mtx_);
                pool_ptr_->cv_.wait(lock);
                func = pool_ptr_->q_.pop();
                LOG("[WORKER %d]: Pop a func from queue", this->id_);
                }
                if (!func.has_value()) {
                    LOG("[WORKER %d]: func is nullopt", this->id_);
                    continue;
                }
                LOG("[WORKER %d]: Waken up and assigned a task", this->id_);
                func.value()();
            }
        }

    };

public:
    explicit thread_pool(int n) : thd_vc_(n), killed_(false) {
        init();
        LOG("[THREAD_POOL]: Finish constructing threads");
    };
    ~thread_pool() = default;

    // launch the threads and ensure running until killed
    void init() {
        int n = thd_vc_.size();
        for (int i = 0; i < n; ++i) {
            thd_vc_[i] = std::thread(worker(i, this));
        }
    }

    // We need typename to explicitly state the following part is a type
    // invoke_result takes TYPE as a param, not variable
    template<typename F, typename... Arg>
    auto submit(F f, Arg... args) -> std::future<typename std::invoke_result<F, Arg...>::type> {
        using return_type = std::invoke_result<F, Arg...>::type;
        // We need to get the answer back, thus need a future ==> std::packaged_task
        auto func = std::bind(std::forward<F>(f), std::forward<Arg>(args)...);
        auto ptr = std::make_shared<std::packaged_task<return_type()>>(func);
        // We need to encapsulate it into a std::function, so as to push into the task queue
        std::function<void()> f_to_push = [ptr]() { (*ptr)(); };
        q_.push(f_to_push);
        LOG("[SUBMIT]: Success");
        this->cv_.notify_one();
        LOG("[CV]: Call notify one");
        return ptr->get_future();
    }

    void end() {
        killed_ = true;
        this->cv_.notify_all();
        for (auto& t : thd_vc_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

private:
    std::vector<std::thread> thd_vc_;
    std::mutex mtx_;                    // control the visit of private var
    std::condition_variable cv_;
    bool killed_;
    Queue<std::function<void()>> q_;
    
};

#endif