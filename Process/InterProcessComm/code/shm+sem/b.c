#include "frame_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#define BUF (1024)

typedef struct {
    int is_used;
    char *buf;
} frame_t;
IPC_IDS ids;

int get_frame(frame_t *frame)
{
    void *shm_addr = AttachSharedFrame(&ids, frame, sizeof(frame_t));
    frame->buf = (char *)((unsigned long)shm_addr + sizeof(int));
	return 0;
}

void free_frame(const frame_t *frame)
{
    void *addr = (void *)((unsigned long)frame->buf - sizeof(int));
    DeattachSharedFrame(&ids, addr);
}

void *get(void *arg)
{
    while (1) {
        frame_t frame;
        if (0 == get_frame(&frame)) {
			printf("get frame[%s] success!\n", frame.buf);
            free_frame(&frame);
        }
    }
    return NULL;
}

int main()
{
	GetFrameStart(0, &ids);
    printf("get: shm_id %d, sem_id %d\n", ids.shm_id, ids.sem_id);

	pthread_t tid;
	pthread_create(&tid, NULL, get, NULL);
    pthread_detach(tid);
	pause();
	return 0;
}
