# Thread Stack Leaked

线程栈泄露其实很少会导致OOM问题，因为一般来说线程栈的物理内存占用相对虚拟内存来说是少了很多的，所以该问题的主要致命点在于虚拟内存。

32位系统上虚拟内存是很有限的，虚拟内存不足会导致各种业务的卡死、崩溃或其他异常（和业务相关），是非常严重的一类问题。而64位系统，若是在内存资源有限的嵌入式设备上，随着线程栈的泄露，也会在一段时间后达到水位线以下而OOM，这个时间主要取决于泄露线程创建的频率。

下面给出了一段非常简单的创建线程函数（code文件夹中的main.c）：

```c
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>

void *start_routine(void *arg)
{
    printf("hello world!\n");
    return NULL;
}

int main()
{
    int i;
    pthread_t thread;
    pthread_create(&thread, NULL, start_routine, NULL);
    while(1);//do other thing
    return 0;
}
```

这就是一个典型的线程栈泄漏例子，当`start_routine`这个线程函数因为某种业务原因，要频繁地抛出执行，这种泄漏将是潜在且持续的。为了更好地说明这个问题发生的原因，这里将插入讲解下线程创建的原理知识（本节内容将基于glibc创建线程函数展开，这也是应用最多的创建线程函数）。

### 线程的状态

在glibc线程相关函数中，线程可以指定两种状态：`PTHREAD_CREATE_DETACHED`和`PTHREAD_CREATE_JOINABLE`。

- PTHREAD_CREATE_DETACHED

  ![Image text](../../../img-storage/Detach%E7%BA%BF%E7%A8%8B.png)

- PTHREAD_CREATE_JOINABLE

  ![Image text](../../../img-storage/Joinable%E7%BA%BF%E7%A8%8B.png)

从上面的UML图可以看出，`PTHREAD_CREATE_DETACHED`状态主要用于主进程无须等待所创建的线程结束而可以和线程分开各自完成自己的事情的情况。相反地，`PTHREAD_CREATE_JOINABLE`状态则是用于主进程接下来要做的事情依赖于所创建线程的结果，即让线程这些“下属们”并行去完成多件事情，自己作为“领导”只需等待（join）他们最终的结果进行“汇总”，最终再“汇报”给用户。

### 线程创建的过程

这部分需要结合源码来讲解，相关源码在code文件夹的glibc-2.32子文件夹中提供了，下图展示了相关函数的流程（图中的文件路径为在glibc源码中的相对路径）：

![Image text](../../../img-storage/%E7%BA%BF%E7%A8%8B%E5%88%9B%E5%BB%BA%E6%B5%81%E7%A8%8B.png)

从流程中可以看出：

- 就像函数运行必须要依附于函数堆栈一样，线程也不例外，创建线程的第一步就是申请线程栈；
- `pd`结构是依附于线程栈而产生并赋值的，主要是记录线程的一些信息，包括用户指定的线程函数指针就保存在了它的`start_routine`成员中；
- 真正线程的创建还是依赖于Linux操作系统的`clone`系统调用，线程可以正常访问进程中的任何虚拟内存块，主要也依靠着`CLONE_VM`这个`clone_flags`的指定，指定后线程的虚拟内存和进程的虚拟内存完全镜像；
- `clone`系统调用完成后，将直接通过所创建的堆栈运行`start_thread`函数，值得注意的是，这是glibc内部封装的函数而还没有到用户指定的线程函数；
- `start_thread`内部将调用之前保存在`pd`结构中的`start_routine`成员所指向的函数，当函数完成后，若是指定`PTHREAD_CREATE_DETACHED`的线程则会自动回收线程栈资源，最后调用`exit`退出（线程其实就是进程，因而退出也直接`exit`通知内核回收相关资源）。

下面不妨用一个小例子来模拟这个过程（code文件夹中的test_create_thread.c）：

```c
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int count = 0;
void *stack = NULL;
int stacksize = 8192;
int start_routine(void *arg)
{
    printf("hello world with count(%d)!\n", count++);
    free(stack);
    exit(1);
}

int main()
{
    int i;
    stack = malloc(stacksize);
    if (NULL == stack) {
        printf("thread stack create failed!\n");
        exit(0);
    }
    count++;
    clone(start_routine, (char *)stack + stacksize, CLONE_VM | CLONE_VFORK, 0);
    printf("after thread, count = %d!\n", count);
    while(1);//do other thing
    return 0;
}
```

值得注意的是，

