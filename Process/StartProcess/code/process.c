#include <stdio.h>
#include <unistd.h>

int main()
{
#ifdef START_DEAMON
    int pid = fork();
    if (pid < 0) {
        perror("fork failed!\n");
    } else if (pid == 0) {
        setsid();
        chdir("/");
#endif
        while (1) {
            printf("Process is running...\n");
            sleep(10);
        }
#ifdef START_DEAMON
    } else {
        //father stop here
    }
#endif
    return 0;
}
