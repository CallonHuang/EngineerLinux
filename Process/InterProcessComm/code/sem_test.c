#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <unistd.h>
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
	sem_buf.sem_flg = 0;
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

int init_sem(int sem_id, int init_value)
{
    union semun sem_union;
    sem_union.val = init_value;
    if (semctl(sem_id, 0, SETVAL, sem_union) < 0) {
        printf("init_sem failed with %d(%s).\n", errno, strerror(errno));
        return -1;
    }
    return 0;
}

int del_sem(int sem_id)
{
    union semun sem_union;
    if (semctl(sem_id, 0, IPC_RMID, sem_union) < 0) {
        printf("del_sem failed with %d(%s).\n", errno, strerror(errno));
        return -1;
    }
    return 0;
}

int main()
{
    pid_t result;
    int sem_id = semget(ftok(".", 'a'), 1, 0666 | IPC_CREAT);
    init_sem(sem_id, 0);
    result = fork();
    if (result < 0) {
        printf("fork failed with %d(%s).\n", errno, strerror(errno));
    } else if (result == 0) {
        printf("Child process will wait for some seconds...\n");
        sleep(5);
        printf("The returned value is %d in the child progress(PID=%d)\n", result, getpid());
        sem_op_('v', sem_id, 0);
    } else {
        sem_op_('p', sem_id, 0);
        printf("The returned value is %d in the father progress(PID=%d)\n", result, getpid());
        del_sem(sem_id);
    }
    return 0;
}


