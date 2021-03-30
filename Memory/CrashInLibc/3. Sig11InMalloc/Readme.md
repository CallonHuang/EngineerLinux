# SIGSEGV In Malloc

这里主要讨论涉及的第三类问题，具体描述如下：

> Program received signal SIGSEGV, Segmentation fault. `_int_malloc` (av=av@entry=0x7fffff3ebc40 <main_arena>, bytes=bytes@entry=4096) at malloc.c:3789

先看下代码和现象：

```c
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

struct malloc_chunk {

  size_t      mchunk_prev_size;  /* Size of previous chunk (if free).  */
  size_t      mchunk_size;       /* Size in bytes, including overhead. */

  struct malloc_chunk* fd;         /* double links -- used only if free. */
  struct malloc_chunk* bk;

  /* Only used for large blocks: pointer to next larger size.  */
  struct malloc_chunk* fd_nextsize; /* double links -- used only if free. */
  struct malloc_chunk* bk_nextsize;
};

int main()
{
    void *tmp[2]; 
    struct malloc_chunk *test;
    int i;
    for (i = 0; i < 2; i++) {
        tmp[i] = malloc(1040);
        memset(tmp[i], 0, 1040);
    }    
    free(tmp[0]);
#ifdef SIG11_IN_MALLOC
    test = (struct malloc_chunk *)((char *)tmp[0] - 2*sizeof(size_t));
    test->bk = 0;
#endif
    tmp[0] = malloc(1040);
	return 0;
}
```

放开代码中`SIG11_IN_MALLOC`的宏定义后，直接编译执行就能得到如下结果：

```c
$ gcc -g main.c -o main
$ gdb main
GNU gdb (Ubuntu 8.1.1-0ubuntu1) 8.1.1
Copyright (C) 2018 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
<http://www.gnu.org/software/gdb/documentation/>.
For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from main...done.
(gdb) r
Starting program: /mnt/d/linux/git/EngineerLinux/Memory/CrashInLibc/Sig11InMalloc/code/main

Program received signal SIGSEGV, Segmentation fault.
_int_malloc (av=av@entry=0x7fffff3ebc40 <main_arena>, bytes=bytes@entry=2048) at malloc.c:3789
3789    malloc.c: 没有那个文件或目录.
(gdb) bt
#0  _int_malloc (av=av@entry=0x7fffff3ebc40 <main_arena>, bytes=bytes@entry=2048) at malloc.c:3789
#1  0x00007fffff0971cc in __GI___libc_malloc (bytes=2048) at malloc.c:3067
#2  0x00000000080007c5 in main () at main.c:35
```

从前面两小节已经了解到了libc内存管理中大大小小的概念，也知道了chunk是由链表组织起来的，但是究竟有几个链表？除了大小不同还有什么别的含义吗？

为了更好地回答这两个问题并将本小节所示源码中造成崩溃的点讲透彻，还需要全面地了解libc中的链表组织：

- Fast bin：

  这也是在上一小节的讲解过程中提及到的概念，此处将详细做个诠释。如果说libc中链表是分级管理的，那么fast bin就是那第一级的缓存链表，并且不是一条链表，而是一个链表集合，是为了满足程序频繁申请和释放小内存而设定的。设想一下，当程序通过`free`将多个连续的小chunk归还给libc后，libc自然会将其进行合并，但此时程序再次调用`malloc`申请小的chunk时，又需要从大的空闲内存中切出一块，这样无疑是低效的。故而对于不大于fast bin设定的最大缓存（2*`2*sizeof(size_t)`~8\*`2*sizeof(size_t)`，间隔`2*sizeof(size_t)`）的chunk释放后，将首先被加入fast bin，由于fast bin并不改变chunk的bit0，所以不会将其与相邻空闲chunk进行合并（后面会提到在fast bin分配不成功时才会强制合并fast bin）。后续对于不大于fast bin设定的最大缓存的chunk申请，也优先从fast bin中查找（且满足LIFO规则）。

- Small bin：

  这是`malloc`分配过程中，第二个会尝试的链表集合，它缓存的chunk大小范围比fast bin更大（2*`2*sizeof(size_t)`~64\*`2*sizeof(size_t)`，间隔`2*sizeof(size_t)`），与fast bin不同的是，在它内部是不会存在两个相邻的空闲chunk的。

- Unsorted bin：

  就像unsorted这个修饰词一样，它内部缓存着各种各样大小的chunk，并且它有且只有一条链表，之所以这么设计，主要还是为了做一层缓存。可以将它理解为是small bin和large bin的缓存，所有的chunk调用`free`回收时，先会根据大小看看能否放入fast bin，若不能则统统放入unsorted bin。在分配时，当fast bin和small bin都不能满足要求时，会将fast bin中的chunk全部合并加入unsorted bin，然后尝试使用unsorted bin来分配，所以从分配的角度，它是`malloc`第三个会尝试的链表。

