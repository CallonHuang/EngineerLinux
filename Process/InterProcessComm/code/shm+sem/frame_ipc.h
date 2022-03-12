#ifndef FRAME_IPC_H
#define FRAME_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int shm_id;
	int sem_id;
} IPC_IDS;


int CreateSharedFrame(IPC_IDS *ids, int size);

int SendSharedFrame(int flag, IPC_IDS *ids, void *extra_data, int extra_len, void *data, int len);

int GetFrameStart(int flag, IPC_IDS *ids);

void *AttachSharedFrame(IPC_IDS *ids, void *frame, int len);

int DeattachSharedFrame(IPC_IDS *ids, void *addr);

int DestroySharedFrame(IPC_IDS *ids);

#ifdef __cplusplus
}
#endif

#endif
