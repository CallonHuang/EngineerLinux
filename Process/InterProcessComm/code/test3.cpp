#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define KEY_PATH "/usr"

#define CHECK_FUNC_RET(cond, ret, ...) {    \
    if (cond) {                             \
        printf(__VA_ARGS__);                \
        return ret;                         \
    }                                       \
}

int main()
{
    int key = ftok(KEY_PATH, 0);
    int shm_id = shmget(key, 1024, IPC_CREAT | IPC_EXCL | 0666);//shmget(key, 1024, IPC_CREAT | 0666);
    CHECK_FUNC_RET(shm_id < 0, -1, "shmget failed, errno %d(%s)\n", errno, strerror(errno));
    void *addr = shmat(shm_id, nullptr, 0);
    CHECK_FUNC_RET(addr == (void *)-1, -1, "shmat failed, errno %d(%s)\n", errno, strerror(errno));
    struct shmid_ds shmds;
    shmctl(shm_id, IPC_STAT, &shmds);
    printf("no. of current attaches: %ld\n", shmds.shm_nattch);
#ifdef SHMDT
    shmdt(addr);
#endif
#ifndef NO_IPC_RMID
    shmctl(shm_id, IPC_RMID, nullptr);
#endif
    shm_id = shmget(key, 1024, IPC_CREAT | IPC_EXCL | 0666);
    CHECK_FUNC_RET(shm_id < 0, -1, "shmget failed, errno %d(%s)\n", errno, strerror(errno));
    addr = shmat(shm_id, nullptr, 0);
    CHECK_FUNC_RET(addr == (void *)-1, -1, "shmat failed, errno %d(%s)\n", errno, strerror(errno));
#ifdef SHMDT
    shmdt(addr);
#endif
    shmctl(shm_id, IPC_STAT, &shmds);
    printf("no. of current attaches: %ld\n", shmds.shm_nattch);
    shmctl(shm_id, IPC_RMID, nullptr);
    return 0;
}
