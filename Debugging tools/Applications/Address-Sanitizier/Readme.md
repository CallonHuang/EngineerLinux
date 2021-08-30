# Address-Sanitizier

> 由于[官方文档](https://github.com/google/sanitizers/wiki/AddressSanitizer)真的解释地很详尽，因此本文绝大多数内容是对它的一个翻译。

### Content

- [简介](#简介)
- [原理概述](#原理概述)
  - [Instrumentation](#Instrumentation)
  - [Run-time library](#Run-time-library)
- [使用举例](#使用举例)
  - [Use after free](Use-after-free)
  - [Heap buffer overflow](Heap-buffer-overflow)
  - [Stack buffer overflow](#Stack-buffer-overflow)
  - [Global buffer overflow](#Global-buffer-overflow)
  - [Use after return](#Use-after-return)
  - [Use after scope](#Use-after-scope)
  - [Initialization order bugs](#Initialization-order-bugs)
  - [Memory leaks](#Memory-leaks)

---

## 简介

Address Sanitizer（ASAN）是一个快速的内存错误检测工具。它非常快，只拖慢程序两倍左右。它包括一个编译器插桩（instrumentation）模块和一个提供 `malloc`/`free` 替代项的动态运行时库（Run-time library）。

从gcc 4.8开始，AddressSanitizer成为gcc的一部分。当然，要获得更好的体验，最好使用4.9及以上版本，因为gcc 4.8的AddressSanitizer还不完善，最大的缺点是没有符号信息。

---

## 原理概述

### Instrumentation

整个虚拟地址空间在Address Sanitizer作用下被划分为了两个不相连的部分：

- 应用程序申请的内存（`Mem`）：这是提供给应用程序中使用的内存
- 影子内存（`Shadow`）：这部分内存包含影子数值（或元数据），它与应用程序申请的内存有着映射关系，将 `Mem` 的一个字节标记为 *poisoned*，其实就是在对应的 `Shadow` 中写入指定值

编译器中的instrumentation表现如下：

```c
shadow_address = MemToShadow(address);
if (ShadowIsPoisoned(shadow_address)) {
  ReportError(address, kAccessSize, kIsWrite);
}
```

`Mem` 中的8字节映射到 `Shadow` 中是1字节，这个字节可能有9种不同的值：

- 所有8字节都是 *unpoisoned* （可寻址）的，则值为0；
- 所有8字节都是 *poisoned* （不可寻址）的，则值为负；
- 前 `k` 字节为 *unpoisoned*，后面 `8-k` 字节为 *poisoned*， 则值为`k`。`malloc` 分配的内存总是8字节的倍数，如果要分配的缓存不是8字节的倍数，则尾部的8个字节 *poisoned* 状态不同。比如 `malloc(13)`，会得到两个8字节，前1个全是 *unpoisoned*，后一个只有前5个字节是*unpoisoned* 后3个字节是 *poisoned*。

instrumentation此时表现如下：

```c
byte *shadow_address = MemToShadow(address);
byte shadow_value = *shadow_address;
if (shadow_value) {
  if (SlowPathCheck(shadow_value, address, kAccessSize)) {
    ReportError(address, kAccessSize, kIsWrite);
  }
}
```

```c
// Check the cases where we access first k bytes of the qword
// and these k bytes are unpoisoned.
bool SlowPathCheck(shadow_value, address, kAccessSize) {
  last_accessed_byte = (address & 7) + kAccessSize - 1;
  return (last_accessed_byte >= shadow_value);
}
```

若是调用 `MemToShadow(ShadowAddr)` 将落入不可寻址的 `ShadowGap` 区域，所以，当程序直接访问 `Shadow` 将直接崩溃。在64位系统下，`Shadow` 和 `Mem` 的关系为

```c
Shadow = (Mem >> 3) + 0x7fff8000;
```

| 地址范围                           | 类型         |
| ---------------------------------- | ------------ |
| `[0x10007fff8000, 0x7fffffffffff]` | `HighMem`    |
| `[0x02008fff7000, 0x10007fff7fff]` | `HighShadow` |
| `[0x00008fff7000, 0x02008fff6fff]` | `ShadowGap`  |
| `[0x00007fff8000, 0x00008fff6fff]` | `LowShadow`  |
| `[0x000000000000, 0x00007fff7fff]` | `LowMem`     |

在32位系统下，`Shadow` 和 `Mem` 的关系为

```c
Shadow = (Mem >> 3) + 0x20000000;
```

| 地址范围                   | 类型         |
| -------------------------- | ------------ |
| `[0x40000000, 0xffffffff]` | `HighMem`    |
| `[0x28000000, 0x3fffffff]` | `HighShadow` |
| `[0x24000000, 0x27ffffff]` | `ShadowGap`  |
| `[0x20000000, 0x23ffffff]` | `LowShadow`  |
| `[0x00000000, 0x1fffffff]` | `LowMem`     |

为了检测栈溢出，Address Sanitizer指令将表现如下：

原始代码，

```c
void foo() {
  char a[8];
  ...
  return;
}
```

经instrumentation改编后，

```c
void foo() {
  char redzone1[32];  // 32-byte aligned
  char a[8];          // 32-byte aligned
  char redzone2[24];
  char redzone3[32];  // 32-byte aligned
  int  *shadow_base = MemToShadow(redzone1);
  shadow_base[0] = 0xffffffff;  // poison redzone1
  shadow_base[1] = 0xffffff00;  // poison redzone2, unpoison 'a'
  shadow_base[2] = 0xffffffff;  // poison redzone3
  ...
  shadow_base[0] = shadow_base[1] = shadow_base[2] = 0; // unpoison all
  return;
}
```

值得注意的是，当前紧凑的映射将无法捕获到如下所示的越界访问：

```c
int *x = new int[2]; // 8 bytes: [0,7].
int *u = (int*)((char*)x + 6);
*u = 1;  // Access to range [6-9]
```

如果需要解决该问题，可参考 https://github.com/google/sanitizers/issues/100，但是这会造成性能上的损耗。

### Run-time library

Address Sanitizer的运行时库替换了 `malloc`/`free` ，并提供了类似 `__asan_report_load8` 的错误报告函数。

- `malloc` 在缓存前后分配保护区，缓存本身标记为 *unpoisoned*，保护区标记为 *poisoned*
- `free` 将整个区域，包括缓存和保护区，都标记为 *poisoned*，并将该区域放入一个特别的队列中，以保证这个 `chunk` 在相当长的时间内不会再次使用它

在此机制下，内存访问的安全性就由检测访问地址是否 *poisoned* 来判定。内存访问的代码都被编译器替换如下：

替换前，

```c
*address = ...;  // or: ... = *address;
```

替换后，

```c
if (IsPoisoned(address)) {
  ReportError(address, kAccessSize, kIsWrite);
}
*address = ...;  // or: ... = *address;
```

访问之前检查访问地址是否 *poisoned*，如果是，报告错误。

---

## 使用举例

所有示例代码均在 *code* 文件夹中提供。

### Use after free

```c++
$ cat use-after-free.cpp
int main(int argc, char **argv) {
  int *array = new int[100];
  delete [] array;
  return array[argc];  // BOOM
}
$ g++ -O -g -fsanitize=address use-after-free.cpp -o use-after-free
$ ./use-after-free
=================================================================
==59==ERROR: AddressSanitizer: heap-use-after-free on address 0x614000000044 at pc 0x7f38484008e1 bp 0x7fffd42a5730 sp 0x7fffd42a5720
READ of size 4 at 0x614000000044 thread T0
    #0 0x7f38484008e0 in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-free.cpp:4
    #1 0x7f3846c61bf6 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x21bf6)
    #2 0x7f38484007a9 in _start (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-free+0x7a9)

0x614000000044 is located 4 bytes inside of 400-byte region [0x614000000040,0x6140000001d0)
freed by thread T0 here:
    #0 0x7f3847121480 in operator delete[](void*) (/usr/lib/x86_64-linux-gnu/libasan.so.4+0xe1480)
    #1 0x7f38484008ab in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-free.cpp:3

previously allocated by thread T0 here:
    #0 0x7f3847120608 in operator new[](unsigned long) (/usr/lib/x86_64-linux-gnu/libasan.so.4+0xe0608)
    #1 0x7f384840089b in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-free.cpp:2

SUMMARY: AddressSanitizer: heap-use-after-free /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-free.cpp:4 in main
Shadow bytes around the buggy address:
  0x0c287fff7fb0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c287fff7fc0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c287fff7fd0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c287fff7fe0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c287fff7ff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x0c287fff8000: fa fa fa fa fa fa fa fa[fd]fd fd fd fd fd fd fd
  0x0c287fff8010: fd fd fd fd fd fd fd fd fd fd fd fd fd fd fd fd
  0x0c287fff8020: fd fd fd fd fd fd fd fd fd fd fd fd fd fd fd fd
  0x0c287fff8030: fd fd fd fd fd fd fd fd fd fd fa fa fa fa fa fa
  0x0c287fff8040: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c287fff8050: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==59==ABORTING
```

### Heap buffer overflow

```c++
$ cat heap-buffer-overflow.cpp
int main(int argc, char **argv) {
  int *array = new int[100];
  array[0] = 0;
  int res = array[argc + 100];  // BOOM
  delete [] array;
  return res;
}
$ g++ -O -g -fsanitize=address heap-buffer-overflow.cpp -o heap-buffer-overflow
$ ./heap-buffer-overflow
=================================================================
==115==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x6140000001d4 at pc 0x7f29a5a00956 bp 0x7fffdb1d9080 sp 0x7fffdb1d9070
READ of size 4 at 0x6140000001d4 thread T0
    #0 0x7f29a5a00955 in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/heap-buffer-overflow.cpp:4
    #1 0x7f29a4261bf6 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x21bf6)
    #2 0x7f29a5a007f9 in _start (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/heap-buffer-overflow+0x7f9)

0x6140000001d4 is located 4 bytes to the right of 400-byte region [0x614000000040,0x6140000001d0)
allocated by thread T0 here:
    #0 0x7f29a4720608 in operator new[](unsigned long) (/usr/lib/x86_64-linux-gnu/libasan.so.4+0xe0608)
    #1 0x7f29a5a008e6 in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/heap-buffer-overflow.cpp:2

SUMMARY: AddressSanitizer: heap-buffer-overflow /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/heap-buffer-overflow.cpp:4 in main
Shadow bytes around the buggy address:
  0x0c287fff7fe0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c287fff7ff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c287fff8000: fa fa fa fa fa fa fa fa 00 00 00 00 00 00 00 00
  0x0c287fff8010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c287fff8020: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x0c287fff8030: 00 00 00 00 00 00 00 00 00 00[fa]fa fa fa fa fa
  0x0c287fff8040: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c287fff8050: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c287fff8060: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c287fff8070: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c287fff8080: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==115==ABORTING
```

### Stack buffer overflow

```c++
$ cat stack-buffer-overflow.cpp
int main(int argc, char **argv) {
  int stack_array[100];
  stack_array[1] = 0;
  return stack_array[argc + 100];  // BOOM
}
$ g++ -O -g -fsanitize=address stack-buffer-overflow.cpp -o stack-buffer-overflow
$ ./stack-buffer-overflow
=================================================================
==128==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7fffc2111ee4 at pc 0x7fe128600af9 bp 0x7fffc2111d20 sp 0x7fffc2111d10
READ of size 4 at 0x7fffc2111ee4 thread T0
    #0 0x7fe128600af8 in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/stack-buffer-overflow.cpp:4
    #1 0x7fe126c61bf6 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x21bf6)
    #2 0x7fe128600899 in _start (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/stack-buffer-overflow+0x899)

Address 0x7fffc2111ee4 is located in stack of thread T0 at offset 436 in frame
    #0 0x7fe128600989 in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/stack-buffer-overflow.cpp:1

  This frame has 1 object(s):
    [32, 432) 'stack_array' <== Memory access at offset 436 overflows this variable
HINT: this may be a false positive if your program uses some custom stack unwind mechanism or swapcontext
      (longjmp and C++ exceptions *are* supported)
SUMMARY: AddressSanitizer: stack-buffer-overflow /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/stack-buffer-overflow.cpp:4 in main
Shadow bytes around the buggy address:
  0x10007841a380: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x10007841a390: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x10007841a3a0: 00 00 00 00 00 00 f1 f1 f1 f1 00 00 00 00 00 00
  0x10007841a3b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x10007841a3c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x10007841a3d0: 00 00 00 00 00 00 00 00 00 00 00 00[f2]f2 00 00
  0x10007841a3e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x10007841a3f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x10007841a400: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x10007841a410: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x10007841a420: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==128==ABORTING
```

### Global buffer overflow

```c++
$ cat global-buffer-overflow.cpp
int global_array[100] = {-1};
int main(int argc, char **argv) {
  return global_array[argc + 100];  // BOOM
}
$ g++ -O -g -fsanitize=address global-buffer-overflow.cpp -o global-buffer-overflow
$ ./global-buffer-overflow
=================================================================
==143==ERROR: AddressSanitizer: global-buffer-overflow on address 0x7f52d1c011b4 at pc 0x7f52d1a009a4 bp 0x7fffec1c9700 sp 0x7fffec1c96f0
READ of size 4 at 0x7f52d1c011b4 thread T0
    #0 0x7f52d1a009a3 in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/global-buffer-overflow.cpp:3
    #1 0x7f52d0261bf6 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x21bf6)
    #2 0x7f52d1a00879 in _start (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/global-buffer-overflow+0x879)

0x7f52d1c011b4 is located 4 bytes to the right of global variable 'global_array' defined in 'global-buffer-overflow.cpp:1:5' (0x7f52d1c01020) of size 400
SUMMARY: AddressSanitizer: global-buffer-overflow /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/global-buffer-overflow.cpp:3 in main
Shadow bytes around the buggy address:
  0x0feada3781e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feada3781f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feada378200: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feada378210: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feada378220: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x0feada378230: 00 00 00 00 00 00[f9]f9 f9 f9 f9 f9 00 00 00 00
  0x0feada378240: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feada378250: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feada378260: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feada378270: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feada378280: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==143==ABORTING
```

### Use after return

这里需要注意，默认是不检查 *Use after return* 的，需要使用 `ASAN_OPTIONS=detect_stack_use_after_return=1` 来开启该功能。

```c++
$ cat use-after-return.cpp
int *ptr;
__attribute__((noinline))
void FunctionThatEscapesLocalObject() {
  int local[100];
  ptr = &local[0];
}

int main(int argc, char **argv) {
  FunctionThatEscapesLocalObject();
  return ptr[argc];
}
$ g++ -O -g -fsanitize=address use-after-return.cpp -o use-after-return
$ ./use-after-return
$ ASAN_OPTIONS=detect_stack_use_after_return=1 ./use-after-return
=================================================================
==166==ERROR: AddressSanitizer: stack-use-after-return on address 0x7f690aef9024 at pc 0x7f6910800c09 bp 0x7ffff55caa80 sp 0x7ffff55caa70
READ of size 4 at 0x7f690aef9024 thread T0
    #0 0x7f6910800c08 in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-return.cpp:10
    #1 0x7f690ee61bf6 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x21bf6)
    #2 0x7f6910800999 in _start (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-return+0x999)

Address 0x7f690aef9024 is located in stack of thread T0 at offset 36 in frame
    #0 0x7f6910800a89 in FunctionThatEscapesLocalObject() /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-return.cpp:3

  This frame has 1 object(s):
    [32, 432) 'local' <== Memory access at offset 36 is inside this variable
HINT: this may be a false positive if your program uses some custom stack unwind mechanism or swapcontext
      (longjmp and C++ exceptions *are* supported)
SUMMARY: AddressSanitizer: stack-use-after-return /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-return.cpp:10 in main
Shadow bytes around the buggy address:
  0x0feda15d71b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feda15d71c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feda15d71d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feda15d71e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feda15d71f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x0feda15d7200: f5 f5 f5 f5[f5]f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5
  0x0feda15d7210: f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5
  0x0feda15d7220: f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5 f5
  0x0feda15d7230: f5 f5 f5 f5 f5 f5 f5 f5 00 00 00 00 00 00 00 00
  0x0feda15d7240: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0feda15d7250: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==166==ABORTING
```

### Use after scope

```c++
$ cat use-after-scope.cpp
volatile int *p = 0;

int main() {
  {
    int x = 0;
    p = &x;
  }
  *p = 5;
  return 0;
}
$ g++ -O -g -fsanitize=address use-after-scope.cpp -o use-after-scope
$ ./use-after-scope
=================================================================
==180==ERROR: AddressSanitizer: stack-use-after-scope on address 0x7fffc68d7c80 at pc 0x7f4760000b93 bp 0x7fffc68d7c50 sp 0x7fffc68d7c40
WRITE of size 4 at 0x7fffc68d7c80 thread T0
    #0 0x7f4760000b92 in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-scope.cpp:8
    #1 0x7f475e861bf6 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x21bf6)
    #2 0x7f4760000999 in _start (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-scope+0x999)

Address 0x7fffc68d7c80 is located in stack of thread T0 at offset 32 in frame
    #0 0x7f4760000a89 in main /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-scope.cpp:3

  This frame has 1 object(s):
    [32, 36) 'x' <== Memory access at offset 32 is inside this variable
HINT: this may be a false positive if your program uses some custom stack unwind mechanism or swapcontext
      (longjmp and C++ exceptions *are* supported)
SUMMARY: AddressSanitizer: stack-use-after-scope /mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/use-after-scope.cpp:8 in main
Shadow bytes around the buggy address:
  0x100078d12f40: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x100078d12f50: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x100078d12f60: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x100078d12f70: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x100078d12f80: 00 00 00 00 00 00 00 00 00 00 00 00 f1 f1 f1 f1
=>0x100078d12f90:[f8]f2 f2 f2 00 00 00 00 00 00 00 00 00 00 00 00
  0x100078d12fa0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x100078d12fb0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x100078d12fc0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x100078d12fd0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x100078d12fe0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==180==ABORTING
```

### Initialization order bugs

该问题主要由于：构造不同的文件中全局变量的顺序是未定的，这种问题非常难以发现，改变代码的表现，通常将获得一个不期望得到的结果。

Address Sanitizer在编译好的可执行文件中插入了相关的检查，但是需要使用 `ASAN_OPTIONS=check_initialization_order=true` 来开启。

```c++
$ cat Initialization-order-bugs/a.cpp Initialization-order-bugs/b.cpp
#include <stdio.h>
extern int extern_global;
int __attribute__((noinline)) read_extern_global() {
  return extern_global;
}
int x = read_extern_global() + 1;
int main() {
  printf("%d\n", x);
  return 0;
}int foo() { return 42; }
int extern_global = foo();
$ g++ -O -g -fsanitize=address Initialization-order-bugs/a.cpp Initialization-order-bugs/b.cpp -o a.out
$ ./a.out
1
$ g++ -O -g -fsanitize=address Initialization-order-bugs/b.cpp Initialization-order-bugs/a.cpp -o a.out
$ ./a.out
43
$ ASAN_OPTIONS=check_initialization_order=true ./a.out
43
$ g++ -O -g -fsanitize=address Initialization-order-bugs/a.cpp Initialization-order-bugs/b.cpp -o a.out
$ ASAN_OPTIONS=check_initialization_order=true ./a.out
=================================================================
==236==ERROR: AddressSanitizer: initialization-order-fiasco on address 0x7f0d7b6021a0 at pc 0x7f0d7b400c40 bp 0x7ffff2346c00 sp 0x7ffff2346bf0
READ of size 4 at 0x7f0d7b6021a0 thread T0
    #0 0x7f0d7b400c3f in read_extern_global() Initialization-order-bugs/a.cpp:4
    #1 0x7f0d7b400ca9 in __static_initialization_and_destruction_0 Initialization-order-bugs/a.cpp:6
    #2 0x7f0d7b400ca9 in _GLOBAL__sub_I__Z18read_extern_globalv Initialization-order-bugs/a.cpp:10
    #3 0x7f0d7b400e0c in __libc_csu_init (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/a.out+0xe0c)
    #4 0x7f0d79c61b87 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x21b87)
    #5 0x7f0d7b400b29 in _start (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/a.out+0xb29)

0x7f0d7b6021a0 is located 0 bytes inside of global variable 'extern_global' defined in 'Initialization-order-bugs/b.cpp:2:5' (0x7f0d7b6021a0) of size 4
  registered at:
    #0 0x7f0d7a0764a8  (/usr/lib/x86_64-linux-gnu/libasan.so.4+0x364a8)
    #1 0x7f0d7b400db3 in _GLOBAL__sub_I_00099_1__Z3foov (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/a.out+0xdb3)

SUMMARY: AddressSanitizer: initialization-order-fiasco Initialization-order-bugs/a.cpp:4 in read_extern_global()
Shadow bytes around the buggy address:
  0x0fe22f6b83e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0fe22f6b83f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0fe22f6b8400: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0fe22f6b8410: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0fe22f6b8420: 00 00 00 00 00 00 00 00 04 f9 f9 f9 f9 f9 f9 f9
=>0x0fe22f6b8430: 00 00 00 00[f6]f6 f6 f6 f6 f6 f6 f6 00 00 00 00
  0x0fe22f6b8440: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0fe22f6b8450: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0fe22f6b8460: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0fe22f6b8470: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0fe22f6b8480: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==236==ABORTING
```

### Memory leaks

在这个例子中，发现 `-O` 选项将会优化此问题，因此尝试了多次，如下所示：

```c++
$ cat memory-leaks.cpp
#include <stdlib.h>

void *p;

int main() {
  p = malloc(7);
  p = 0; // The memory is leaked here.
  return 0;
}
$ g++ -O -g -fsanitize=address memory-leaks.cpp -o memory-leaks
$ ./memory-leaks
$ ASAN_OPTIONS=detect_leaks=1 ./memory-leaks
$ g++ -O -g -fsanitize=address -fsanitize=leak memory-leaks.cpp -o memory-leaks
$ ./memory-leaks
$ ASAN_OPTIONS=detect_leaks=1 ./memory-leaks
$ g++ -O -g -fsanitize=leak memory-leaks.cpp -o memory-leaks
$ ./memory-leaks
$ ASAN_OPTIONS=detect_leaks=1 ./memory-leaks
$ g++ -fsanitize=leak memory-leaks.cpp -o memory-leaks
$ ./memory-leaks
=================================================================
==276==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 7 byte(s) in 1 object(s) allocated from:
    #0 0x7f7e1b95eacb in __interceptor_malloc (/usr/lib/x86_64-linux-gnu/liblsan.so.0+0xeacb)
    #1 0x7f7e1cc00717 in main (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/memory-leaks+0x717)
    #2 0x7f7e1b571bf6 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x21bf6)

SUMMARY: LeakSanitizer: 7 byte(s) leaked in 1 allocation(s).
$ g++ -fsanitize=address -fsanitize=leak memory-leaks.cpp -o memory-leaks
$ ./memory-leaks
=================================================================
==289==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 7 byte(s) in 1 object(s) allocated from:
    #0 0x7fdacd51eb40 in __interceptor_malloc (/usr/lib/x86_64-linux-gnu/libasan.so.4+0xdeb40)
    #1 0x7fdace800957 in main (/mnt/d/linux/git/EngineerLinux/Debugging tools/Applications/Address-Sanitizier/code/memory-leaks+0x957)
    #2 0x7fdacd061bf6 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x21bf6)

SUMMARY: AddressSanitizer: 7 byte(s) leaked in 1 allocation(s).
```

与前面不同的是，`-fsanitize=leak` 选项才是检测内存泄漏的关键，且不是所有平台都支持该检测，如 *ARM* 平台可能会得到如下提示：

```shell
==1901==AddressSanitizer: detect_leaks is not supported on this platform.
```







