# Valgrind

### Content

- [前言](#前言)
- [使用方式](#使用方式)

---

### 前言

实际上，*Valgrind* 虽说是一个强大的内存检测工具，但是由于嵌入式平台资源的局限性，往往直接将它运行在主板上排查问题的很少，一般都是采用如下两种方式：

1. 所检测的代码是一个库，可以将其剥离出来，在 *x86*/*x64* 环境下编译成 *demo* 后进行测试，一般这种也称为软件入库前的单元测试；
2. 所检测的代码可以通过 *mock* 技术在 *x86*/*x64* 环境下进行模拟运行。

不管通过什么方式，都需要保证程序在 *x86*/*x64* 服务器上可以正常运行，此时再配合上 *Valgrind* 简直让人拍手称快！

---

### 使用方式

这里以一个对动态库的检测实例进行说明：

```shell
#!/bin/sh
SH_CUR_DIR=$(dirname `readlink -f $0`)
cd $SH_CUR_DIR/
g++ -L./ test.cpp -o test -lmemtrace -lc -lstdc++ -lpthread
if [ $? -ne 0 ];then
    echo "test compile failed!"
    cd -
    exit 1
fi
valgrind=`which valgrind`
ret=0
if [ ! -n "$valgrind" ];then
	echo "valgrind env is not correct!"
	cd -
	exit 1
else
	rm -f trace.log report.log
	$valgrind --tool=memcheck --leak-check=full --show-reachable=yes --log-file=./report.log ./test > trace.log &
	count=0
	while [ $count -le 15 ]
	do
		sleep 1
		if [ -f report.log ];then
			pid=`cat report.log | grep "Copyright" | awk -F '==' '{print $2}'`
			if grep 'normal_occupy=' trace.log;then
				#END OF TEST
				normal_occupy=`cat trace.log | grep "normal_occupy="`
				normal_occupy=${normal_occupy##*=}
				break
			fi
		fi
		let count++
	done
	if [ $count -ge 15 ];then
		echo "may jammed!"
		ret=1
	fi
	if [ -n "$pid" ];then
		echo "pid="$pid
		ps_ret=`ps | grep "$pid"`
		echo $ps_ret
		if [ -n "$ps_ret" ];then
			echo "need kill $pid"
			kill -9 $pid
		fi
	fi
	if [ $ret -eq 0 ];then
		eval $(sed 's/,//g' report.log | awk '
		BEGIN{definitely_lost=0;indirectly_lost=0;possibly_lost=0;still_reachable=0;tmp=0;invalid=0;}
		/Invalid write/{
			invalid=1
		}
		/Invalid read/{
			invalid=1
		}
		/definitely lost:/{
			definitely_lost=$4
		}
		/indirectly lost:/{
			indirectly_lost=$4
		}
		/possibly lost:/{
			possibly_lost=$4
		}
		/still reachable in loss/{
			tmp=$2
			still_reachable+=tmp
		}
		/allocatestack.c:/{
			still_reachable-=tmp
		}

		END{
		print "invalid="invalid
		print "definitely_lost="definitely_lost
		print "indirectly_lost="indirectly_lost
		print "possibly_lost="possibly_lost
		print "still_reachable="still_reachable
		}
		')	
		if [ ! -n "$normal_occupy" ];then
			normal_occupy=0
		fi
		echo "invalid=$invalid normal_occupy=$normal_occupy, still_reachable=$still_reachable, definitely_lost=$definitely_lost, indirectly_lost=$indirectly_lost, possibly_lost=$possibly_lost"
		if [ $definitely_lost -eq 0 ] && [ $indirectly_lost -eq 0 ] && [ $possibly_lost -eq 0 ];then
			if [ $still_reachable -eq $normal_occupy ] && [ $invalid -eq 0 ];then
				ret=0
			else
				ret=1
			fi
		else
			ret=1
		fi
	fi
fi
cd -
exit $ret
```

**脚本说明：**

1. 这里的测试对象是 *libmemtrace.so* ，一个用于跟踪内存使用的工具库；
2. 测试程序为 *test*，通过 *Valgrind* 执行 *test* 来得到内存是否泄漏的报告；
3. 除了 *Valgrind* 的报告需要分析外，还有一份由 *test* 主动在标准输出打印的名为 *trace.log* 的日志，这里面主要输出了 *libmemtrace.so* 在程序结束时正常占用的内存，由变量 *normal_occupy* 表示；
4. *Valgrind* 字段说明：
   - *Invalid write* / *Invalid read* ：内存越界
   - *definitely lost* ：内存泄漏，程序结束时，如果一块动态分配的内存没有被释放且通过程序内的指针变量均无法访问这块内存则会报这个错误
   - *indirectly lost* ：一般和 *definitely lost* 一起出现，修复前者即可
   - *possibly lost* ：内存泄漏，程序结束时，如果一块动态分配的内存没有被释放且通过程序内的指针变量均无法访问这块内存的起始地址，但可以访问其中的某一部分数据，则会报这个错误
   - *still reachable* ：可以访问，未丢失但也未释放

**测试程序的设计：**

1. 测试程序必须覆盖每个库中的接口，即测试覆盖率越高越好；
2. 当程序结束时，如果有什么状态或者字段可以代表某种含义的，可以输出到标准输出上通过脚本进行解析，如上述例子中的 *normal_occupy* ，否则会造成 *Valgrind* 的误判或者漏判；
3. 如果库中接口支持并发，测试程序可以考虑做一些并行测试或者循环测试，但不要过于测试和严苛，毕竟只是个单元测试例程。



