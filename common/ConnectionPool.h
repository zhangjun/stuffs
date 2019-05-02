
#pragma once

#ifndef __TML_CONNECTION_POOL_INC__
#define __TML_CONNECTION_POOL_INC__

#include <queue>
#include <set>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>

namespace common {
template<typename T>
class connection_pool {
public:
    //auto_reconnect default true if you don't want auto connect, pass false
    connection_pool(bool auto_reconnect = true) {
        if (auto_reconnect) {
            std::thread(std::bind(reconnect, this));
        }
    }
    
    //add a connection to pool
    bool add(T *conn) {
        //std::scoped_lock lock(mutex);
        std::unique_lock<std::mutex> lock(mutex);
        idle.push(conn);
        cond.notify_one();
        return true;
    }
    
    //get a connection from pool
    T* get(int timeout = 0) {
        //std::scoped_lock lock(mutex);
        std::unique_lock<std::mutex> lock(mutex);
        if (!idle.empty()) {
            T* conn = idle.front();
            idle.pop();
            in_use.insert(conn);
            return conn;
        }
        while (idle.empty()) {
            if (timeout > 0) {
                if (std::cv_status::timeout == cond.wait_for(lock, std::chrono::milliseconds(timeout))) {
                    return NULL;
                }
            } else {
                cond.wait(lock);
            }
        }
        T* conn = idle.front();
        idle.pop();
        in_use.insert(conn);
        return conn;
    }
    
    //put a connection back to pool
    bool put_back(T *conn) {
        //std::scoped_lock lock(mutex);
        std::unique_lock<std::mutex> lock(mutex);
        idle.push(conn);
        in_use.erase(conn);
        cond.notify_one();
        return true;
    }
    
    //mark a connection as invalid
    bool mark_invalid(T* conn) {
        //std::scoped_lock lock(mutex);
        std::unique_lock<std::mutex> lock(mutex);
        in_use.erase(conn);
        bad.push(conn);
        return true;
    }
    
private:
    //auto reconnect in a seperate thread
    static void reconnect(connection_pool<T> *pool) {
        while (true) {
            if (pool->bad.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            T* conn = NULL;
            {
                //std::scoped_lock lock(pool->mutex);
                std::unique_lock<std::mutex> lock(pool->mutex);
                if (!pool->bad.empty()) {
                    conn = pool->bad.front();
                    pool->bad.pop();
                }
            }
            if (conn) {
                if (conn->reconnect()) {
                    //std::scoped_lock lock(pool->mutex);
                    std::unique_lock<std::mutex> lock(pool->mutex);
                    pool->idle.push(conn);
                    pool->cond.notify_one();
                } else {
                    //std::scoped_lock lock(pool->mutex);
                    std::unique_lock<std::mutex> lock(pool->mutex);
                    pool->bad.push(conn);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    std::queue<T *> idle;
    std::queue<T *> bad;
    std::set<T *> in_use;
    std::mutex mutex;
    std::condition_variable cond;
};
}

#endif //__TML_CONNECTION_POOL_INC__
