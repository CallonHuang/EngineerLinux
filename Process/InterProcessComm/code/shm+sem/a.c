#include "frame_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF (1024)

int main()
{
	IPC_IDS ids;
	CreateSharedFrame(&ids, sizeof(int) + BUF);
	printf("shm_id %d, sem_id %d\n", ids.shm_id, ids.sem_id);
	while (1) {
		int is_used = 1;
		char *buf = (char *)calloc(1, BUF);
		strcpy(buf, "hello");
		int ret = SendSharedFrame(0, &ids, &is_used, sizeof(is_used), buf, BUF);
		if (ret != 0) {
			printf("SendSharedFrame need wait!\n");
		} else {
			printf("SendSharedFrame success!\n");
		}
		free(buf);
	}
	return 0;
}
