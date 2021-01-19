# 令人费解的Libc崩溃

Linux的工程应用上，随着业务逻辑的复杂程度不断提高，会遇到各种各样奇怪的软件bug，其中之一可能就是glibc中提示的崩溃，若是对`libc_malloc`原理缺乏初步的了解，这将是令人困惑和费解的一类bug。

如下列出了常见的几种问题：

1. Abort In Free类问题:  `free(): invalid pointer / free(): invalid size / munmap_chunk(): invalid pointer / double free or corruption`
2. `malloc.c:2401: sysmalloc: Assertion ``(old_top == initial_top (av) && old_size == 0) || ((unsigned long) (old_size) >= MINSIZE && prev_inuse (old_top) && ((unsigned long) old_end & (pagesize - 1)) == 0)' failed.`
3. `SIGSEGV in malloc`
4. `corrupted double-linked list`
5. `double free`可能造成的越界假象

看完这些问题是否就有种上头的感觉？可能造成这些问题的例子都将在github相应文件夹的code子文件夹中提供，那么，接下来就逐一进行例子的分析并揭开libc内存管理的面纱。每个类型问题将对应一个文件夹进行阐述：

- **AbortInFree**文件夹对应问题1
- **SysmallocAssert**文件夹对应问题2
- **Sig11InMalloc**文件夹对应问题3
- **DoubleLinkedList**对应问题4
- **Others**文件夹对应问题5

推荐从问题1逐一进行阅读，这样知识获取将更加连贯。