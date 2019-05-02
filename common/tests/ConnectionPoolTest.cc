#include <iostream>
#include <thread>
#include <string>

#include "ConnectionPool.h"

class connection {
public:
    connection(int id) : id(id) {
    }
    
    ~connection() {
    }
    
    bool test() {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 10));
        if (rand() % 10) {
            std::cout << std::this_thread::get_id() << ": " << id << ", ok" << std::endl;
            return true;           
        } else {
            std::cout << std::this_thread::get_id() << ": " << id << ", fail" << std::endl;
            return false;
        }
    }
    
    bool reconnect() {
        if (rand() % 3) {
            std::cout << std::this_thread::get_id() << ": " << id << ", reconnect ok" << std::endl;
            return true;
        } else {
            std::cout << std::this_thread::get_id() << ": " << id << ", reconnect fail" << std::endl;
            return false;           
        }
    }
private:
    int id;
};

void ThreadFunc(common::connection_pool<connection> *pool) {
    while (true) {
        connection *conn = pool->get();
        if (conn) {
            if (!conn->test()) {
                pool->mark_invalid(conn);
            } else {
                pool->put_back(conn);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
    }
}

int main() {
    common::connection_pool<connection> pool;
    for (int i = 0; i < 10; i++) {
        pool.add(new connection(i));
    }
    
    std::thread td[10];
    for (int i = 0; i < 10; i++) {
        td[i] = std::thread(std::bind(ThreadFunc, &pool));
    }
    
    for (int i = 0; i < 10; i++) {
        td[i].join();
    }
    
    return 0;
}
