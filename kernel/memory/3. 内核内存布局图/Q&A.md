1. 在32bit linux中，内核线性映射的虚拟地址和物理地址是如何换算出来的？

用户空间和内核空间使用3:1的划分方法时，内核空间只有1GB大小。这1GB的映射空间，其中有一部分用于直接映射物理地址。这个区域称为线性映射区。在ARM32平台上，物理地址[0:760MB]的这一部分内存被线性映射到[3GB:3GB+768MB]的虚拟地址上。**线性映射区的虚拟地址和物理地址相差PAGE_OFFSET**，即3GB。内核中有相关的宏来实现线性映射区虚拟地址与物理地址的查找过程，例如`__pa(x)`和`__va(x)`

```
[arch/arm/include/asm/memory.h]
#define __pa(x)	__virt_to_phys((unsigned long)(x))
#define __va(x) ((void *)__phys_to_virt(phys_addr_t)(x))
static inline phys_addr_t __virt_to_phys(unsigned long x)
{
	return (phys_addr_t)x - PAGE_OFFSET + PHYS_OFFSET;
}

static inline unsigned long __phys_to_virt(phys_addr_t x)
{
	return x - PHYS_OFFSET + PAGE_OFFSET;
}

```



2. 在32bit linux中，高端内存的起始地址是如何计算出来的呢？

在内核初始化内存时，在`santiy_check_meminfo()`函数中确定高端内存的起始地址，全局变量high_memory来存放高端内存的起始地址。

```
static void * __initdata vmalloc_min =
	(void *)(VMALLOC_END - (240 << 20) - VMALLOC_OFFSET);
void __init sanity_check_meminfo(void)
{
	phys_addr_t vmalloc_limit = __pa(vmalloc_min - 1) + 1;
	arm_lowmem_limit = vmalloc_limit;
	high_memory = __va(arm_lowmem_limit - 1) + 1;
}
```

vmalloc_min计算出来的结果是0x2F80_0000，即760MB；





3. 请画出ARM32 Linux 内核内存分布图

![](../picture/ARM32内核内存布局图.png)