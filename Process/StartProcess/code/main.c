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
#define PROCESS_MAX_PARAMS (32)

int CreateProcess(const char *cmdstring) /* with appropriate signal handling */
{
    pid_t pid;
    int status = 0;
    struct sigaction ignore, saveintr, savequit;
    sigset_t chldmask, savemask;

    if (cmdstring == NULL)
        return(1); /* always a command processor with UNIX */

    ignore.sa_handler = SIG_IGN; /* ignore SIGINT and SIGQUIT */
    sigemptyset(&ignore.sa_mask);
    ignore.sa_flags = 0;
    if (sigaction(SIGINT, &ignore, &saveintr) < 0)
        return(-1);
    if (sigaction(SIGQUIT, &ignore, &savequit) < 0)
        return(-1);
    sigemptyset(&chldmask); /* now block SIGCHLD */
    sigaddset(&chldmask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &chldmask, &savemask) < 0)
        return(-1);

    if ((pid = fork()) < 0) {
        status = -1; /* probably out of processes */
    } else if (pid == 0) { /* child */
        /* restore previous signal actions & reset signal mask */
        sigaction(SIGINT, &saveintr, NULL);
        sigaction(SIGQUIT, &savequit, NULL);
        sigprocmask(SIG_SETMASK, &savemask, NULL);
        execl("/bin/sh", "sh", "-c", cmdstring, (char *)0);
        _exit(127); /* exec error */
    } else { /* parent */
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
    if (sigprocmask(SIG_SETMASK, &savemask, NULL) < 0)
        return(-1);

    return((status==-1)?-1:0);
}

int CreateProcess_ex(const char *cmdstring)
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
#if defined(USE_SYSTEM)
    system("./process");
#elif defined(USE_POPEN)
    FILE *fp = popen("./process", "w");
    if (fp == (void *)0)
        return -1;
    pclose(fp);
#else
    CreateProcess("./process"); //fork+execl
#endif
    while (1);
    return 0;
}
