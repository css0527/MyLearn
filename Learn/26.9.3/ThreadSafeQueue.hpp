#ifndef THREAD_SAFE_QUEUE_HPP
#define THREAD_SAFE_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> q;
    mutable std::mutex mtx;
    std::condition_variable cv;
    bool stopped = false;

public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;

    // Disable copy and move semantics
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    // Add an item to the queue
    void push(T val) {
        std::lock_guard<std::mutex> lock(mtx);
        q.push(std::move(val));
        cv.notify_one();
    }

    //出队
    bool pop(T& out) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { 
            return !q.empty() || stopped; 
        });

        if (stopped && q.empty()) {
            return false; // Queue is stopped and empty
        }

        out = std::move(q.front());
        q.pop();
        return true;
    }

    //尝试弹出（非阻塞)
    bool try_pop(T& out) {
        std::lock_guard<std::mutex> lock(mtx);
        if (q.empty()) {
            return false; // Queue is empty
        }
        out = std::move(q.front());
        q.pop();
        return true;
    }

    //获取队列大小
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return q.size();
    }

    //检查是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return q.empty();
    }

    //停止队列（唤醒所有等待线程)
    void stop() {
        std::lock_guard<std::mutex> lock(mtx);
        stopped = true;
        cv.notify_all();
    }

    //重置停止状态
    void reset() {
        std::lock_guard<std::mutex> lock(mtx);
        stopped = false;
    }
};

#endif // THREAD_SAFE_QUEUE_HPP