#include <functional>
#include <memory>
#include <thread>
#include <queue>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <semaphore.h>
#include <unistd.h>

bool ready = false;
std::mutex mutex_thread;
std::condition_variable cond;

std::shared_ptr<std::thread> thread_wait;
std::shared_ptr<std::thread> thread_notify;

void *wait_routine(void)
{
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_thread);
        while (!ready)
            cond.wait(lock);
        ready = false;
        std::cout << "waited!" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    return nullptr;
}
#ifdef USE_FUNC
void notify()
{
    std::unique_lock<std::mutex> lock(mutex_thread);
    ready = true;
    cond.notify_all();
}
#endif
void *notify_routine()
{
    while (true) {
#ifdef USE_FUNC
        notify();
#else
        std::unique_lock<std::mutex> lock(mutex_thread);
        ready = true;
    	cond.notify_all();
#endif
        std::cout << "is ready" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    return nullptr;
}

int main()
{
    thread_wait = std::make_shared<std::thread>(&wait_routine);
    thread_notify = std::make_shared<std::thread>(&notify_routine);
    thread_wait->join();
    thread_notify->join();
    return 0;
}