#include "frame_ipc.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>

#define CREAT 0

static int g_flag = 0;
static void **g_addr = NULL;
union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                (Linux-specific) */
};

int sem_op_(char opt, int sem_id, int sem_no)
{
	struct sembuf sem_buf;
	sem_buf.sem_num = sem_no;
#if 0
	sem_buf.sem_flg = 0;
#else
	sem_buf.sem_flg = SEM_UNDO;
#endif
	if (opt == 'p') {
		sem_buf.sem_op = -1;
	} else {
		sem_buf.sem_op = 1;
	}
	if (semop(sem_id, &sem_buf, 1) == -1) {
		printf("semop failed with %d(%s).\n", errno, strerror(errno));
		return -1;
	}
	return 0;
}

int CreateSharedFrame(IPC_IDS *ids, int size)
{
	int sem_id;
	union semun dummy;
	key_t key = ftok("/usr/lib", 's' + g_flag);
	int shm_id = shmget(key, size, IPC_CREAT | IPC_EXCL | 0666);
	if (shm_id < 0) {
		if (errno == EEXIST) {
			printf("[FRAME_IPC] shm is exist, now remove it.\n");
			shm_id = shmget(key, 0, 0);
			shmctl(shm_id, IPC_RMID, 0);
			shm_id = shmget(key, size, IPC_CREAT | IPC_EXCL | 0666);
		}
	}
	key = ftok("/usr/lib", 'a' + g_flag);
	sem_id = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);
	if (sem_id < 0) {
		if (errno == EEXIST) {
			printf("[FRAME_IPC] sem_id is exist, now remove it.\n");
			sem_id = semget(key, 1, 0666);
			semctl(sem_id, 1, IPC_RMID, dummy);
			sem_id = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);
		}
	}
	dummy.val = 0;
	int ret = semctl(sem_id, CREAT, SETVAL, dummy);
	printf("CREAT SETVAL ret: %d\n", ret);
	printf("CREAT value %d\n", semctl(sem_id, CREAT, GETVAL, dummy));
	ids->shm_id = shm_id;
	ids->sem_id = sem_id;
	void *addr = shmat(ids->shm_id, NULL, 0);
	*(int *)addr = 0;
	shmdt(addr);
	g_flag++;
	g_addr = (void **)realloc(g_addr, g_flag * sizeof(void *));
	memset(g_addr, 0, g_flag * sizeof(void *));
	return 0;
}

int SendSharedFrame(int flag, IPC_IDS *ids, void *extra_data, int extra_len, void *data, int len)
{
	if (g_addr[flag] == NULL)
		g_addr[flag] = shmat(ids->shm_id, NULL, 0);
	
	if (*(int *)(g_addr[flag]) == 0) {
		if (g_addr[flag] == (void *)-1) {
			printf("shmat failed with %d(%s).\n", errno, strerror(errno));
			return -1;
		}
		if (extra_data != NULL && extra_len > 0)
			memcpy(g_addr[flag], extra_data, extra_len);
		if (data != NULL && len > 0)
			memcpy((void *)((unsigned long)g_addr[flag] + extra_len), data, len);
		sem_op_('v', ids->sem_id, CREAT);
	} else {
		return -1;
	}
	return 0;
}

int GetFrameStart(int flag, IPC_IDS *ids)
{
	int sem_id;
	key_t key = ftok("/usr/lib", 's' + flag);
	ids->shm_id = shmget(key, 0, 0);
	key = ftok("/usr/lib", 'a' + flag);
	ids->sem_id = semget(key, 1, 0666);
	return (ids->shm_id < 0 || ids->sem_id < 0) ? -1 : 0;
}

void *AttachSharedFrame(IPC_IDS *ids, void *frame, int len)
{
	sem_op_('p', ids->sem_id, CREAT);
	
	void *addr = shmat(ids->shm_id, NULL, 0);
	
	memcpy(frame, addr, len);
	return addr;
}

int DeattachSharedFrame(IPC_IDS *ids, void *addr)
{
	*(int *)addr = 0;
	shmdt(addr);
}


int DestroySharedFrame(IPC_IDS *ids)
{
	union semun dummy;
	shmctl(ids->shm_id, IPC_RMID, 0);
	semctl(ids->sem_id, 1, IPC_RMID, dummy);
}



