# pstack

在日常开发过程中，难免会遇到卡死相关的问题，其中一个可用的工具就是 *pstack*，下载路径为 https://github.com/bangerlee/strace_pstack/blob/master/pstack.sh ，为了方便也将其提供在了 *code* 目录下。实际上，*pstack* 是可以利用 *gdb* 实现的一个脚本，但是由于其封装的易用性，让很多人更愿意将其作为单一的工具来使用。本文涉及的测试程序也放在了 *code* 目录下。

### 使用方式

这里以本文提供的例子来演示其使用方法：

```shell
$ gcc -g main.c -o main -lpthread
$ ./main &
[1] 2455
$ routine[0]...
routine[1]...
routine[2]...
$ sudo bash pstack.sh 2455
Thread 4 (Thread 0x7fd1de7b0700 (LWP 2458)):
#0  0x00007fd1df8c47a0 in __GI___nanosleep (requested_time=requested_time@entry=0x7fd1de7afe90, remaining=remaining@entry=0x7fd1de7afe90) at ../sysdeps/unix/sysv/linux/nanosleep.c:28
#1  0x00007fd1df8c467a in __sleep (seconds=0) at ../sysdeps/posix/sleep.c:55
#2  0x00007fd1e02008b5 in routine (args=0x2) at main.c:14
#3  0x00007fd1dfbe76db in start_thread (arg=0x7fd1de7b0700) at pthread_create.c:463
#4  0x00007fd1df90171f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95
Thread 3 (Thread 0x7fd1defc0700 (LWP 2457)):
#0  0x00007fd1df8c47a0 in __GI___nanosleep (requested_time=requested_time@entry=0x7fd1defbfe90, remaining=remaining@entry=0x7fd1defbfe90) at ../sysdeps/unix/sysv/linux/nanosleep.c:28
#1  0x00007fd1df8c467a in __sleep (seconds=0) at ../sysdeps/posix/sleep.c:55
#2  0x00007fd1e02008b5 in routine (args=0x1) at main.c:14
#3  0x00007fd1dfbe76db in start_thread (arg=0x7fd1defc0700) at pthread_create.c:463
#4  0x00007fd1df90171f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95
Thread 2 (Thread 0x7fd1df7d0700 (LWP 2456)):
#0  0x00007fd1df8c47a0 in __GI___nanosleep (requested_time=requested_time@entry=0x7fd1df7cfe90, remaining=remaining@entry=0x7fd1df7cfe90) at ../sysdeps/unix/sysv/linux/nanosleep.c:28
#1  0x00007fd1df8c467a in __sleep (seconds=0) at ../sysdeps/posix/sleep.c:55
#2  0x00007fd1e02008b5 in routine (args=0x0) at main.c:14
#3  0x00007fd1dfbe76db in start_thread (arg=0x7fd1df7d0700) at pthread_create.c:463
#4  0x00007fd1df90171f in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:95
Thread 1 (Thread 0x7fd1e00f0740 (LWP 2455)):
#0  0x00007fd1dfbf1fc2 in __libc_pause () at ../sysdeps/unix/sysv/linux/pause.c:30
#1  0x00007fd1e0200913 in main () at main.c:25
```

通过打印出所有线程的调用栈，可以很好地分析各个线程的运行情况，从而找到死锁/死循环导致设备卡死的根因。

在部分嵌入式平台上使用时，经常发现打印所有线程堆栈时只会显示进程自身的堆栈，此时可考虑编译调试版的 *libc库* 后再次进行尝试，*libc库* 的源码路径为 http://ftp.gnu.org/gnu/glibc/ 。这里将以 *海思3536* 编译 *glibc-2.29* 为例：

```shell
$ cd glibc-2.29
$ mkdir build/ out/
$ cd build
$ ../configure --prefix=$PWD/../out --host=arm-hisiv400-linux-gnueabi --enable-add-on=nptl CC=arm-hisiv400_v2-linux-gcc CXX=arm-hisiv400_v2-linux-g++ --disable-werror --disable-sanity-checks CFLAGS="-g -Os -funwind-tables"
```

最后将编译出的成果物 *out/lib* 中的 *libpthread-2.29.so* 替换掉目标单板默认带的库即可。

