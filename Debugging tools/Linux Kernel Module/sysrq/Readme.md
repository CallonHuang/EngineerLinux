# sysrq

### Content

- [简介](#简介)

- [原理](#原理)
- [使用](#使用)

---

### 简介

在工程应用上，有着很多奇怪的问题从应用层并不好分析，如 CPU过高，OOM等，其实目前有着各式各样的工具用于查看这些问题，但是 *sysrq* 有时却是底层程序员更乐意使用的一个工具。

---

### 原理

其原理很简单，就是通过魔术键组合/`/proc/sysrq-trigger` 触发的方式，让内核陷入中断进行 *backtrace* /状态信息查询，最主要的是使用部分。

---

### 使用

需要注意，*sysrq* 默认的输出是在串口上，当某些远程 *ssh* 环境时，可以通过 *dmesg* 来获取到输出：

```shell
root@OpenWrt:~# echo h > /proc/sysrq-trigger
root@OpenWrt:~# dmesg
[70380.731614] sysrq: SysRq : HELP : loglevel(0-9) reboot(b) crash(c) terminate-all-tasks(e) memory-full-oom-kill(f) kill-all-tasks(i) thaw-filesystems(j) sak(k) show-backtrace-all-active-cpus(l) show-memory-usage(m) nice-all-RT-tasks(n) poweroff(o) show-registers(p) show-all-timers(q) unraw(r) sync(s) show-task-states(t) unmount(u) show-blocked-tasks(w)
```

如上为获取 *sysrq* 帮助的命令，这里主要介绍下常用的几个：

- [show-backtrace-all-active-cpus](#show-backtrace-all-active-cpus)
- [show-task-states](#show-task-states)
- [show-blocked-tasks](#show-blocked-tasks)
- [show-memory-usage](#show-memory-usage)
- [reboot](#reboot)

> 在实际使用时一定注意不要敲错，因为 *sysrq* 的优先级很高，中断时间长/触发了某些关键命令，极有可能破坏环境。

#### show-backtrace-all-active-cpus

```shell
root@OpenWrt:~# echo l > /proc/sysrq-trigger
root@OpenWrt:~# dmesg
[47807.752788] sysrq: SysRq : Show backtrace of all active CPUs
[47807.758527] NMI backtrace for cpu 1
[47807.762033] CPU: 1 PID: 14024 Comm: ash Tainted: P           O    4.9.37 #0
[47807.769034] Hardware name: Generic DT based system
[47807.773896] [<c010fc00>] (unwind_backtrace) from [<c010b778>] (show_stack+0x10/0x14)
[47807.781670] [<c010b778>] (show_stack) from [<c0335538>] (dump_stack+0x88/0x9c)
[47807.788924] [<c0335538>] (dump_stack) from [<c0338dbc>] (nmi_cpu_backtrace+0xc0/0xc4)
[47807.796781] [<c0338dbc>] (nmi_cpu_backtrace) from [<c0338ea0>] (nmi_trigger_cpumask_backtrace+0xe0/0x128)
[47807.806392] [<c0338ea0>] (nmi_trigger_cpumask_backtrace) from [<c0381498>] (__handle_sysrq+0xa4/0x170)
[47807.815757] [<c0381498>] (__handle_sysrq) from [<c03819f8>] (write_sysrq_trigger+0x54/0x64)
[47807.824146] [<c03819f8>] (write_sysrq_trigger) from [<c022b8c8>] (proc_reg_write+0x5c/0x84)
[47807.832522] [<c022b8c8>] (proc_reg_write) from [<c01d4e94>] (__vfs_write+0x1c/0x110)
[47807.840295] [<c01d4e94>] (__vfs_write) from [<c01d5c80>] (vfs_write+0xa4/0x168)
[47807.847641] [<c01d5c80>] (vfs_write) from [<c01d6a1c>] (SyS_write+0x3c/0x90)
[47807.854752] [<c01d6a1c>] (SyS_write) from [<c0107700>] (ret_fast_syscall+0x0/0x3c)
[47807.862508] Sending NMI from CPU 1 to CPUs 0:
[47807.867086] NMI backtrace for cpu 0
[47807.870574] CPU: 0 PID: 31 Comm: kswapd0 Tainted: P           O    4.9.37 #0
[47807.877612] Hardware name: Generic DT based system
[47807.882393] task: e79b8900 task.stack: e79e8000
[47807.886914] PC is at shrink_page_list+0x94/0xc18
[47807.891521] LR is at shrink_page_list+0x80/0xc18
[47807.896129] pc : [<c019e230>]    lr : [<c019e21c>]    psr: 600f0113
[47807.902385] sp : e79e9d38  ip : 00000000  fp : 00000000
[47807.907599] r10: e79e9d78  r9 : 00000001  r8 : c092af44
[47807.912814] r7 : e79e9e18  r6 : e79e9f18  r5 : e7f49d00  r4 : e7f49d14
[47807.919330] r3 : e81a7054  r2 : e79e9e18  r1 : 00000100  r0 : 00000200
[47807.925847] Flags: nZCv  IRQs on  FIQs on  Mode SVC_32  ISA ARM  Segment none
[47807.932971] Control: 10c5387d  Table: a5af406a  DAC: 00000051
[47807.938707] CPU: 0 PID: 31 Comm: kswapd0 Tainted: P           O    4.9.37 #0
[47807.945743] Hardware name: Generic DT based system
[47807.950525] [<c010fc00>] (unwind_backtrace) from [<c010b778>] (show_stack+0x10/0x14)
[47807.958256] [<c010b778>] (show_stack) from [<c0335538>] (dump_stack+0x88/0x9c)
[47807.965468] [<c0335538>] (dump_stack) from [<c0338d70>] (nmi_cpu_backtrace+0x74/0xc4)
[47807.973286] [<c0338d70>] (nmi_cpu_backtrace) from [<c010db54>] (handle_IPI+0xcc/0x1d8)
[47807.981192] [<c010db54>] (handle_IPI) from [<c010145c>] (gic_handle_irq+0x88/0x8c)
[47807.988750] [<c010145c>] (gic_handle_irq) from [<c010c34c>] (__irq_svc+0x6c/0x90)
[47807.996221] Exception stack(0xe79e9ce8 to 0xe79e9d30)
[47808.001263] 9ce0:                   00000200 00000100 e79e9e18 e81a7054 e7f49d14 e7f49d00
[47808.009429] 9d00: e79e9f18 e79e9e18 c092af44 00000001 e79e9d78 00000000 00000000 e79e9d38
[47808.017593] 9d20: c019e21c c019e230 600f0113 ffffffff
[47808.022635] [<c010c34c>] (__irq_svc) from [<c019e230>] (shrink_page_list+0x94/0xc18)
[47808.030366] [<c019e230>] (shrink_page_list) from [<c019f544>] (shrink_inactive_list+0x2e4/0x45c)
[47808.039140] [<c019f544>] (shrink_inactive_list) from [<c019fe1c>] (shrink_node+0x46c/0x89c)
[47808.047478] [<c019fe1c>] (shrink_node) from [<c01a0a50>] (kswapd+0x2a8/0x61c)
[47808.054602] [<c01a0a50>] (kswapd) from [<c0134844>] (kthread+0xec/0xf4)
[47808.061206] [<c0134844>] (kthread) from [<c01077b8>] (ret_from_fork+0x14/0x3c)
```

该命令可以查看当前每个CPU核上调用的栈，当出现某些卡死问题时，反复分析调用栈，可以摸索出规律，从而得到底层当前的问题所在。如上为 [内存引起的iowait问题](../../../Memory/StuckInIOwait) 中实际截出的调用栈，反复地敲下得到的结果显示，一直有一个核在做 `shrink_node` 回收内存的操作（另一个核在响应 *sysrq* 命令，即`__handle_sysrq`）。其实最终得到结论就是此时内存不足， *kswapd* 进程一直在回收 *cache* 导致的 *iowait* 过高问题。

#### show-task-states

```shell
root@OpenWrt:~# echo t > /proc/sysrq-trigger
root@OpenWrt:~# dmesg
...
[48460.092750] kworker/0:1H    S    0    74      2 0x00000000
[48460.099501] [<c05ef3e4>] (__schedule) from [<c05ef724>] (schedule+0x40/0xa0)
[48460.107004] [<c05ef724>] (schedule) from [<c012fc4c>] (worker_thread+0xfc/0x5a0)
[48460.115066] [<c012fc4c>] (worker_thread) from [<c0134844>] (kthread+0xec/0xf4)
[48460.123378] [<c0134844>] (kthread) from [<c01077b8>] (ret_from_fork+0x14/0x3c)
...
[48465.939928] videocapture    D    0  3242      1 0x00000000
[48465.945936] [<c05ef3e4>] (__schedule) from [<c05ef724>] (schedule+0x40/0xa0)
[48465.953347] [<c05ef724>] (schedule) from [<c05f22a8>] (schedule_timeout+0x134/0x174)
[48465.962405] [<c05f22a8>] (schedule_timeout) from [<c05ef21c>] (io_schedule_timeout+0x7c/0xb4)
[48465.972594] [<c05ef21c>] (io_schedule_timeout) from [<c05eff18>] (bit_wait_io+0x10/0x5c)
[48465.981720] [<c05eff18>] (bit_wait_io) from [<c05efb94>] (__wait_on_bit+0x60/0xb8)
[48465.990167] [<c05efb94>] (__wait_on_bit) from [<c018e468>] (wait_on_page_bit_killable+0x88/0x98)
[48466.000185] [<c018e468>] (wait_on_page_bit_killable) from [<c018eea0>] (__lock_page_or_retry+0xb0/0xec)
[48466.012022] [<c018eea0>] (__lock_page_or_retry) from [<c018f394>] (filemap_fault+0x4b8/0x5f4)
[48466.021882] [<c018f394>] (filemap_fault) from [<c0252570>] (ext4_filemap_fault+0x2c/0x40)
[48466.030624] [<c0252570>] (ext4_filemap_fault) from [<c01b14fc>] (__do_fault+0x68/0xe8)
[48466.039995] [<c01b14fc>] (__do_fault) from [<c01b5218>] (handle_mm_fault+0x600/0xa74)
[48466.049786] [<c01b5218>] (handle_mm_fault) from [<c0112088>] (do_page_fault+0x124/0x2bc)
[48466.059318] [<c0112088>] (do_page_fault) from [<c0101370>] (do_PrefetchAbort+0x38/0x9c)
[48466.067895] [<c0101370>] (do_PrefetchAbort) from [<c010c948>] (ret_from_exception+0x0/0x18)
[48466.076859] Exception stack(0xe5b2bfb0 to 0xe5b2bff8)
[48466.082899] bfa0:                                     7d5b4850 00000000 7d5b4870 a97e97a8
[48466.092732] bfc0: 7e534250 a97ff000 7d5b4850 b1b09f44 a1e00fb8 00000000 9c6f8cc8 9c6f8cf8
[48466.101827] bfe0: a97ff07c 9c6f8c38 a97eb454 b351dd30 600b0010 ffffffff
[48466.109362] combine         S    0  3257      1 0x00000000
[48466.115453] [<c05ef3e4>] (__schedule) from [<c05ef724>] (schedule+0x40/0xa0)
[48466.124315] [<c05ef724>] (schedule) from [<c017c3d4>] (futex_wait_queue_me+0x104/0x1b4)
[48466.132958] [<c017c3d4>] (futex_wait_queue_me) from [<c017cd44>] (futex_wait+0xe8/0x224)
[48466.142296] [<c017cd44>] (futex_wait) from [<c017eb24>] (do_futex+0x3b0/0xce0)
[48466.150131] [<c017eb24>] (do_futex) from [<c017f59c>] (SyS_futex+0x148/0x184)
[48466.158693] [<c017f59c>] (SyS_futex) from [<c0107700>] (ret_fast_syscall+0x0/0x3c)
[48466.168110] tracker         D    0  3258      1 0x00000000
[48466.175037] [<c05ef3e4>] (__schedule) from [<c05ef724>] (schedule+0x40/0xa0)
[48466.182485] [<c05ef724>] (schedule) from [<c05f22a8>] (schedule_timeout+0x134/0x174)
[48466.190823] [<c05f22a8>] (schedule_timeout) from [<c05ef21c>] (io_schedule_timeout+0x7c/0xb4)
[48466.200414] [<c05ef21c>] (io_schedule_timeout) from [<c05eff18>] (bit_wait_io+0x10/0x5c)
[48466.210142] [<c05eff18>] (bit_wait_io) from [<c05efb94>] (__wait_on_bit+0x60/0xb8)
[48466.218756] [<c05efb94>] (__wait_on_bit) from [<c018e468>] (wait_on_page_bit_killable+0x88/0x98)
[48466.228406] [<c018e468>] (wait_on_page_bit_killable) from [<c018eea0>] (__lock_page_or_retry+0xb0/0xec)
[48466.239347] [<c018eea0>] (__lock_page_or_retry) from [<c018f394>] (filemap_fault+0x4b8/0x5f4)
[48466.249794] [<c018f394>] (filemap_fault) from [<c0252570>] (ext4_filemap_fault+0x2c/0x40)
[48466.259453] [<c0252570>] (ext4_filemap_fault) from [<c01b14fc>] (__do_fault+0x68/0xe8)
[48466.267887] [<c01b14fc>] (__do_fault) from [<c01b5218>] (handle_mm_fault+0x600/0xa74)
[48466.276320] [<c01b5218>] (handle_mm_fault) from [<c0112088>] (do_page_fault+0x124/0x2bc)
[48466.286102] [<c0112088>] (do_page_fault) from [<c0101370>] (do_PrefetchAbort+0x38/0x9c)
[48466.295103] [<c0101370>] (do_PrefetchAbort) from [<c010c948>] (ret_from_exception+0x0/0x18)
[48466.304767] Exception stack(0xe59a1fb0 to 0xe59a1ff8)
[48466.310316] 1fa0:                                     e3fd2fa9 00000074 3cd75310 5bd1e995
[48466.319639] 1fc0: 8fc16d94 8fc16d10 8fc16d18 b580e000 ad40ab00 ad40ac9c e3fd2fa9 8fc16f1c
[48466.330254] 1fe0: fffffffc 8fc16cf8 00000008 b57ec6a4 600f0010 ffffffff
[48466.338161] liveness        S    0  3259      1 0x00000000
[48466.344256] [<c05ef3e4>] (__schedule) from [<c05ef724>] (schedule+0x40/0xa0)
[48466.351646] [<c05ef724>] (schedule) from [<c017c3d4>] (futex_wait_queue_me+0x104/0x1b4)
[48466.361001] [<c017c3d4>] (futex_wait_queue_me) from [<c017cd44>] (futex_wait+0xe8/0x224)
[48466.370550] [<c017cd44>] (futex_wait) from [<c017eb24>] (do_futex+0x3b0/0xce0)
[48466.379118] [<c017eb24>] (do_futex) from [<c017f59c>] (SyS_futex+0x148/0x184)
[48466.386805] [<c017f59c>] (SyS_futex) from [<c0107700>] (ret_fast_syscall+0x0/0x3c)
```

该命令可以说是非常强大了，它能够显示当前所有线程的内核调用栈（内核不区分进程和线程）。同样，通过调用栈可以协助分析当前进程/线程异常的原因。如上为 [内存引起的iowait问题](../../../Memory/StuckInIOwait) 中实际截出的线程栈，可以看到，所有为 `D` 状态的线程都是在缺页后等待io超时的状态下。

#### show-blocked-tasks

```shell
root@OpenWrt:~# echo w > /proc/sysrq-trigger
root@OpenWrt:~# dmesg
[49686.374707] sysrq: SysRq : Show Blocked State
[49686.379189]   task                PC stack   pid father
[49686.384454] logd            D    0   493      1 0x00000000
[49686.389994] [<c05ef3e4>] (__schedule) from [<c05ef724>] (schedule+0x40/0xa0)
[49686.397072] [<c05ef724>] (schedule) from [<c05f22a8>] (schedule_timeout+0x134/0x174)
[49686.404823] [<c05f22a8>] (schedule_timeout) from [<c05ef21c>] (io_schedule_timeout+0x7c/0xb4)
[49686.413391] [<c05ef21c>] (io_schedule_timeout) from [<c05eff18>] (bit_wait_io+0x10/0x5c)
[49686.421515] [<c05eff18>] (bit_wait_io) from [<c05efb94>] (__wait_on_bit+0x60/0xb8)
[49686.429108] [<c05efb94>] (__wait_on_bit) from [<c018e468>] (wait_on_page_bit_killable+0x88/0x98)
[49686.437934] [<c018e468>] (wait_on_page_bit_killable) from [<c018eea0>] (__lock_page_or_retry+0xb0/0xec)
[49686.447393] [<c018eea0>] (__lock_page_or_retry) from [<c018f394>] (filemap_fault+0x4b8/0x5f4)
[49686.455998] [<c018f394>] (filemap_fault) from [<c0252570>] (ext4_filemap_fault+0x2c/0x40)
[49686.464216] [<c0252570>] (ext4_filemap_fault) from [<c01b14fc>] (__do_fault+0x68/0xe8)
[49686.472167] [<c01b14fc>] (__do_fault) from [<c01b5218>] (handle_mm_fault+0x600/0xa74)
[49686.480043] [<c01b5218>] (handle_mm_fault) from [<c0112088>] (do_page_fault+0x124/0x2bc)
[49686.488196] [<c0112088>] (do_page_fault) from [<c0101370>] (do_PrefetchAbort+0x38/0x9c)
[49686.496258] [<c0101370>] (do_PrefetchAbort) from [<c010c948>] (ret_from_exception+0x0/0x18)
[49686.504608] Exception stack(0xe64a3fb0 to 0xe64a3ff8)
[49686.509682] 3fa0:                                     00000001 ffffffff b6f714d0 00000000
[49686.517921] 3fc0: bef3ecec bef3ecec ffffffff b6f2d010 000003e8 b6f2d040 b6f2d040 00000000
[49686.526162] 3fe0: b6f2cf18 bef3ecc8 b6f18280 b6e7fe7c 600b0010 ffffffff
...
[49687.277765] app  D    0  2469      1 0x00000000
[49687.283315] [<c05ef3e4>] (__schedule) from [<c05ef724>] (schedule+0x40/0xa0)
[49687.290525] [<c05ef724>] (schedule) from [<c05f22a8>] (schedule_timeout+0x134/0x174)
[49687.298357] [<c05f22a8>] (schedule_timeout) from [<c05ef21c>] (io_schedule_timeout+0x7c/0xb4)
[49687.306957] [<c05ef21c>] (io_schedule_timeout) from [<c05eff18>] (bit_wait_io+0x10/0x5c)
[49687.315219] [<c05eff18>] (bit_wait_io) from [<c05efb94>] (__wait_on_bit+0x60/0xb8)
[49687.322825] [<c05efb94>] (__wait_on_bit) from [<c018e468>] (wait_on_page_bit_killable+0x88/0x98)
[49687.331696] [<c018e468>] (wait_on_page_bit_killable) from [<c018eea0>] (__lock_page_or_retry+0xb0/0xec)
[49687.341164] [<c018eea0>] (__lock_page_or_retry) from [<c018f394>] (filemap_fault+0x4b8/0x5f4)
[49687.349764] [<c018f394>] (filemap_fault) from [<c0252570>] (ext4_filemap_fault+0x2c/0x40)
[49687.358019] [<c0252570>] (ext4_filemap_fault) from [<c01b14fc>] (__do_fault+0x68/0xe8)
[49687.366027] [<c01b14fc>] (__do_fault) from [<c01b5218>] (handle_mm_fault+0x600/0xa74)
[49687.373903] [<c01b5218>] (handle_mm_fault) from [<c0112088>] (do_page_fault+0x124/0x2bc)
[49687.382067] [<c0112088>] (do_page_fault) from [<c0101370>] (do_PrefetchAbort+0x38/0x9c)
[49687.390135] [<c0101370>] (do_PrefetchAbort) from [<c010c948>] (ret_from_exception+0x0/0x18)
[49687.398542] Exception stack(0xe55e3fb0 to 0xe55e3ff8)
[49687.403616] 3fa0:                                     bec80280 017045a8 00003468 b1eec6f0
[49687.411862] 3fc0: bec8033c b20df000 b1cf9420 01be4f9c bec8033c b20e4260 b1eec6f0 00000000
[49687.420112] 3fe0: bec80280 bec80268 b1cf35d4 b1cf35d8 60010010 ffffffff
[49687.426763] app  D    0  2495      1 0x00000000
[49687.432303] [<c05ef3e4>] (__schedule) from [<c05ef724>] (schedule+0x40/0xa0)
[49687.439417] [<c05ef724>] (schedule) from [<c05f22a8>] (schedule_timeout+0x134/0x174)
[49687.447242] [<c05f22a8>] (schedule_timeout) from [<c05ef21c>] (io_schedule_timeout+0x7c/0xb4)
[49687.455853] [<c05ef21c>] (io_schedule_timeout) from [<c05eff18>] (bit_wait_io+0x10/0x5c)
[49687.463978] [<c05eff18>] (bit_wait_io) from [<c05efb94>] (__wait_on_bit+0x60/0xb8)
[49687.471610] [<c05efb94>] (__wait_on_bit) from [<c018e468>] (wait_on_page_bit_killable+0x88/0x98)
[49687.480467] [<c018e468>] (wait_on_page_bit_killable) from [<c018eea0>] (__lock_page_or_retry+0xb0/0xec)
[49687.490033] [<c018eea0>] (__lock_page_or_retry) from [<c018f394>] (filemap_fault+0x4b8/0x5f4)
[49687.498654] [<c018f394>] (filemap_fault) from [<c0252570>] (ext4_filemap_fault+0x2c/0x40)
[49687.506891] [<c0252570>] (ext4_filemap_fault) from [<c01b14fc>] (__do_fault+0x68/0xe8)
[49687.514834] [<c01b14fc>] (__do_fault) from [<c01b5218>] (handle_mm_fault+0x600/0xa74)
[49687.522732] [<c01b5218>] (handle_mm_fault) from [<c0112088>] (do_page_fault+0x124/0x2bc)
[49687.530949] [<c0112088>] (do_page_fault) from [<c0101370>] (do_PrefetchAbort+0x38/0x9c)
[49687.539025] [<c0101370>] (do_PrefetchAbort) from [<c010c948>] (ret_from_exception+0x0/0x18)
[49687.547414] Exception stack(0xe5a49fb0 to 0xe5a49ff8)
[49687.552480] 9fa0:                                     afe06d50 afe06d50 afe06d50 afe06be0
[49687.560706] 9fc0: 016d4bf8 bec80578 bec80576 00000152 afe073e0 ffffffec 00000000 afe06bc4
[49687.568956] 9fe0: b6ec4638 afe06ba8 b68b57ac b68493ac 200d0010 ffffffff
[49687.575637] videocapture    D    0  3242      1 0x00000000
[49687.581179] [<c05ef3e4>] (__schedule) from [<c05ef724>] (schedule+0x40/0xa0)
[49687.588280] [<c05ef724>] (schedule) from [<c05f22a8>] (schedule_timeout+0x134/0x174)
[49687.596065] [<c05f22a8>] (schedule_timeout) from [<c05ef21c>] (io_schedule_timeout+0x7c/0xb4)
[49687.604627] [<c05ef21c>] (io_schedule_timeout) from [<c05eff18>] (bit_wait_io+0x10/0x5c)
[49687.612786] [<c05eff18>] (bit_wait_io) from [<c05efb94>] (__wait_on_bit+0x60/0xb8)
[49687.620434] [<c05efb94>] (__wait_on_bit) from [<c018e468>] (wait_on_page_bit_killable+0x88/0x98)
[49687.629272] [<c018e468>] (wait_on_page_bit_killable) from [<c018eea0>] (__lock_page_or_retry+0xb0/0xec)
[49687.638728] [<c018eea0>] (__lock_page_or_retry) from [<c018f394>] (filemap_fault+0x4b8/0x5f4)
[49687.647323] [<c018f394>] (filemap_fault) from [<c0252570>] (ext4_filemap_fault+0x2c/0x40)
[49687.655585] [<c0252570>] (ext4_filemap_fault) from [<c0112a74>] (__sync_icache_dcache+0x30/0x8c)
[49687.664406] [<c0112a74>] (__sync_icache_dcache) from [<00000004>] (0x4)
[49687.671058] tracker         D    0  3258      1 0x00000000
[49687.676613] [<c05ef3e4>] (__schedule) from [<c05ef724>] (schedule+0x40/0xa0)
[49687.683694] [<c05ef724>] (schedule) from [<c05f22a8>] (schedule_timeout+0x134/0x174)
[49687.691509] [<c05f22a8>] (schedule_timeout) from [<c05ef21c>] (io_schedule_timeout+0x7c/0xb4)
[49687.700110] [<c05ef21c>] (io_schedule_timeout) from [<c05eff18>] (bit_wait_io+0x10/0x5c)
[49687.708255] [<c05eff18>] (bit_wait_io) from [<c05efb94>] (__wait_on_bit+0x60/0xb8)
[49687.715879] [<c05efb94>] (__wait_on_bit) from [<c018e468>] (wait_on_page_bit_killable+0x88/0x98)
[49687.724702] [<c018e468>] (wait_on_page_bit_killable) from [<c018eea0>] (__lock_page_or_retry+0xb0/0xec)
[49687.734245] [<c018eea0>] (__lock_page_or_retry) from [<c018f394>] (filemap_fault+0x4b8/0x5f4)
[49687.742841] [<c018f394>] (filemap_fault) from [<c0252570>] (ext4_filemap_fault+0x2c/0x40)
[49687.751075] [<c0252570>] (ext4_filemap_fault) from [<c01b14fc>] (__do_fault+0x68/0xe8)
[49687.759056] [<c01b14fc>] (__do_fault) from [<c01b5218>] (handle_mm_fault+0x600/0xa74)
[49687.766980] [<c01b5218>] (handle_mm_fault) from [<c0112088>] (do_page_fault+0x124/0x2bc)
[49687.775135] [<c0112088>] (do_page_fault) from [<c0101370>] (do_PrefetchAbort+0x38/0x9c)
[49687.783220] [<c0101370>] (do_PrefetchAbort) from [<c010c948>] (ret_from_exception+0x0/0x18)
[49687.791609] Exception stack(0xe59a1fb0 to 0xe59a1ff8)
[49687.796695] 1fa0:                                     af518658 00000000 00000001 a1871ab8
[49687.804896] 1fc0: 8fc15aac 8c3fffe8 8fc164a0 00000000 8fc16044 8fc16024 00000004 00000000
[49687.813127] 1fe0: 8c3fffe8 8fc159c0 b5591e84 b5555bd8 200f0010 ffffffff
...
```

该命令为前一个命令的子集，这里只会展示被 *block* 的进程/线程的调用栈，如果只想分析异常的进程/线程，使用 `w` 就也不用自己筛选了。

#### show-memory-usage

```shell
root@OpenWrt:~# echo m > /proc/sysrq-trigger
root@OpenWrt:~# dmesg
[142208.618298] Mem-Info:
[142208.618317] active_anon:89059 inactive_anon:348 isolated_anon:0
[142208.618317]  active_file:24394 inactive_file:24373 isolated_file:0
[142208.618317]  unevictable:0 dirty:2240 writeback:0 unstable:0
[142208.618317]  slab_reclaimable:10824 slab_unreclaimable:2047
[142208.618317]  mapped:4297 shmem:1055 pagetables:624 bounce:0
[142208.618317]  free:10206 free_pcp:118 free_cma:3808
[142208.618335] Node 0 active_anon:356236kB inactive_anon:1392kB active_file:97576kB inactive_file:97492kB unevictable:0kB isolated(anon):0kB isolated(file):0kB mapped:17188kB dirty:8960kB writeback:0kB shmem:4220kB writeback_tmp:0kB unstable:0kB pages_scanned:0 all_unreclaimable? no
[142208.618354] Normal free:40824kB min:16384kB low:20480kB high:24576kB active_anon:356236kB inactive_anon:1392kB active_file:97576kB inactive_file:97492kB unevictable:0kB writepending:8952kB present:675840kB managed:661756kB mlocked:0kB slab_reclaimable:43296kB slab_unreclaimable:8188kB kernel_stack:1520kB pagetables:2496kB bounce:0kB free_pcp:472kB local_pcp:424kB free_cma:15232kB
lowmem_reserve[]: 0 0 0
[142208.618366] Normal: 678*4kB (UMEC) 428*8kB (UMEC) 20*16kB (UC) 438*32kB (UMC) 94*64kB (UMC) 4*128kB (C) 4*256kB (C) 3*512kB (C) 3*1024kB (C) 2*2048kB (C) 1*4096kB (C) = 40824kB
49823 total pagecache pages
[142208.618422] 0 pages in swap cache
[142208.618427] Swap cache stats: add 0, delete 0, find 0/0
[142208.618429] Free swap  = 0kB
[142208.618432] Total swap = 0kB
[142208.618435] 168960 pages RAM
[142208.618437] 0 pages HighMem/MovableOnly
[142208.618440] 3521 pages reserved
[142208.618443] 4096 pages cma reserved
```

该命令可以详细展示出当前的内存使用情况，如 *active_anon* （匿名页），*active_file* （磁盘数据页），*shmem* 等，而且可以看到是否是当前内存逐步碎片化导致的异常。

#### reboot

```shell
root@OpenWrt:~# echo b > /proc/sysrq-trigger
```

最好是魔术键组合以达到帅气的重启，不解释。
