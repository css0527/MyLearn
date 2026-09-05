#include "ThreadSafeQueue.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

using namespace std;

void producer(ThreadSafeQueue<int>& q, int id, int count) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);

    for (int i = 0; i < count; ++i) {
        int val = dis(gen);
        q.push(val);
        cout << "Producer " << id << " pushed: " << val
             << " (队列大小: " << q.size() << ")" << endl;
        this_thread::sleep_for(std::chrono::milliseconds(50 + dis(gen) % 50));
    }
        std::cout << "Producer " << id << " finished producing." << std::endl;
    
}

void consumer(ThreadSafeQueue<int>& q, int id) {
    int val;
    while (q.pop(val)) {
            cout << "Consumer " << id << " popped: " << val
                 << " (剩余: " << q.size() << ")" << endl;
            this_thread::sleep_for(std::chrono::milliseconds(80));
        } 
        std::cout << "Consumer " << id << " finished consuming." << std::endl;
}

int main() {
    ThreadSafeQueue<int> q;

    // Create producer and consumer threads
    std::thread p1(producer, std::ref(q), 1, 10);
    std::thread p2(producer, std::ref(q), 2, 10);
    std::thread c1(consumer, std::ref(q), 1);   
    std::thread c2(consumer, std::ref(q), 2);

    // Wait for producers to finish
    p1.join();
    p2.join();

    // Wait for a moment to let consumers finish processing
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Stop the queue and notify consumers
    q.stop();
    c1.join();
    c2.join();  

    cout << "All producers and consumers have finished." << endl;
    return 0;
}