# 细数IPC的坑

## Content

- [简介](#简介)
- [mqueue](#mqueue)
  - [`mq_open`](#mq_open)
  - [`mq_timedxxx`](#mq_timedxxx)

- [socket](#socket)
- [*SO_SNDTIMEO* / *SO_RCVTIMEO* / *select*](#so_sndtimeo--so_rcvtimeo--select) 
  
- [附-socket+mqueue=Ipc类](#附-socket+mqueue=Ipc类)

---

## 简介

进程间通信（*Inter-Process Communication*）的使用是Linux应用开发中不可避免的一环，那么在工程应用中使用 *IPC* 有哪些注意的点是本节描述的重点。

## mqueue

### `mq_open`

在工程应用中，随着需求和代码量的增大，比较容易出现文件描述符超出限制的情况，对于POSIX消息队列来说，其错误则可能为 *EMFILE*。产生这种错误的原因一般有两点：

- 文件描述符超出限制
- 消息队列大小超出了总大小限制

在工程中，可以通过如下几个命令进行确认是两个问题中的哪一种，方便对于具体问题进行修正：

```shell
$ ls -al /proc/$pid/fd | wc -l
      123
$ ulimit -a
core file size          (blocks, -c) 0
data seg size           (kbytes, -d) unlimited
file size               (blocks, -f) unlimited
pending signals                 (-i) 1024
max locked memory       (kbytes, -l) 32
max memory size         (kbytes, -m) unlimited
open files                      (-n) 1024
pipe size            (512 bytes, -p) 8
POSIX message queues     (bytes, -q) 819200
stack size              (kbytes, -s) 10240
cpu time               (seconds, -t) unlimited
max user processes              (-u) 4096
virtual memory          (kbytes, -v) unlimited
file locks                      (-x) unlimited
$ mkdir /mnt/mqueue;mount -t none mqueue /mnt/mqueue
$ cat /mnt/mqueue/*
QSIZE:129 NOTIFY:2 SIGNO:0 NOTIFY_PID:8260
...
```

1. 对于文件描述符是否超出限制，可以使用第一个 `ls` 命令结合第二个 `ulimit` 命令的方式查看
2. 如果不是文件描述符的个数限制，基本可以往 “消息队列大小超出了总大小限制” 这个方向排查，第三和第四个命令只能看到当前系统各个消息队列缓存的大小，即 *QSIZE* ，并不能看到申请的大小总和（至少当前Linux默认不会将其展示出来），因此完全下结论需要将所有的 `mq_open` 进行捕获并累加

解决方法也不难，只需要在应用层使用 `setrlimt` 函数修改限制即可：

```c
struct rlimit r;
getrlimit(RLIMIT_MSGQUEUE, &r);
r.rlim_cur = r.rlim_max = (20<<20);
setrlimt(RLIMIT_MSGQUEUE, &r);
```

### *mq_timedxxx*

在消息队列接收/发送数据时，通常使用的接口是 `mq_receive` / `mq_send`，默认是阻塞的，当开发人员需要使用带超时时间的接收/发送函数时，就需要使用 `mq_timedreceive` / `mq_timedsend` 。

值得注意的是，它们和常见的超时函数不同， `mq_timedreceive` / `mq_timedsend` 使用的是 `struct timespec` 结构，只能指定秒和纳秒两种单位的时间，且它们所指的超时是针对当前时间而言的，下面列出了其正确的使用方式：

```c
struct timespec abs_timeout;
clock_gettime(CLOCK_REALTIME, &abs_timeout);
/* timeout：ms */
abs_timeout.tv_sec += timeout / 1000;
abs_timeout.tv_nsec += (timeout % 1000) * 1000 * 1000;
if (abs_timeout.tv_nsec > 1000000000) {
    abs_timeout.tv_nsec -= 1000000000;
    abs_timeout.tv_sec += 1;
}
#if SEND
ret = mq_timedsend(ipc_info_.ipc_mq, buf, len, 50, &abs_timeout);
#else
ret = mq_timedreceive(ipc_info_.ipc_mq, buf, len, nullptr, &abs_timeout);
#endif
```

注意对 `tv_nsec` 大于 *1000000000* 的处理，因为当 `tv_nsec` 大于 *1000000000* 时，应当 “进位” 为 `tv_sec` 。

---

## socket

### *SO_SNDTIMEO* / *SO_RCVTIMEO* / *select*

和 *mq_timedxxx* 如出一辙，`socket` 使用中也不免要对 `recv` / `recvfrom` / `accept` / `send` / `sendto` / `connect` 进行超时设定，这一般在应用上需要通过 `setsockopt` 函数来完成，当然如果需要更加准确的超时，那么 `select` 是一个更好的选择：

```c++
#if SEND
struct timeval value;
socklen_t value_len = sizeof(value);
int sock_ret;
/* timeout：ms */
int sec = timeout / 1000;
int usec = (timeout % 1000) * 1000;
sock_ret = getsockopt(ipc_info_.socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &value_len);
if ((sock_ret != 0) || (value.tv_sec != sec)
    || (value.tv_usec != usec)) {
    value.tv_sec  = sec;
    value.tv_usec = usec;
    if (sock_ret != 0) LOG_WARN("getsockopt failed, error = %d(%s).\n", errno, strerror(errno));
    /* do not need most accurate. */
    sock_ret = setsockopt(ipc_info_.socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, value_len);
    if (sock_ret != 0) LOG_WARN("setsockopt failed, error = %d(%s).\n", errno, strerror(errno));
}
ret = sendto(ipc_info_.socket, buf, len, 0, (struct sockaddr *)&addr, addr_len);
#else
struct timeval value;
int select_ret;
fd_set fds;
FD_ZERO(&fds);
FD_SET(ipc_info_.socket, &fds);
value.tv_sec  = timeout / 1000;
value.tv_usec = (timeout % 1000) * 1000;
/* more accurate than setsockopt */
select_ret = select(ipc_info_.socket+1, &fds, nullptr, nullptr, &value);
if (0 == select_ret) {
    return 0;
} else if (-1 == select_ret) {
    //LOG_ERROR("select failed, error = %d(%s).\n", errno, strerror(errno));
    return 0;
}
ret = recvfrom(ipc_info_.socket, buf, len, 0, (struct sockaddr *)&addr, &addr_len);
#endif
```

值得注意的是，其超时结构为 `struct timeval` 只能指定秒和微秒两种单位的时间。



---

## 附-socket+mqueue=Ipc类

在本节的code/ipc文件夹中给出了一个C++综合 `socket` 和 `mqueue` 实现的 *Ipc* 类，文中的部分代码片段也是截取自其中，供大家参考。









