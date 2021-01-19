# Sysmalloc Assertion

这里主要讨论涉及的第二类问题：**sysmalloc：**`malloc.c:2401: sysmalloc: Assertion ``(old_top == initial_top (av) && old_size == 0) || ((unsigned long) (old_size) >= MINSIZE && prev_inuse (old_top) && ((unsigned long) old_end & (pagesize - 1)) == 0)' failed.`，首先还是看下代码和现象：

```C
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
    int *ttmp, i;
    struct malloc_chunk *next;
#ifndef SYSMALLOC_ASSERT
    printf("sbrk(0) = %p\n", sbrk(0));
#endif
    char *tmp = (char *)malloc(1024);
#ifdef SYSMALLOC_ASSERT
    next = (struct malloc_chunk*)((char *)tmp + 1024);
    next->mchunk_size = 0;
#else
    printf("sbrk(0) = %p, tmp = %p\n", sbrk(0), tmp);
#endif
    ttmp = (int *)malloc(4);
#ifndef SYSMALLOC_ASSERT
    printf("sbrk(0) = %p, tmp = %p, ttmp = %p\n", sbrk(0), tmp, ttmp);
#endif
	return 0;
}
```

放开代码中`SYSMALLOC_ASSERT`的宏定义后，直接编译执行就能得到如下结果：

```c
$ gcc main.c  -o main
$ ./main
main: malloc.c:2401: sysmalloc: Assertion `(old_top == initial_top (av) && old_size == 0) || ((unsigned long) (old_size) >= MINSIZE && prev_inuse (old_top) && ((unsigned long) old_end & (pagesize - 1)) == 0)' failed.
已放弃 (核心已转储)
```

通过前一小节的学习，首先了解到了libc内部对于内存的组织方式，一句话总结就是：将大块内存切成chunk小块并通过一系列链表进行组织和管理。所以，从这次的代码就不难看到，崩溃的直接原因是由于修改了下一个空闲chunk的头部，但是从libc报出的错误却无从下手。为了更好地解释这个问题，不妨从更大的内存角度看下内存组织结构：

![Image text](https://github.com/CallonHuang/EngineerLinux/raw/master/img-storage/topchunk%E5%88%9D%E8%AF%86.png)

图中左侧依然是linux进程的虚拟内存分布图，这次将图中的堆区完全放大以反应在libc内存管理中的另一个概念top chunk。这里同时假设堆区只分配了七个chunk，其中浅色填充的chunk就代表的是已经通过`malloc`已经分配给程序使用的内存块，深色填充的chunk代表已经通过`free`归还给libc的。

就和top chunk这个名词一样，它是一种特殊的chunk，位于堆区的顶部（实际上不仅只有堆区有top chunk，匿名映射区的管理也有，不过不妨碍这个概念的讲解，且个人认为先用堆区进行理解更佳）。前一小节提到，libc在每次libc内部的缓存不够时，都会通过调用`sysmalloc`函数从操作系统”批发“一大块内存进行管理和分配，top chunk则是这管理的一环：

- 在获取到“批发”而来的内存时，它会将”批发“而来的一大块内存全部先作为top chunk，然后分配时则是意味着从top chunk中切出对应大小的块分配给应用程序
- 对于堆区，若程序调用`free`归还的chunk与top chunk相邻，则会被合并进top chunk（实际上当chunk被放入fastbin中是不会被合并的，但是只限于单一的这种可能，在fastbin未讲解前不妨这么理解），当top chunk过大时，还会将其切去一部分大小，切出来的部分则归还给操作系统

接下来，再通过程序来找下top chunk的影子。若本小节的程序实例未放开`SYSMALLOC_ASSERT`的宏定义，那么你可能得到如下结果：

```c
$ gcc main.c -o main
$ ./main
sbrk(0) = 0x7fffc135b000
sbrk(0) = 0x7fffc137c000, tmp = 0x7fffc135b470
sbrk(0) = 0x7fffc137c000, tmp = 0x7fffc135b470, ttmp = 0x7fffc135b880
```

在程序完全未分配内存之前，`sbrk(0)`的地址为0x7fffc135b000，当第一次malloc调用后，`sbrk(0)`的地址调整为了0x7fffc137c000，这个调整就对应了前面提到的，通过调用`sysmalloc`函数问操作系统“批发”内存的过程。

对于堆区，`首次调整的大小size=ALIGN_UP(首次申请大小+2*sizeof(size_t)+默认128K,4K)`，通过程序可以看到，首次申请的大小为1024，那么前面的结果向上4K对其就是132K，刚好是两次`sbrk(0)`地址的差值。再来看`tmp`和`ttmp`的地址，不难发现由于都是从top chunk中切割而来，所以地址是连续的，所以相差0x410=1024+size(size_t)，即chunk head的大小。

到这，再来翻译一下之前的报错`sysmalloc: Assertion ``(old_top == initial_top (av) && old_size == 0) || ((unsigned long) (old_size) >= MINSIZE && prev_inuse (old_top) && ((unsigned long) old_end & (pagesize - 1)) == 0)' failed.`，它是在向系统“批发”内存之前的内部校验：

对于不是第一次的分配，要求原有top chunk的大小至少要满足`MINSIZE`（64位系统上一般是32 Byte）并且它的`prev_inuse`（前一小节提到的flag中的bit0）应该被置位。而本小节提供的案例程序中，直接将top chunk的head赋值为了0，因而校验失败。