1. 这里的`clone`实际调用的并不是系统调用，而是glibc封装的一段汇编代码，在 sysdeps/unix/sysv/linux/[平台名称]/clone.S 中可以看到，在它内部将调用真正的`clone`系统调用并在完成后直接在堆栈指针处存入传参并跳转运行指定函数，code/glibc-2.32文件夹中给出的是x86_64平台的clone.S源码，感兴趣的朋友可以一探究竟；
2. `CLONE_VFORK`只是本例这么使用，并非glibc创建线程指定的flag，在这里指定是为了让子线程先于进程执行，这样就可以更好地看出两者共用`count`这一全局变量的现象；
3. 指定的堆栈是申请出的堆内存的尾部，原因在于目前大多数平台上，栈的生长方向都是高地址到低地址增长。

### 线程资源的回收

在前面已经提到了`PTHREAD_CREATE_DETACHED`状态的线程将在线程函数结束后自动回收堆栈资源，但是却没有提及`PTHREAD_CREATE_JOINABLE`状态的线程。实际上，对于`PTHREAD_CREATE_JOINABLE`状态的线程，需要主进程/主线程在调用join时回收（具体函数在glibc中为`pthread_join`），即只有真正在“领导”想要结果并检查了“下属们”递交的结果后，才能放“下属们”回家，否则“领导”来不及取走的结果可能已经因为“下属们”提前“下班”而被“遗弃”。那么异步结束的线程如何通过`pthread_join`函数判断结束呢？

![Image text](../../../img-storage/Join%E7%BA%BF%E7%A8%8B%E8%B5%84%E6%BA%90%E5%9B%9E%E6%94%B6.png)

上图完整的展示了这一过程，本质上`pthread_join`对线程的判断还是依赖了Linux内核，当调用`clone`系统函数时，glibc指定了`CLONE_CHILD_CLEARTID`这个`clone_flags`，这意味着告诉内核，在`exit`调用后将`tid`的值清0（`exit->do_exit->exit_mm->exit_mm_release->mm_release`），从而方便glibc在`pthread_join`函数中判断线程是否正常退出。对于用户主动调用`pthread_cancel`导致的线程结束，因为经过了glibc，就可以通过`pd`结构设置相关标志位直接进行判断。这里的源码也都在code/glibc-2.32文件夹中给出了。

此时，再来解答本文中第一段代码涉及的线程栈泄漏问题就很简单了，既然已经知道了指定为`PTHREAD_CREATE_DETACHED`状态的线程是可以自动回收资源的，那么线程栈泄漏的原因只能有一个，那就是`pthread_create`在默认不指定`attr`时是`PTHREAD_CREATE_JOINABLE`状态，而源码中又没有调用`pthread_join`，所以导致了线程栈泄漏。而默认为`PTHREAD_CREATE_JOINABLE`状态这一点，在glibc源码的注释中可以很容易看出来，且确实在初始化时未对`flags`成员赋值为`ATTR_FLAG_DETACHSTATE`（值为1）。



## 补充知识点1——线程栈缓存

其实，如果对`ALLOCATE_STACK`略为深入研究的话，会发现glibc在线程栈的申请和释放其实还对应有缓存链表，不妨用一个例子来验证下（code文件夹中的test_stack_cache.c）：

```c
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>

void *start_routine(void *arg)
{
    pthread_attr_t attr;
    size_t stacksize;
    void *stackaddr = NULL;
    pthread_t thread = pthread_self();
    pthread_getattr_np(thread, &attr);
    pthread_attr_getstack(&attr, &stackaddr, &stacksize);
    pthread_attr_destroy(&attr);
    printf("thread[%ld] will finished: stackaddr[%p] stacksize[%ld]\n", thread, stackaddr, stacksize);
    return NULL;
}

int pthread_spawn(pthread_t *thread, unsigned int flag, void *(*start_routine) (void *), void *arg)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (flag & 0x1L)
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (flag > 1)
        pthread_attr_setstacksize(&attr, (flag & ~(0x1L)));
    return pthread_create(thread, &attr, start_routine, arg);
}

int main()
{
    int i;
    pthread_t thread[8];
    void *ret = NULL;
    const unsigned int stacksize[8] = {0, 128*1024, 512*1024, 2048*1024, 128*1024, 256*1024, 1024*1024, 0};
    for (i = 0; i < 4; i++) {
    	pthread_spawn(&thread[i], stacksize[i], start_routine, NULL);
    	printf("thread[%ld] created with stacksize[%u].\n", thread[i], (unsigned int)(stacksize[i] & ~(0x1L)));
    }
    for (i = 0; i < 4; i++)
    	pthread_join(thread[i], &ret);
    printf("------------------------------------------------------\n");
    for (; i < 7; i++) {
        pthread_spawn(&thread[i], stacksize[i], start_routine, NULL);
    	printf("thread[%ld] created with stacksize[%u].\n", thread[i], (unsigned int)(stacksize[i] & ~(0x1L)));
    }
    pthread_spawn(&thread[i], stacksize[i], start_routine, NULL);
    printf("thread[%ld] created with stacksize[%u].\n", thread[i], (unsigned int)(stacksize[i] & ~(0x1L)));
    for (i = 4; i < 8; i++)
        pthread_join(thread[i], &ret);
    return 0;
}
```

