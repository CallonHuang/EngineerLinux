#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int count = 0;
void *stack = NULL;
int stacksize = 8192;
int start_routine(void *arg)
{
    printf("hello world with count(%d)!\n", count++);
    free(stack);
    exit(1);
}

int main()
{
    int i;
    stack = malloc(stacksize);
    if (NULL == stack) {
        printf("thread stack create failed!\n");
        exit(0);
    }
    count++;
    clone(start_routine, (char *)stack + stacksize, CLONE_VM | CLONE_VFORK, 0);
    printf("after thread, count = %d!\n", count);
    while(1);//do other thing
    return 0;
}
