#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <sys/resource.h>
#include <sys/time.h>

#define PROCESS_MAX_PARAMS (32)

typedef enum {
    FIFO,
    NICE
} PRIO_TYPE;

typedef struct {
    PRIO_TYPE type;
    int value;
} PRIORITY;

int CreateProcess_ex(const char *cmdstring, PRIORITY *priority)
{
    pid_t pid;
    sigset_t chld_mask, save_mask;
    int status = 0;
    struct sigaction ignore, saveintr, savequit;
    sigaction(SIGINT, &ignore, &saveintr);
    sigaction(SIGQUIT, &ignore, &savequit);
    sigemptyset(&chld_mask);
    sigaddset(&chld_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &chld_mask, &save_mask);
    pid = fork();
    if (pid < 0) {
        status = -1;
    } else if (0 == pid) {
        std::vector<std::string> cmd_elems;
        std::string command = cmdstring;
        size_t pos = 0;
        size_t len = command.length();
        int j = 0;
        char *args[PROCESS_MAX_PARAMS + 3];
        bool redirection = false;
        cmd_elems.reserve(1);
        while (pos < len) {
            int find_pos = command.find(' ', pos);
            if (find_pos < 0) {
                cmd_elems.push_back(command.substr(pos, len - pos));
                break;
            }
            cmd_elems.push_back(command.substr(pos, find_pos - pos));
            pos = find_pos + 1;
        }
        args[0] = args[1] = (char *)cmd_elems[0].c_str();
        j = 2;
        for (int i = 1; i < cmd_elems.size(); i++) {
            char *env = nullptr;
            /*env*/
            if ((cmd_elems[i][0] == '$') && (cmd_elems[i].length() >= 2)) {
                cmd_elems[i].erase(0, 1);
                if ((cmd_elems[i][1] == '{') && (cmd_elems[i][cmd_elems[i].length() - 1] == '}')) {
                    cmd_elems[i].erase(1);
                    cmd_elems[i].erase(cmd_elems[i].length() - 1);
                }
                env = getenv(cmd_elems[i].c_str());
                if (nullptr == env) {
                    continue;
                } else {
                    cmd_elems[i].clear();
                    cmd_elems[i] = env;
                }
            }
            if (redirection) {
                redirection = false;
                int32_t fd = open(cmd_elems[i].c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
                dup2(fd, 1);
                close(fd);
                continue;
            }
            /*redirection*/
            if ((cmd_elems[i][0] == '>') && (cmd_elems[i].length() == 1)) {
                /* Get next */
                redirection = true;
                continue;
            }
            args[j++] = (char *)cmd_elems[i].c_str();
        }
        args[j] = nullptr;
        sigaction(SIGINT, &saveintr, 0L);
        sigaction(SIGQUIT, &savequit, 0L);
        /* Setup the priority of the process */
        {
            struct sched_param setParam;
            pid_t self_pid = getpid();
            if (sched_getparam(self_pid, &setParam) == 0) {
                if (NICE == priority->type) {
                    setParam.sched_priority = 0;
                    sched_setscheduler(self_pid, SCHED_OTHER, &setParam);
                    setpriority(PRIO_PROCESS, 0, priority->value);
                } else {
                    setParam.sched_priority = priority->value;
                    sched_setscheduler(self_pid, SCHED_FIFO, &setParam);
                }
            }
        }
        sigprocmask(SIG_SETMASK, &save_mask, 0L);
        if (execvp(args[1], args + 1) < 0)
            exit(SIGABRT);
        _exit(127); /* exec error */
    } else {
#ifndef NO_WAIT
        while (waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR) {
                status = -1; /* error other than EINTR from waitpid() */
                break;
            }
        }
#endif
    }
    /* restore previous signal actions & reset signal mask */
    if (sigaction(SIGINT, &saveintr, NULL) < 0)
        return(-1);
    if (sigaction(SIGQUIT, &savequit, NULL) < 0)
        return(-1);
    if (sigprocmask(SIG_SETMASK, &save_mask, NULL) < 0)
        return(-1);

    return((status==-1)?-1:0);
}

int main()
{
    PRIORITY priority;
    priority.value = -10;
    priority.type = NICE;
    CreateProcess_ex("./process", &priority); //fork+execl
    while (1);
    return 0;
}