先执行看下结果：

```c
$ gcc test_stack_cache.c -o test -lpthread
$ ./test
thread[139944148141824] created with stacksize[0].
thread[139944148141824] will finished: stackaddr[0x7f4748bd1000] stacksize[8388608]
thread[139944158365440] created with stacksize[131072].
thread[139944158365440] will finished: stackaddr[0x7f4749d71000] stacksize[131072]
thread[139944158168832] created with stacksize[524288].
thread[139944158168832] will finished: stackaddr[0x7f4749ce1000] stacksize[524288]
thread[139944139687680] created with stacksize[2097152].
thread[139944139687680] will finished: stackaddr[0x7f47489c1000] stacksize[2097152]
------------------------------------------------------
thread[139944158365440] created with stacksize[131072].
thread[139944158365440] will finished: stackaddr[0x7f4749d71000] stacksize[131072]
thread[139944158168832] created with stacksize[262144].
thread[139944158168832] will finished: stackaddr[0x7f4749ce1000] stacksize[524288]
thread[139944139687680] created with stacksize[1048576].
thread[139944139687680] will finished: stackaddr[0x7f47489c1000] stacksize[2097152]
thread[139944148141824] created with stacksize[0].
thread[139944148141824] will finished: stackaddr[0x7f4748bd1000] stacksize[8388608]
```

从代码上可以看到首次申请的栈大小为8MB（设为0则默认8MB）、128KB、512KB和2MB，而在同一调用`pthread_join`回收后，

1. 再次申请的128KB地址和上次申请的128KB地址相同；
2. 再次申请的8MB地址和上次申请的8MB地址相同；
3. 再次申请的256KB被扩充为了512KB，且地址和上次申请的512KB也相同；
4. 再次申请的1MB被扩充为了2MB，且地址和上次申请的2MB也相同；

这四个现象无一不说明着线程栈缓存的存在，通过源码就更容易看出这一机制了（code/glibc-2.32/补充知识点1相关源码 文件夹的allocatestack.c），主要涉及`get_cached_stack`函数（申请）和`queue_stack`函数（释放）。当然这种缓存也不是无止境的，默认情况下当缓存超过`stack_cache_maxsize`（40MB）时将通过`munmap`归还给操作系统。



## 补充知识点2——pthread_attr_t泄漏

还是刚刚的例子（为了说明需要这里只截取了`start_routine`函数部分），

```c
void *start_routine(void *arg)
{
    pthread_attr_t attr;
    size_t stacksize;
    void *stackaddr = NULL;
    pthread_t thread = pthread_self();
    pthread_getattr_np(thread, &attr);
    pthread_attr_getstack(&attr, &stackaddr, &stacksize);
    //pthread_attr_destroy(&attr);
    printf("thread[%ld] will finished: stackaddr[%p] stacksize[%ld]\n", thread, stackaddr, stacksize);
    return NULL;
}
```

在`start_routine`函数中为了获取本线程的线程栈，调用了`pthread_getattr_np`函数来获取，在结束时又调用了`pthread_attr_destroy`进行销毁，如果此时将`pthread_attr_destroy`注释掉，那么就又会引发下一个线程相关函数的内存泄漏。

很多不了解的朋友可能会比较疑惑，明明`attr`只是个局部变量，为什么还要调用一个专门的destroy函数来销毁，线程结束栈回收后不就完了吗？

实际上，`pthread_getattr_np`函数获取到的信息不止`stackaddr`和`stacksize`这么简单，还有一个动态申请的内部指针`cpuset`需要释放，它是表示线程所绑定的CPU信息的（本质上通过`sched_getaffinity`系统调用实现），而`pthread_attr_destroy`函数恰恰就是针对这一动态申请的内存进行释放的。

code/glibc-2.32/补充知识点2相关源码 文件夹中列出了glibc中的代码，逻辑很简单，再次不作多余赘述。



