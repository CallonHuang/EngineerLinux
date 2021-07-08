#include <functional>
#include <memory>
#include <thread>
#include <queue>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <semaphore.h>
#include <unistd.h>

typedef struct {
    struct {
        uint32_t width;
        uint32_t height;
        int field;
        int pformat;
        int vformat;
        int cmode;
        int drange;
        int cmnt;
        uint32_t header[3];
        uint32_t stride[3];
        uint32_t extstride[3];

        uint64_t headerphy[3];
        uint64_t headervir[3];
        uint64_t phyaddr[3];
        uint64_t viraddr[3];
        uint64_t extphyaddr[3];
        uint64_t extviraddr[3];

        int16_t offsettop;
        int16_t offsetbottom;
        int16_t offsetleft;
        int16_t offsetright;

        uint32_t max;
        uint32_t min;

        uint32_t timeref;
        uint64_t pts;

        uint64_t pdata;
        uint32_t framef;
    };
    uint32_t id;
    int mode;
} INFO;

std::mutex mutex_queue1;
std::queue<INFO> queue1;
std::mutex mutex_queue2;
std::queue<INFO> queue2;

#ifdef SEM
sem_t sem;
#else
std::mutex mutex_thread;
std::condition_variable cond;
#endif
bool ready = false;

std::shared_ptr<std::thread> thread_wait;
std::shared_ptr<std::thread> thread_notify1;
std::shared_ptr<std::thread> thread_notify2;

void *wait_routine(void)
{
    while (true) {
#ifdef SEM
        sem_wait(&sem);
#else
        std::unique_lock<std::mutex> lock(mutex_thread);
        while (!ready)
            cond.wait(lock);
        ready = false;
#endif
        std::cout << "waited!" << std::endl;
        std::unique_lock<std::mutex> lock1(mutex_queue1);
        std::unique_lock<std::mutex> lock2(mutex_queue2);
        if (queue1.size() > 0 && queue2.size() > 0) {
            queue1.pop();
            queue2.pop();
            //do something
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    return nullptr;
}
#ifndef SEM
void notify()
{
    std::unique_lock<std::mutex> lock(mutex_thread);
    ready = true;
    cond.notify_all();
}
#endif
void *notify_routine(int idx)
{
    while (true) {
        INFO info;
        if (1 == idx) {
            std::unique_lock<std::mutex> lock1(mutex_queue1);
            queue1.push(std::move(info));
            if (queue1.size() > 4)
                queue1.pop();
        } else {
            std::unique_lock<std::mutex> lock2(mutex_queue2);
            queue2.push(std::move(info));
            if (queue2.size() > 4)
                queue2.pop();
        }
#ifdef SEM
        sem_post(&sem);
#else
        notify();
#endif
        std::cout << idx << "is ready" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    return nullptr;
}

int main()
{
#ifdef SEM
    sem_init(&sem, 0, 0);
#endif
    thread_wait = std::make_shared<std::thread>(&wait_routine);
    thread_notify1 = std::make_shared<std::thread>(&notify_routine, 1);
    thread_notify2 = std::make_shared<std::thread>(&notify_routine, 2);
    thread_wait->join();
    thread_notify1->join();
    thread_notify2->join();
    return 0;
}