- Large bin：

  从词面上看，它代表的是大块chunk的集合，它从Small bin的上限（64\*`2*sizeof(size_t)`）开始缓存，最大可缓存虚拟内存极限大小的chunk（2^32 Byte或2^64 Byte）。但是细想下， 如果说还是按照small bin和fast bin的方式，以`2*sizeof(size_t)`为间隔，那得消耗多少条链表？所以，在large bin的管理上也和其他bin不相同，它的不同区段以不同的间隔组织，并额外增加了两条链表（之前讲解chunk结构时未解释的两个成员，`fd_nextsize`和`bk_nextsize`），维护这种组织关系。下图将fast bin、small bin、unsorted bin和large bin的程序组织以及链表组织结构（以64位系统的chunk分布为例）：

![Image text](../../../img-storage/%E9%93%BE%E8%A1%A8%E7%BB%84%E7%BB%87.PNG)

到此，就已经将libc中对于内存管理的各种概念和结构基本解释清楚了，下面就看下真正的`malloc`流程（细节未完全展示，可能有些流程被忽略，但是作为对分配的概览是足够的）：

![Image text](../../../img-storage/malloc%E6%B5%81%E7%A8%8B.png)



那么，本节所示代码的问题出在哪里呢？

因为它修改了`free`后chunk的`bk`指针，造成libc尝试从unsorted bin中拿出这个节点时访问到了空指针而段错误崩溃，具体源码逻辑如下：

```c
victim = unsorted_chunks (av)->bk; //拿出unsorted bin中最后一个chunk
bck = victim->bk;                  //指向最后一个chunk的上一个，此时由于被程序破坏了，所以bck=0
...
unsorted_chunks (av)->bk = bck;   //指针操作，想要从unsorted bin中移除最后一个chunk节点
bck->fd = unsorted_chunks (av);   //第3789行，访问了空指针崩溃
```



## 多啰嗦的话

若各位是在ubuntu虚拟机中进行代码编译和调试，为了更好地调试libc的问题并分析libc的源码，不妨安装如下软件包：

```c
$ sudo apt-get install libc6-dbg
[sudo] callon 的密码：
正在读取软件包列表... 完成
正在分析软件包的依赖关系树
正在读取状态信息... 完成
libc6-dbg 已经是最新版 (2.27-3ubuntu1.4)。
升级了 0 个软件包，新安装了 0 个软件包，要卸载 0 个软件包，有 4 个软件包未被升级。
$ sudo apt-get source libc6
正在读取软件包列表... 完成
选择 glibc 作为源代码包而非 libc6
提示：glibc 的打包工作被维护于以下位置的 Git 版本控制系统中：
https://git.launchpad.net/~ubuntu-core-dev/ubuntu/+source/glibc
请使用：
git clone https://git.launchpad.net/~ubuntu-core-dev/ubuntu/+source/glibc
获得该软件包的最近更新(可能尚未正式发布)。
需要下载 17.0 MB 的源代码包。
获取:1 https://mirrors.tuna.tsinghua.edu.cn/ubuntu bionic-updates/main glibc 2.27-3ubuntu1.4 (dsc) [9,612 B]
获取:2 https://mirrors.tuna.tsinghua.edu.cn/ubuntu bionic-updates/main glibc 2.27-3ubuntu1.4 (tar) [15.9 MB]
获取:3 https://mirrors.tuna.tsinghua.edu.cn/ubuntu bionic-updates/main glibc 2.27-3ubuntu1.4 (diff) [1,091 kB]
已下载 17.0 MB，耗时 32秒 (530 kB/s)
dpkg-source: info: extracting glibc in glibc-2.27
dpkg-source: info: unpacking glibc_2.27.orig.tar.xz
dpkg-source: info: unpacking glibc_2.27-3ubuntu1.4.debian.tar.xz
dpkg-source: info: applying git-updates.diff
...
```

若ubuntu的软件源更新很慢，则可以按照如下方式更新：

百度搜索–>ubuntu镜像站–>选择清华的镜像源–>进去选择ubuntu—>选择ubuntu的版本–>复制里面的内容–>覆盖至原有的`/etc/apt/sources.list`的内容

全部成功后就能看到如下环境：

```c
drwxrwxrwx 1 callon callon       512 1月  21 22:38 glibc-2.27/
-rwxrwxrwx 1 callon callon   1091320 1月  21 22:38 glibc_2.27-3ubuntu1.4.debian.tar.xz*
-rwxrwxrwx 1 callon callon      9612 1月  21 22:38 glibc_2.27-3ubuntu1.4.dsc*
-rwxrwxrwx 1 callon callon  15923832 1月  21 22:38 glibc_2.27.orig.tar.xz*
```

其中的glibc-2.27就是源码文件夹，在有了这个环境后，以后无论进行调试还是进行源码分析都会有很大地帮助。

