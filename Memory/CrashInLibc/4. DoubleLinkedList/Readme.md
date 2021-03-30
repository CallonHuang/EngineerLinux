# Corrupted Double-linked List In Free

这里主要讨论涉及的第四类问题，具体描述如下：

> corrupted double-linked list, Program received signal SIGABRT, Aborted.

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
    void *tmp[4]; 
    struct malloc_chunk *test;
    int i;
    for (i = 0; i < 4; i++) {
        tmp[i] = malloc(1040);
        memset(tmp[i], 0, 1040);
    }
    free(tmp[1]);
#ifdef DOUBLE_LINKED_IN_FREE
    test = (struct malloc_chunk *)((char *)tmp[1] - 2*sizeof(size_t));
    test->bk = tmp[3]; //test->fd = tmp[3]; //also get double linked
#endif
    free(tmp[2]);
	return 0;
}
```

放开代码中`DOUBLE_LINKED_IN_FREE`的宏定义后，直接编译执行就能得到如下结果：

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
Starting program: /mnt/d/linux/git/EngineerLinux/Memory/CrashInLibc/4. DoubleLinkedList/code/main
corrupted double-linked list

Program received signal SIGABRT, Aborted.
__GI_raise (sig=sig@entry=6) at ../sysdeps/unix/sysv/linux/raise.c:51
51      ../sysdeps/unix/sysv/linux/raise.c: 没有那个文件或目录.
(gdb) bt
#0  __GI_raise (sig=sig@entry=6) at ../sysdeps/unix/sysv/linux/raise.c:51
#1  0x00007fffff040921 in __GI_abort () at abort.c:79
#2  0x00007fffff089967 in __libc_message (action=action@entry=do_abort, fmt=fmt@entry=0x7fffff1b6b0d "%s\n") at ../sysdeps/posix/libc_fatal.c:181
#3  0x00007fffff0909da in malloc_printerr (str=str@entry=0x7fffff1b4c3a "corrupted double-linked list") at malloc.c:5342
#4  0x00007fffff09825f in _int_free (have_lock=0, p=<optimized out>, av=0x7fffff3ebc40 <main_arena>) at malloc.c:4325
#5  __GI___libc_free (mem=<optimized out>) at malloc.c:3134
#6  0x00000000080007c7 in main () at main.c:34
```

在这之前已经将libc内存管理中的概念和`malloc`的大致流程都讲过了，下面将结合这个问题聊下`free`的大体流程：

![Image text](../../../img-storage/free%E6%B5%81%E7%A8%8B.png)

可以发现，当调用`free`回收一个chunk时，

1. libc将会根据chunk的bit0来判断前一个chunk是否空闲，若空闲则将从链表中摘除并更新本次的chunk大小；
2. 判断后一个chunk是否空闲需要区分后一个chunk是否为top chunk，是则将被合并进top chunk，否则按照对前一个chunk的处理一样，将后一个chunk也从链表中摘除并更新本次的chunk大小；
3. 当发现最终得到的chunk大小较大（大于64K，默认阈值），将判断chunk所在的堆区或者匿名映射区能否归还部分给操作系统。

而本节代码中的问题就恰好出在合并chunk时的`assert`判断上，判断源码如下：

```c
static void
unlink_chunk (mstate av, mchunkptr p)
{
  ...
  mchunkptr fd = p->fd;//指向p所在chunk的下一个chunk
  mchunkptr bk = p->bk;//指向p所在chunk的上一个chunk

  if (__builtin_expect (fd->bk != p || bk->fd != p, 0))//判断p所在chunk的链表存在指针的异常指向
    malloc_printerr ("corrupted double-linked list");
  ...
}
```

试想下，通常双向链表的节点应满足：前一个节点的后向指针将指向自己并且后一个节点的前驱指针将指向自己，所以libc就用了这种方式校验当前chunk的指针是否被破坏。



