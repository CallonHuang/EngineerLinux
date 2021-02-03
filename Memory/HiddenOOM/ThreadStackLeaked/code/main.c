#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>

void *start_routine(void *arg)
{
    printf("hello world!\n");
    return NULL;
}

int main()
{
    int i;
    pthread_t thread;
    pthread_create(&thread, NULL, start_routine, NULL);
    while(1);//do other thing
    return 0;
}
