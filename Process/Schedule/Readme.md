# 从优先级看调度

## Content

- [简介](#简介)
- [优先级的设置](#优先级的设置)
  - [优先级的取值](#优先级的取值)
  - [优先级的类别](#优先级的类别)

- [整体框架](#整体框架)
  - [调度的源头](#调度的源头)
  - [调度的策略](#调度的策略)
  - [调度的产物](#调度的产物)



---

## 简介

作为一个应用开发人员，涉及进程调度的内容真的不多，其中优先级可能是一个合适的入口。和前面的章节不同，并不是针对遇到的问题，而是针对知识点。因此，本节主要就是从优先级的设置方向了解下进程调度背后的实现方式，否则在设计程序时也无法为应用程序设置合适的优先级。

很多朋友可能在线程的优先级上是知道一些的，因为确实会考虑设置它们的优先级，但是进程却几乎不用设置，因为系统给了大家一个默认的调度器，不过在少数的场合下（对进程响应有更高的要求）也会对其进行设置，这也就是为什么TI公司在设计 [PCD（**P**rocess **C**ontrol **D**aemon）](https://sourceforge.net/p/pcd/code/HEAD/tree) 时将其作为了一个配置项，所以应用开发人员了解进程优先级时很有必要的。

本节参考PCD模块的配置，主要涉及两种调度：*FIFO* 和 *NICE* 。

## 优先级的设置

内核对于应用进程设置自己的运行优先级给了相应的系统调用接口，下面给出的设置优先级的例子是根据 [进程启动](../StartProcess) 的创建进程函数修改而来，这里只截取其中相关的一部分（完整程序在同级的 `code` 文件夹中可以看到）：

```c++
pid = fork();
if (pid < 0) {
    status = -1;
} else if (0 == pid) {
    ...
    /* Setup the priority of the process */
    {
        struct sched_param setParam;
        pid_t self_pid = getpid();
        if (sched_getparam(self_pid, &setParam) == 0) {
            if (NICE == priority->type) {
                setParam.sched_priority = 0;
                sched_setscheduler(self_pid, SCHED_OTHER, &setParam);
                setpriority(PRIO_PROCESS, 0, priority->value);
            } else {
                setParam.sched_priority = priority->value;
                sched_setscheduler(self_pid, SCHED_FIFO, &setParam);
            }
        }
    }
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
```

可以看到，针对 *NICE* 和 *FIFO* 两种类型，都是调用 `sched_setscheduler` 函数进行设置的，不过设置具体值时，*NICE* 额外调用了 `setpriority` 。

### 优先级的取值

在Linux系统中，*FIFO* 类型的优先级有时也称为实时进程优先级，而 *NICE* 则为非实时进程优先级，其取值范围如下：

|类型|作为参数的范围|内核转换后的范围|
|--|--|--|
|FIFO|0~99|0~99|
|NICE|-20~19|100~139|

参数和内核的转换范围可以跟踪 `setpriority` 系统调用来得到：

![sched_prio](../../img-storage/sched_prio.svg)

其中， `MAX_RT_PRIO` 就定义了最大的实时进程优先级，而真正优先级的范围为 `MAX_PRIO` ，是加上了 *NICE* （非实时进程）优先级所占跨度的结果，值为140。而 *NICE* 的范围也不难通过 `NICE_TO_PRIO` 宏计算出来。

### 优先级的类别

> 实际上，这里所说的 “优先级的类别” 其实就意味着调度器的类别。

通过跟踪 `sched_setscheduler` 系统调用函数就可以看到这个过程：

![sched_type](../../img-storage/sched_type.svg)

这里的 `sched_class` 就是调度器类别，它根据优先级的范围进行确定是哪种调度器，从面向对象的角度来看，这是定义了接口的设计模式：

![sched_class](../../img-storage/sched_class.svg)

> `rt_sched_class` 和 `fair_sched_class` 就是它的特化实现，前者又称为实时调度，后者又称为CFS（**C**ompletely **F**air **S**cheduler）。

进程的管理模块 `task_struct` 则需要包含/依赖它提供 “规则” 让调度器进行具体调度，如这里截取的四个比较重要的函数：

1. `enqueue_task` ：将进程添加到具体的运行队列；
2. `dequeue_task` ：将进程从运行队列中删除；
3. `pick_next_task` ：选择下一个进行调度的进程；
4. `task_fork` ：用于建立 `fork` 系统调用和调度器之间的关联，每次新进程建立后，就调用该函数通知调度器，但 `rt_sched_class` 并没有实现它，因为 *CFS* 需要做设置虚拟运行时间等准备工作，而实时调度不需要，这也能看出并不是调度类的每个接口具体的调度器都必须实现的。



## 整体框架 

在了解了优先级和与之相关的调度类后，这里将通过整体框架展示下它们在进程调度中的角色：

![sched_fw](../../img-storage/sched_fw.svg)

可以看到，整个调度过程由调度器发起，前面的优先级/调度类都为它提供了调度的策略，它根据这些策略产生的调度结果来唤醒此刻应该被调度的进程。

下面将从程序的维度，来边展示边分析如下几个过程：

1. 触发调度的源头
2. 调度策略的发挥
3. 调度结果的展现

### 调度的源头

下图展示了两种导致重新调度的情况，一种是**周期性地更新当前任务的状态时**，另一种是**睡眠的任务被唤醒时**。

![sched_time](../../img-storage/sched_time.svg)

整个过程可描述如下：

1. 这两种情况都将通过调度策略来进行判断，是否需要发生重新调度，满足条件的情况下会置上重新调度的标志位；

2. 当存在中断恢复/系统调用结束/其他进程主动放弃调度时，会调用 `schedule` 函数，其实现如下：

   ```c
   static __always_inline bool need_resched(void)
   {
   	return unlikely(tif_need_resched());
   }
   ...
   asmlinkage __visible void __sched schedule(void)
   {
   	struct task_struct *tsk = current;
   
   	sched_submit_work(tsk);
   	do {
   		preempt_disable();
   		__schedule(false);
   		sched_preempt_enable_no_resched();
   	} while (need_resched());
   	sched_update_worker(tsk);
   }
   EXPORT_SYMBOL(schedule);
   ```

   函数内部使用 `need_resched` 判断是否需要重新调度时（实际就是判断前面过程设置的 `TIF_NEED_RESCHED` 标志位），将会真正调用 `__schedule` 函数触发真正的调度过程

### 调度的策略



### 调度的产物

