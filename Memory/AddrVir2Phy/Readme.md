# 虚拟地址转换物理地址

如果对Linux系统调用有所了解的朋友一般都能知道，想要将物理地址映射到进程内作为虚拟地址使用，可以通过 `mmap` 来完成，但是工程上是否有某些场景涉及需要通过虚拟地址找到物理地址呢？答案是有的，比如：

进程A `malloc` 了一块大内存，且想要和B进程进行共享，比较高效的方式是将物理地址发送给B，让B去映射即可，但是进程A要怎样拿到一个虚拟地址对应的物理地址就成为了整个机制的关键所在。这里先给出答案：`/proc/self/pagemap` 。`pagemap` 文件是每个进程私有的虚拟页到物理页的映射情况，每个 *page* 页对应的记录项由8个字节（64-bit）组成，格式如下：

```shell
    * Bits 0-54  page frame number (PFN) if present
    * Bits 0-4   swap type if swapped
    * Bits 5-54  swap offset if swapped
    * Bit  55    pte is soft-dirty (see Documentation/vm/soft-dirty.txt)
    * Bit  56    page exclusively mapped (since 4.2)
    * Bits 57-60 zero
    * Bit  61    page is file-page or shared-anon (since 3.5)
    * Bit  62    page swapped
    * Bit  63    page present
```

比较简单地可以关注两部分，Bit 63 和 Bits 0-54，前者说明当前页是否存在，后者则是记录着物理偏移的值，即传统意义上的物理地址。

测试程序如下：

```c++
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

#define PAGEMAP_FILE   "/proc/self/pagemap"
#define PFN_MASK   		((((uint64_t)1) << 55) - 1)
#define PFN_PRESENT_FLAG  (((uint64_t)1) << 63)

int AddrVir2Phy(unsigned long vir_addr, unsigned long *phy_addr)
{
	int fd, ret;
	int page_size = getpagesize();
	unsigned long vir_idx = vir_addr / page_size;
	unsigned long offset = vir_idx * sizeof(uint64_t);
	uint64_t addr;
	
	fd = open(PAGEMAP_FILE, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open failed!\n");
		return -1;
	}
	
	if ((off_t)-1 == lseek(fd, offset, SEEK_SET)) {
		fprintf(stderr, "lseek failed!\n");
		ret = -1;
		goto SAFE_EXIT;
	}
	
	if (sizeof(addr) != read(fd, &addr, sizeof(addr))) {
		fprintf(stderr, "read failed!\n");
		ret = -1;
		goto SAFE_EXIT;
	}
	
	if (0 == (addr & PFN_PRESENT_FLAG)) {
		fprintf(stderr, "page is not present!\n");
		ret = -1;
		goto SAFE_EXIT;
	}
	*phy_addr = (addr & PFN_MASK) * page_size + vir_addr % page_size;
	
SAFE_EXIT:
	close(fd);
	return ret;
}

int MemMmap(unsigned long phy_addr, uint32_t size, unsigned long *vir_addr)
{
	int fd = open("/dev/mem", O_RDWR);
	int page_size = getpagesize();
	unsigned long offset = phy_addr % page_size;
	size = (size + page_size - 1) & ~(page_size - 1);
	phy_addr = phy_addr & ~(page_size - 1);
	*vir_addr = (unsigned long)mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phy_addr);
	if (*vir_addr == (unsigned long)MAP_FAILED) return -1;
	*vir_addr += offset;
	return 0;
}

int main()
{
	unsigned long vir_addr = (unsigned long)malloc(1024 * 1024);
	unsigned long offset, phy_addr, conv_addr;
	strcpy((char *)vir_addr, "hello world");
	AddrVir2Phy(vir_addr, &phy_addr);
	printf("vir: %#lx, phy: %#lx\n", vir_addr, phy_addr);
	int ret = MemMmap(phy_addr, 1024 * 1024, &conv_addr);
	printf("conv: %#lx, phy: %#lx\n", conv_addr, phy_addr);
	printf("result : %s\n", (char *)conv_addr);
	return 0;
}
```

主要测试了从 `malloc` 到转换后的物理地址，再到利用物理地址 `mmap` 出新的虚拟地址，最终获取新的虚拟地址中的值是否正确。

[交叉] 编译方式如下：

```shell
[huangkailun test]$ arm-linux-gnueabihf-g++ test.cpp -o test
```

这里是 *test* 放到设备上运行测出的结果如下：

```shell
[root:/userdata]# ./test
vir: 0xa6ad5008, phy: 0xb0b3008
conv: 0xa69d5008, phy: 0xb0b3008
result : hello world
[root:/userdata]#
```

