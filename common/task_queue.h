#pragma once

#ifndef __TML_TASK_QUEUE_INC__
#define __TML_TASK_QUEUE_INC__

#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

namespace common {

template<typename T>
class task_queue {
public:
    task_queue() {
    }
    
    bool push(T *t) {
        std::unique_lock<std::mutex> lock(mutex);
        tasks.push(t);
        cond.notify_one();
        return true;
    }
    
    T* pop(int timeout = 0) {
        std::unique_lock<std::mutex> lock(mutex);
        if (!tasks.empty()) {
            T *t = tasks.front();
            tasks.pop();
            return t;
        }
        while (tasks.empty()) {
            if (timeout > 0) {
                if (std::cv_status::timeout == cond.wait_for(lock, std::chrono::milliseconds(timeout))) {
                    return NULL;
                }
            } else {
                cond.wait(lock);
            }
        }
        T *t = tasks.front();
        tasks.pop();
        return t;
    }
    
private:
    std::queue<T *> tasks;
    std::mutex mutex;
    std::condition_variable cond;
};

}


#endif //__TML_TASK_QUEUE_INC__