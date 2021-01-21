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

![Image text](../../../img-storage/topchunk%E5%88%9D%E8%AF%86.png)

图中左侧是linux进程的虚拟内存分布图，这次将图中的堆区完全放大以反映在libc内存管理中的另一个概念——top chunk。这里同时假设堆区只分配了七个chunk，其中浅色填充的chunk就代表的是已经通过`malloc`已经分配给程序使用的内存块，深色填充的chunk代表已经通过`free`归还给libc的。

就和top chunk这个名词一样，它是一种特殊的chunk，位于堆区的顶部（实际上不仅只有堆区有top chunk，匿名映射区的管理也有，不过不妨碍这个概念的讲解，且个人认为先用堆区进行理解更佳）。前一小节提到，libc在每次libc内部的缓存不够时，都会通过调用`sysmalloc`函数（对于堆区底层是`brk`系统调用，对于匿名映射区底层是`mmap`系统调用）从操作系统”批发“一大块内存进行管理和分配，top chunk则是这管理的一环：

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

在程序完全未分配内存之前，`sbrk(0)`的地址为0x7fffc135b000，当第一次malloc调用后，`sbrk(0)`的地址调整为了0x7fffc137c000，这个调整就对应了前面提到的，通过调用`sysmalloc`函数从操作系统“批发”内存的过程。

对于堆区，`首次调整的大小size=ALIGN_UP(首次申请大小+2*sizeof(size_t)+默认128K,4K)`，通过程序可以看到，首次申请的大小为1024，那么前面的结果向上4K对齐就是132K，刚好是两次`sbrk(0)`地址的差值。再来看`tmp`和`ttmp`的地址，不难发现由于都是从top chunk中切割而来，所以地址是连续的，相差0x410=1024+`size(size_t)`，其中`size(size_t)`就是chunk head的大小，形象地将这个关系展示如下图：

![Image text](../../../img-storage/topchunk%E9%9A%8Fmalloc%E5%8F%98%E5%8C%96.png)

到这，再来翻译一下之前的报错：`sysmalloc: Assertion ``(old_top == initial_top (av) && old_size == 0) || ((unsigned long) (old_size) >= MINSIZE && prev_inuse (old_top) && ((unsigned long) old_end & (pagesize - 1)) == 0)' failed.`，实际上，这发生在libc向系统“批发”内存之前的内部校验：

对于不是第一次的分配，要求原有top chunk的大小至少要满足`MINSIZE`（64位系统上一般是32 Byte）并且它的`prev_inuse`（前一小节提到的flag中的bit0）应该被置位。而本小节提供的案例程序中，直接将top chunk的head赋值为了0，因而校验失败。



# 补充的知识

在libc的管理过程中，除了top chunk，还有其他的一些必须知晓的概念：

1. Arena：

   可以将其翻译为内存分配区，分为主分配区（main arena）和非主分配区（non main arena），也是前一小节提到的chunk head的flag中的bit2的含义所在。它是libc管理中最大的逻辑概念，主要针对多线程内存分配而言。程序中每次对同一分配区的申请/释放访问都需要加锁，只有一个分配区显然不足以满足性能的要求，因此设计为如下方式：

   1）当线程需要分配内存时，会先判断线程的私有变量中是否记录了自己的分配区，如果已有分配区了，就尝试对分配区进行加锁访问；

   2）若加锁成功，则使用该分配区分配内存；

   3）若加锁失败，则搜索分配区链表，尝试获取一个没有加锁的分配区；

   4）若尝试获取成功，则更新线程私有变量中记录的分配区，然后和加锁成功一样，按照 `2）` 的流程走；

   5）若链表中的所有分配区都尝试失败则开辟一个新的分配区（数量是有限制的，达到上限后就只能持续等待其他线程访问完毕解锁后，才能继续），将新的分配区也加入分配区链表，重新赋值线程私有变量中记录的分配区，并开启线程这一次分配请求；

   6）若线程原本就没有分配区，则和加锁失败一样，按照 `3）` 的流程走；

   7）当线程需要释放内存时，和分配内存不同的时，它只能获取线程私有变量中记录的分配区，若不成功则持续等待。

   此外，主分配区和非主分配区还有一点不同是，主分配区可以访问堆区（通过`brk`系统调用申请）和匿名映射区（只有`brk`失败时使用，通过`mmap`系统调用申请），而非主分配区只能访问匿名映射区（通过`mmap`系统调用申请）。

   之前提到的libc在每次libc内部的缓存不够时向操作系统“批发”内存，从大的分配层面上，可以理解为：当前分配区不足以满足分配条件时，需要向操作系统“批发”内存以填充分配区的可用内存。

2. Heap：

   这个概念并不是之前linux进程虚拟内存分布图中的堆区的含义，只能说类似，准确来说libc管理中的Heap表示的是非主分配区的“堆区”概念。这里就有朋友绕晕了，不是说只有主分配区能访问堆区吗？不急，先带着大家看下图：

   ![Image text](../../../img-storage/heap%E5%88%9D%E8%AF%86.png)

   图中展示了由两个Heap组成的非主分配区：
   
   1）与主分配区相同的是，它也是当内部缓存不足时，从操作系统“批发”一大块内存（HEAP_MAX_SIZE，32位系统上默认是1MB，64位系统上默认是64MB）作为top chunk，然后将top chunk切片给应用程序，所以这种称为“堆区”也没错，至少管理方式很类似；
   
   2）与主分配区不同的是，当非主分配区内部的缓存不足时，它并不能通过调用`brk`来获取到连续的内存充当新的top chunk，所以只能通过`mmap`系统调用获取一块新的Heap，将top chunk更新到新的Heap上，接下来也是通过切片top chunk给应用程序；
   
   3）图中还展示了libc中存储非主分配区的信息结构——`malloc_state`，它的`top`成员则记录了该非主分配区的top chunk所在地址；
   
   4）此外，图中还展示了libc中存储Heap的信息结构——`heap_info`，每个`mmap`的匿名映射都有一个`heap_info`作为头部信息，同一个非主分配区的不同Heap通过`heap_info`中的`prev`组成单向链表。
   
   那么，为什么表示主分配区的信息结构并没有展示在本节之前的堆区展开图中呢？
   
   因为它是个全局变量（代码变量为`main_arena`），存放在libc.so的数据段中。

