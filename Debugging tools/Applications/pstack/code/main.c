#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *routine(void *args)
{
    int i = (int)args;
    while (1) {
        pthread_mutex_lock(&mutex);
        printf("routine[%d]...\n", i);
        pthread_mutex_unlock(&mutex);
        sleep(5);
    }
    return NULL;
}

int main()
{
    pthread_t tid[3];
    int i = 0;
    for (;i < 3; i++)
        pthread_create(&tid[i], NULL, routine, i);
    pause();
}
