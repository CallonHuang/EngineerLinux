#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/prctl.h>

pthread_mutex_t mutex_value1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_value2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_value3 = PTHREAD_MUTEX_INITIALIZER;

void *thread_func1(void *param)
{
    prctl(PR_SET_NAME, "thread_func1");
    while(1) {
        pthread_mutex_lock(&mutex_value1);  
        pthread_mutex_lock(&mutex_value2);
        pthread_mutex_unlock(&mutex_value2);
        pthread_mutex_unlock(&mutex_value1);
        sleep(1);
    }
}

void *thread_func2(void *param)
{
    prctl(PR_SET_NAME, "thread_func2");
    while(1) {
        pthread_mutex_lock(&mutex_value2);
        sleep(1);
        pthread_mutex_lock(&mutex_value3);
        pthread_mutex_unlock(&mutex_value3);
        pthread_mutex_unlock(&mutex_value2);
        sleep(1);
    }
}

void *thread_func3(void *param)
{
    prctl(PR_SET_NAME, "thread_func3");
    while(1) {
        pthread_mutex_lock(&mutex_value3);
        sleep(1);
        pthread_mutex_lock(&mutex_value1);
        pthread_mutex_unlock(&mutex_value1);
        pthread_mutex_unlock(&mutex_value3);
        sleep(1);
    }
}

int main()
{
    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, thread_func1, NULL);
    pthread_create(&t2, NULL, thread_func2, NULL);
    pthread_create(&t3, NULL, thread_func3, NULL);
    pause();

    return 0;
}
