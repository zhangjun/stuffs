#include "task_queue.h"
#include <iostream>
#include <thread>

struct FackTask {
    uint64_t id;
    int data;    
};

void producer_func(common::task_queue<FackTask> *tasks) {
    static uint64_t id = 0;
    while (true) {
        FackTask *t = new FackTask();
        t->id = id++;
        t->data = rand();
        tasks->push(t);
        std::cout << "[producer][" << std::this_thread::get_id() << "][id:" << t->id << ", data:" << t->data << "]" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void consumer_func(common::task_queue<FackTask> *tasks) {
    while (true) {
        FackTask *t = tasks->pop(10);
        if (t) {
            std::cout << "[consumer][ok][" << std::this_thread::get_id() << "][id:" << t->id << ", data:" << t->data << "]" << std::endl;
            delete t;
        } else {
            std::cout << "[consumer][timeout][" << std::this_thread::get_id() << "]" << std::endl;
        }
    }
}

int main(int argc, char *argv[]) {
    common::task_queue<FackTask> task_queue;
    const int PRODUCER_THREAD_COUNT = 10;
    const int CONSUMER_THREAD_COUNT = 4;
    std::thread p[PRODUCER_THREAD_COUNT];
    for (int i = 0; i < PRODUCER_THREAD_COUNT; i++) {
        p[i] = std::thread(std::bind(producer_func, &task_queue));        
    }
    std::thread c[CONSUMER_THREAD_COUNT];
    for (int i = 0; i < CONSUMER_THREAD_COUNT; i++) {
        c[i] = std::thread(std::bind(consumer_func, &task_queue));    
    }
    
    for (int i = 0; i < PRODUCER_THREAD_COUNT; i++) {
        p[i].join();
    }
    for (int i = 0; i < CONSUMER_THREAD_COUNT; i++) {
        c[i].join();
    }
    
    return 0;
}
