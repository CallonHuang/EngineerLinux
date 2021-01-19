# Abort In Free

这里主要谈论涉及的第一大类问题：**Abort In Free:**  `free(): invalid pointer / free(): invalid size / munmap_chunk(): invalid pointer / double free or corruption`，首先不妨看下代码和现象：

```C
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main()
{
    void *temp = NULL;
    temp = malloc(128);
#if defined(INVALID_POINTER)/*free(): invalid pointer*/
    *((size_t*)(temp-sizeof(size_t))) = 0;
#elif defined(INVALID_SIZE)/*free(): invalid size*/
    *((size_t*)(temp-sizeof(size_t))) = 0xffff<<3;
#elif defined(MUNMAP_INVALID_POINTER)/*munmap_chunk(): invalid pointer*/
    *((size_t*)(temp-sizeof(size_t))) = 0x2;
#elif defined(DOUBLE_FREE)/*double free or corruption*/
    *((size_t*)(temp-sizeof(size_t))) = 144<<3|0x0;
#endif
    free(temp);
	return 0;
}
```

代码中的每一个宏说明了下方的代码行可以造成注释中所述的libc崩溃问题，即每一行都是一种类型的崩溃，通过将不同的宏定义依次放开编译得到的结果将如下：

```C
$ gcc main.c -o main
$ ./main
free(): invalid pointer
已放弃 (核心已转储)
$ gcc main.c -o main
$ ./main
free(): invalid size
已放弃 (核心已转储)
$ gcc main.c -o main
$ ./main
munmap_chunk(): invalid pointer
已放弃 (核心已转储)
$ gcc main.c -o main
$ ./main
double free or corruption (!prev)
已放弃 (核心已转储)
```

那么为了更好地解释这个问题，不妨先看下`malloc`内存块的最底层组织结构：

![Image text](https://github.com/CallonHuang/EngineerLinux/raw/master/img-storage/malloc%E5%86%85%E5%AD%98%E7%BB%84%E7%BB%87.png)

图中左侧画出了linux进程的虚拟内存分布图，通过将图中的一小段放大，这样更能反映出接下来要讨论的内存粒度，右侧放大的图即为本次要讨论的重点。

右侧图中可以看到chunk这一名词，它可以简单地理解为分配给程序使用的内存块，每当程序调用`malloc`将从libc管理的内存中拿走合适的一个chunk，而程序调用`free`时将归还给libc这一块内存，其中浅色填充的chunk就代表的是已经通过`malloc`已经分配给程序使用的内存块，深色填充的chunk代表已经通过`free`归还给libc的。不难看到，每个空闲chunk的组织是通过将同样大小的chunk串联成链表来实现（不但如此，这还是一种精妙的内存复用，值得借鉴）。此外，每个chunk分为head和body，即使通过`malloc`从libc拿到内存，由于返回给程序的指针也只是body的起始位置，因而应用程序中只会使用body部分，而chunk的head部分是专门为libc内部管理服务的。图中也通过文字展示了，每一个chunk的head记录了这个chunk的size和一些flag。

接下来不妨看下libc中chunk真正对应的代码表示：

```c
struct malloc_chunk {
  size_t      mchunk_prev_size;  /* Size of previous chunk (if free).  */
  size_t      mchunk_size;       /* Size in bytes, including overhead. */

  struct malloc_chunk* fd;         /* double links -- used only if free. */
  struct malloc_chunk* bk;

  /* Only used for large blocks: pointer to next larger size.  */
  struct malloc_chunk* fd_nextsize; /* double links -- used only if free. */
  struct malloc_chunk* bk_nextsize;
};
```

结合该代码块中的注释`size_t mchunk_prev_size;  /* Size of previous chunk (if free).  */`（即只有上一个chunk是空闲时，`mchunk_prev_size`字段才是有意义的），再来看图中空闲chunk其body的最后一小部分，如`A Chunk body(B chunk_prev_size=A Chunk Head)`。不难看到在代码结构上，`mchunk_prev_size`是属于B的，但是当A要被分配出去给程序使用时，由于A不再空闲，那么B的`mchunk_prev_size`也不再生效，从而作为A的body一同给到程序使用。所以图中所示的chunk是从`mchunk_size`这个成员开始的，它才是前面提到的chunk的head（body就是从`fd`这个成员开始的部分）。

需要注意的是，chunk的head从代码上可以看到，总共只有`sizeof(size_t)`的大小，但它却同时记录了flag和size，这是因为每个chunk大小都要求是8字节对齐的，因此它最低的三个bit可以用来存储三个flag：

|flag包含的bits|含义|
|--|--|
|bit2（对本主题不是很关键）|1-主分配区（main_arena）<br>0-非主分配区|
|bit1|1-使用`mmap`从进程映射区分配<br>0-不是`mmap`分配，若chunk空闲，则该状态不存在|
|bit0|1-前一个chunk正在使用<br>0-前一个chunk为空闲|

到这，前面程序的崩溃问题就可以得到解答了：

之所以一行赋值语句就能造成libc的崩溃，其原因在于修改的是libc管理chunk的头部，而在调用`free`即chunk回收时，libc将校验其头部flags和size，若与预期不符则说明程序存在异常，因而直接`Assert`提示到开发人员。



## 补充知识点

本文反复地提到libc管理，那么应用程序上为何需要libc管理？

主要原因在于linux内核对应用程序的内存申请都是以页为单位响应的（一般系统上1页就是4k Byte的大小），但是往往应用程序的申请需要的只是几个Byte或者几百Byte，这样就造成了两边需求的不匹配。

libc担任起了“协调”两者的角色：

- 为了满足内核的要求：它将在每次libc内部的缓存不够时，去问系统“批发”一大块内存（4k Byte的整数倍）填充自己的内存池
- 为了满足应用程序的要求：它将“批发”来的内存切成一个个的chunk给到应用程序，应用程序归还时则内部对chunk进行合并操作

当内存回收充足且满足一定条件时，再通过系统调用归还给操作系统一大块内存（4k Byte的整数倍）。

这样做的好处主要有三点：

1. 内核可以继续以页为单位进行管理，这是最底层的管理，它不需要关心应用程序是如何使用的
2. 内存池作为一个偏应用的功能还可以随意更换（libc本身就支持替换），更具有了一定的灵活性
3. 大大降低了系统调用的次数，减少了程序耗时，同时也减少了内存碎片的产生

那么，linux内核内部的内存使用也都是以页为单位（4k Byte）的方式使用的吗？

当然不是，linux内核只针对应用程序以页为单位进行管理，而针对它自己使用零碎的内存，它使用了slab进行管理。

