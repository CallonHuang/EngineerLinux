# ltrace

*ltrace* 是一款用于跟踪库函数调用的工具，在工程应用中一般用于在不重编可执行程序的情况下，对库函数调用相关问题进行排查/优化。与 *strace* 较为类似，但是使用频次上还是远不如 *strace* 的。

### 原理

*ltrace* 是一个（函数）库调用跟踪器（*library call tracer*），虽然也是基于 *ptrace* ，但是跟踪库函数和跟踪系统调用的差别还是很大的，这就是为什么 *ltrace* 是一个独立的工具而不是融于 *strace* 之中的原因。

### 编译

目前，还未能成功在某一平台上编译出可用的 *ltrace*（编译正常，但是使用时没有输出），只在 *ubuntu*（*x86* 机器上）直接安装后使用正常。

```shell
sudo apt-get install ltrace
```

只有若有成功交叉编译的案例，将会补充。

### 使用

*ltrace* 的使用命令和 *strace* 基本一模一样，常用的选项如下：

- *-tt* ：在每行输出的前面，显示毫秒级别的时间
- *-T* ：显示每个调用的函数执行所花费的时间
- *-p* ：指定要跟踪的tid/pid
- *-f* ：跟踪目标进程，以及目标进程创建的所有子进程

- *-c* ：统计每一函数调用的执行时间，次数和出错的次数等

```shell
$ ltrace -tt -T -f -p $PID
$ ltrace -c -f -p $PID
$ ltrace -c -f process
```

*ltrace* 跟踪库函数调用的特性，很大程度上能够帮助应用开发者优化自己的代码，如下是实际操作过的一个例子：**将存放于某一文件夹中的所有文件剪切至另一文件夹中**。

面对该问题，在实际应用中很多朋友喜欢直接使用 *system* 执行 *mv* ，但是了解过 *system* 底层的朋友会知道，它的潜在开销实际很大（创建子进程、进程切换等），有没有办法直接使用库函数直接完成 *mv* 的功能呢？当然可以。

于是乎开始了网络搜索拷贝和剪切相关的代码，实现**第一版**如下（仅表达流程）：

```c
int move_dir(char *srcDir, char *dstDir)
{
    ...
    srcDirp = opendir(srcDir);
    while ((srcdir = readdir(srcDirp)) != NULL) {
        ...
        if (snprintf(srcPath, sizeof(srcPath), "%s/%s", srcDir, srcdir->d_name) >= sizeof(srcPath)
            || snprintf(dstPath, sizeof(dstPath), "%s/%s", dstDir, srcdir->d_name) >= sizeof(srcPath)) {
            ret = -1;
            goto SAFE_EXIT;
        }
        ...
        src = fopen(srcPath,"r+");
        dst = fopen(dstPath,"w+");
        while ((len = fread(buf, 1, 1024, src)) > 0)
        	fwrite(buf, 1, len, dst);
        fclose(src);
        fclose(dst);
        unlink(srcPath);
    }
SAFE_EXIT:
    closedir(srcDirp);
    rmdir(srcDir);
    return ret;
}
```

但是，就像预期的那样，它的效率的确堪忧，更不用说没有考虑软链接文件、文件权限等问题了。此时，就用到了 *ltrace* 工具来完整跟踪 *mv* 命令，看看它到底做了哪些优化（主要看剪切/拷贝普通文件的优化）：

```shell
# ltrace mv -f /xxx/xxx /xxx/
...
rename(0x7fff3a4a6e7c, 0x13e4080, 0x7fdb28c63c30, 0x7fdb28996055) = -1
...
open64(0x7fff3a4a6e7c, 0, 438, 311)              = 3
open64(0x13e4080, 193, 0x81a4, 0x7fdb289964b0)   = 4
sendfile64(4, 3, 0, 0x1000000)                   = 24
sendfile64(4, 3, 0, 0x1000000)                   = 0
close(4, 3, 4, 0)                                = 0
close(3, 3, 4, 0x7fdb28997717)                   = 0
...
chown(0x13e4080, 0, 0, 0x7fdb2899e2a7)           = 0
chmod(0x13e4080, 0x81a4, 0, 0x7fdb28997717)      = 0
...
unlink(0x7fff3a4a6e7c, 0x7fff3a4a5460, 0x7fff3a4a5460, 0x7fdb289960f5) = 0
...
+++ exited (status 0) +++
```

其优化的核心点：

1. 使用 *rename* 函数
2. 由于 *rename* 在不同的 *mount* 点进行拷贝时会报 *EXDEV* 的错误，因而在 *rename* 失败时使用 *sendfile* 进行拷贝，最后删除文件

并且往下深入会了解到 *sendfile* 作为高级*I/O*函数，能够在两个描述符之间传递数据（完全在内核中操作），避免了内核与应用之间的拷贝，大大提高了原本 *fread* 和 *fwrite* 这种拷贝方式的效率，被称为零拷贝。

在了解原理后，便可集成进代码如下（完整代码可参见 *code* 文件夹中的 *move.c*）：

```c
int move_dir(char *srcDir, char *dstDir)
{
    ...
    srcDirp = opendir(srcDir);
    while ((srcdir = readdir(srcDirp)) != NULL) {
        ...
        if (snprintf(srcPath, sizeof(srcPath), "%s/%s", srcDir, srcdir->d_name) >= sizeof(srcPath)
            || snprintf(dstPath, sizeof(dstPath), "%s/%s", dstDir, srcdir->d_name) >= sizeof(srcPath)
            || lstat(srcPath, &scrStat) != 0) {
            ret = -1;
            goto SAFE_EXIT;
        }
        ...
        if (S_ISREG(scrStat.st_mode)) {
           	...
            if (0 != rename(srcPath, dstPath)) {
                int srcFd = -1, dstFd = -1;
                srcFd = open(srcPath, O_RDONLY);
                dstFd = open(dstPath, O_CREAT | O_WRONLY, 0777);
                while (sendfile(dstFd, srcFd, NULL, 0x8000) > 0);
                close(srcFd);
                close(dstFd);
                ...
                unlink(srcPath);
            }
        }
    }
SAFE_EXIT:
    closedir(srcDirp);
    rmdir(srcDir);
    return ret;
}
```

这样，就通过使用 *ltrace* 彻底优化了原本 *system* / *fread + fwrite* 的代码效率。

