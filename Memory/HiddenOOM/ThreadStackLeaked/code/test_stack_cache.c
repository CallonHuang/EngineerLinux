#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>

void *start_routine(void *arg)
{
    pthread_attr_t attr;
    size_t stacksize;
    void *stackaddr = NULL;
    pthread_t thread = pthread_self();
    pthread_getattr_np(thread, &attr);
    pthread_attr_getstack(&attr, &stackaddr, &stacksize);
    pthread_attr_destroy(&attr);
    printf("thread[%ld] will finished: stackaddr[%p] stacksize[%ld]\n", thread, stackaddr, stacksize);
    return NULL;
}

int pthread_spawn(pthread_t *thread, unsigned int flag, void *(*start_routine) (void *), void *arg)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (flag & 0x1L)
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (flag > 1)
        pthread_attr_setstacksize(&attr, (flag & ~(0x1L)));
    return pthread_create(thread, &attr, start_routine, arg);
}

int main()
{
    int i;
    pthread_t thread[8];
    void *ret = NULL;
    const unsigned int stacksize[8] = {0, 128*1024, 512*1024, 2048*1024, 128*1024, 256*1024, 1024*1024, 0};
    for (i = 0; i < 4; i++) {
    	pthread_spawn(&thread[i], stacksize[i], start_routine, NULL);
    	printf("thread[%ld] created with stacksize[%u].\n", thread[i], (unsigned int)(stacksize[i] & ~(0x1L)));
    }
    for (i = 0; i < 4; i++)
    	pthread_join(thread[i], &ret);
    printf("------------------------------------------------------\n");
    for (; i < 7; i++) {
        pthread_spawn(&thread[i], stacksize[i], start_routine, NULL);
    	printf("thread[%ld] created with stacksize[%u].\n", thread[i], (unsigned int)(stacksize[i] & ~(0x1L)));
    }
    pthread_spawn(&thread[i], stacksize[i], start_routine, NULL);
    printf("thread[%ld] created with stacksize[%u].\n", thread[i], (unsigned int)(stacksize[i] & ~(0x1L)));
    for (i = 4; i < 8; i++)
        pthread_join(thread[i], &ret);
    return 0;
}
