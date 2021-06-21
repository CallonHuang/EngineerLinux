# backtrace

### Content

- [简介](#简介)
- [使用方式](#使用方式)

---

### 简介

日常开发中，对于大多数的软件崩溃问题，只要有程序崩溃时的堆栈都能有效进行解决，因而这种有效、且空间消耗和调试时间消耗最低的方式，往往在嵌入式领域中使用更普遍。

实际上，*backtrace* 的方式并不止有一种，目前常用的有如下三种：

1. `__builtin_return_address`：一般用于获取上一层调用者的堆栈信息，大多数情况下并不支持多层堆栈的回溯，即只支持`__builtin_return_address(0)`；
2. `-mapcs/-mapcs-frame`：该选项则是告知编译器遵循 *APCS*（*ARM Procedure Call Standard*）规范，因为 *APCS* 规范了 *arm* 寄存器的使用、函数调用过程出栈和入栈的约定，但是缺点是在复杂的代码结构下，会造成编译器内部错误而导致的编译不过问题；
3. `-funwind-tables`：其原理为保存帧的解压缩信息放置在专用链接器部分，而帧展开后的信息允许程序在任何点进行上下文“窥视”

目前广泛使用，且 *GNU* 推荐的方式就是第三种。

---

### 使用方式

本文的例程在 *code* 文件夹中，已经将相关的步骤集合到了一个脚本中，且整个库也集成度很高，基本直接调用运行即可：

```shell
$ sh build.sh
$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./libbacktrace
$ ./main
============Exception Signal 11 Pid:95  Name:main=======================
========================Start Backtrace======================
#00 ./libbacktrace/libbacktrace.so(watch_backtrace+0x77) [0x7f654f3f12dc]
#01 ./libbacktrace/libbacktrace.so [0x7f654f3f13fb]
#02 /lib/x86_64-linux-gnu/libc.so.6 [0x7f654f02f040]
#03 ./main(main+0x1e) [0x7f654fa00848]
#04 /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xe7) [0x7f654f011bf7]
#05 ./main(_start+0x2a) [0x7f654fa0074a]
========================End Backtrace======================
段错误 (核心已转储)
```

值得注意的是，*backtrace* 原本的代码是参考了 *glibc* 中的源码，但 *glibc* 原生的 `backtrace_symbols` 由于调用了 `malloc`，在某些内存越界导致 *glibc* 内部紊乱的情况下会造成 *backtrace* 内部死锁，因而这里进行了优化，改写为了使用外部传参局部变量的方式，主要修改点如下所示：

```diff
static void 
backtrace_symbols (array, size, result, max_len)
     void *const *array;
     int size;
+    char **result;
+    int max_len;
{
	Dl_info info[size];
	int status[size];
	int cnt;
	size_t total = 0;
-   char **result;

	/* Fill in the information we can get from `dladdr'.  */
	for (cnt = 0; cnt < size; ++cnt) {
		status[cnt] = dladdr (array[cnt], &info[cnt]);
		if (status[cnt] && info[cnt].dli_fname &&
			info[cnt].dli_fname[0] != '\0')
		/*
		 * We have some info, compute the length of the string which will be
		 * "<file-name>(<sym-name>) [+offset].
		 */
		total += (strlen (info[cnt].dli_fname ?: "") +
				  (info[cnt].dli_sname ?
				  strlen (info[cnt].dli_sname) + 3 + WORD_WIDTH + 3 : 1)
				  + WORD_WIDTH + 5);
		else
			total += 5 + WORD_WIDTH;
	}

	/* Allocate memory for the result.  */
-	result = (char **) malloc (size * sizeof (char *) + total);
	if (result != NULL) {
		char *last = (char *) (result + size);
		for (cnt = 0; cnt < size; ++cnt) {
			result[cnt] = last;

			if (status[cnt] && info[cnt].dli_fname
				&& info[cnt].dli_fname[0] != '\0') {

				char buf[20];

				if (array[cnt] >= (void *) info[cnt].dli_saddr)
					sprintf (buf, "+%#lx",
							(unsigned long)(array[cnt] - info[cnt].dli_saddr));
				else
					sprintf (buf, "-%#lx",
					(unsigned long)(info[cnt].dli_saddr - array[cnt]));

				last += 1 + sprintf (last, "%s%s%s%s%s[%p]",
				info[cnt].dli_fname ?: "",
				info[cnt].dli_sname ? "(" : "",
				info[cnt].dli_sname ?: "",
				info[cnt].dli_sname ? buf : "",
				info[cnt].dli_sname ? ") " : " ",
				array[cnt]);
			} else
				last += 1 + sprintf (last, "[%p]", array[cnt]);
		}
-		assert (last <= (char *) result + size * sizeof (char *) + total);
+		assert (last <= (char *) result + max_len);
	}
-   return result;
+	return;
}
```



