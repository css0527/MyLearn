# 线程安全队列 (ThreadSafeQueue) 学习笔记

## 1. 项目概述

本项目实现了一个线程安全的队列模板类 `ThreadSafeQueue<T>`，用于多线程环境下的生产者-消费者模式。该队列使用互斥锁（`std::mutex`）和条件变量（`std::condition_variable`）来保证线程安全。

### 1.1 核心特性

- ✅ **线程安全**：所有操作都有互斥锁保护
- ✅ **阻塞/非阻塞**：支持阻塞弹出、非阻塞尝试弹出
- ✅ **优雅停止**：`stop()` 方法可安全唤醒所有等待线程
- ✅ **RAII 风格**：自动管理资源，防止死锁
- ✅ **移动语义**：使用 `std::move` 减少拷贝开销

---

## 2. 类设计

### 2.1 成员变量

```cpp
template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> q;                    // 底层队列
    mutable std::mutex mtx;             // 互斥锁（mutable 允许 const 方法加锁）
    std::condition_variable cv;         // 条件变量
    bool stopped = false;               // 停止标志
};
```

### 2.2 关键方法

| 方法 | 功能 | 阻塞性 |
|------|------|--------|
| `push(T val)` | 入队 | 非阻塞 |
| `pop(T& out)` | 出队 | 阻塞 |
| `try_pop(T& out)` | 尝试出队 | 非阻塞 |
| `size()` | 获取队列大小 | 非阻塞 |
| `empty()` | 检查是否为空 | 非阻塞 |
| `stop()` | 停止队列 | 非阻塞 |
| `reset()` | 重置停止状态 | 非阻塞 |

---

## 3. 核心实现解析

### 3.1 入队操作 (`push`)

```cpp
void push(T val) {
    std::lock_guard<std::mutex> lock(mtx);  // 自动加锁/解锁
    q.push(std::move(val));                  // 移动语义，避免拷贝
    cv.notify_one();                         // 唤醒一个等待的消费者
}
```

**要点**：
- 使用 `lock_guard` 实现 RAII 锁管理
- `notify_one()` 只唤醒一个线程，避免惊群效应

### 3.2 阻塞出队 (`pop`)

```cpp
bool pop(T& out) {
    std::unique_lock<std::mutex> lock(mtx);  // 可手动解锁/加锁
    cv.wait(lock, [this] { 
        return !q.empty() || stopped;        // 等待条件：队列非空或已停止
    });
    
    if (stopped && q.empty()) {
        return false;                        // 队列已停止且为空
    }
    
    out = std::move(q.front());
    q.pop();
    return true;
}
```

**要点**：
- 使用 `unique_lock` 而非 `lock_guard`（因为条件变量需要手动管理锁）
- `wait()` 会自动释放锁，收到通知后重新获取
- Lambda 表达式作为条件谓词，防止虚假唤醒

### 3.3 停止机制 (`stop`)

```cpp
void stop() {
    std::lock_guard<std::mutex> lock(mtx);
    stopped = true;
    cv.notify_all();  // 唤醒所有等待的线程
}
```

**要点**：
- `notify_all()` 唤醒所有消费者
- 消费者检测到 `stopped && empty()` 时返回 `false`，退出循环

---

## 4. 生产者-消费者模式

### 4.1 生产者函数

```cpp
void producer(ThreadSafeQueue<int>& q, int id, int count) {
    // 生成随机数并推入队列
    for (int i = 0; i < count; ++i) {
        int val = dis(gen);
        q.push(val);                        // 非阻塞入队
        cout << "[P" << id << "] 生产: " << val << endl;
        this_thread::sleep_for(chrono::milliseconds(50));
    }
    cout << "[P" << id << "] 完成" << endl;
}
```

### 4.2 消费者函数

```cpp
void consumer(ThreadSafeQueue<int>& q, int id) {
    int val;
    while (q.pop(val)) {                    // 阻塞等待数据
        cout << "[C" << id << "] 消费: " << val << endl;
        this_thread::sleep_for(chrono::milliseconds(80));
    }
    cout << "[C" << id << "] 停止" << endl; // 队列停止时退出
}
```

### 4.3 主流程

```cpp
int main() {
    ThreadSafeQueue<int> q;
    
    // 创建线程
    thread p1(producer, ref(q), 1, 10);
    thread p2(producer, ref(q), 2, 10);
    thread c1(consumer, ref(q), 1);
    thread c2(consumer, ref(q), 2);
    
    // 等待生产者完成
    p1.join();
    p2.join();
    
    // 给消费者一些时间清空队列
    this_thread::sleep_for(seconds(2));
    
    // 停止队列，让消费者退出
    q.stop();
    c1.join();
    c2.join();
}
```

---

## 5. 编译与运行

### 5.1 编译命令

```bash
# Windows (MinGW)
g++ -std=c++17 -pthread main.cpp -o ts_queue.exe

# Linux/macOS
g++ -std=c++17 -pthread main.cpp -o ts_queue
```

### 5.2 运行结果示例

运行 .\ts_queue.exe
```
[P1] 生产: 42 (队列大小: 0)
[P2] 生产: 87 (队列大小: 1)
[C1] 消费: 42 (剩余: 1)
[P1] 生产: 65 (队列大小: 1)
[C2] 消费: 87 (剩余: 0)
...
```

---

## 6. 扩展功能（可选）

### 6.1 超时弹出

```cpp
bool pop_timeout(T& out, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mtx);
    bool ret = cv.wait_for(lock, 
        std::chrono::milliseconds(timeout_ms),
        [this] { return !q.empty() || stopped; });
    
    if (!ret || (stopped && q.empty())) {
        return false;
    }
    out = std::move(q.front());
    q.pop();
    return true;
}
```

### 6.2 批量入队

```cpp
void push_batch(const std::vector<T>& items) {
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& item : items) {
        q.push(item);
    }
    cv.notify_all();
}
```

### 6.3 清空队列

```cpp
void clear() {
    std::lock_guard<std::mutex> lock(mtx);
    std::queue<T> empty;
    std::swap(q, empty);
}
```

---

## 7. 常见问题

### 7.1 为什么禁用拷贝构造？

```cpp
ThreadSafeQueue(const ThreadSafeQueue&) = delete;
ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
```

**原因**：避免多个线程操作同一个队列的副本，导致数据不一致。

### 7.2 为什么 `mutex` 声明为 `mutable`？

```cpp
mutable std::mutex mtx;  // 允许在 const 方法中加锁
```

**原因**：`size()` 和 `empty()` 是 const 方法，但仍需加锁保证线程安全。

### 7.3 虚假唤醒是什么？

条件变量可能在没有 `notify` 的情况下被唤醒（系统原因）。使用 Lambda 条件谓词可以避免：

```cpp
cv.wait(lock, [this] { return !q.empty() || stopped; });
```

---

## 8. 学习要点总结

1. **互斥锁 + 条件变量**是多线程同步的核心工具
2. **RAII 模式**（`lock_guard`/`unique_lock`）能有效防止死锁
3. **条件变量**的 `wait()` 必须配合谓词使用
4. **生产者-消费者模式**是经典的多线程设计模式
5. **停止标志** + `notify_all()` 实现优雅退出
6. **移动语义**可以提升性能，避免不必要的拷贝

