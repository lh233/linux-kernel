1. 请简述Linux内核在理想情况下页面分配器（pag allocator）是如何分配出连续的物理页面的。

页面分配器是通过alloc_pages()函数来分配页面的，最核心的函数则是__alloc_pages_nodemask()函数。当然也需要考虑是NUMA系统或者是UMA系统（选择NUMA系统时，说明最高物理地址和最低物理地址之间存在空洞，空洞的空间当然是非均质的；）

（1）首先根据gfp_mask控制选择区的位置去向（gfp_zone(gfp_mask)函数和gfpflags_to_migratetype(gfp_mask)），然后通过get_page_from_freelist()去尝试分配物理页面，扫描zone。

（2）内核的内存中有三个水位，大于低水位时，直接通过__rmqueue从管理区分配，否则让zone_reclaim()函数回收一些页面备用。在   `rmqueue()`会逐个尝试各个队列，如果空间不够的话就会调用expand()函数到大队列中。

（3）要是还不行，就会跳到下一个内存管理区，重新用rmqueque函数尝试各个空闲块。（NUMA系统）

（4）还不行，使用alloc_pages_limit函数回收inactive clean pages，

（5）再不行，唤醒kswapd函数设法从缓存中获取页面

（6）再失败，这时看优先级，普通时会分析是由于页面确实不足还是碎片太多，所以先整理碎片，在rmqueque

还是不行，就只能等待啦！

2. 在页面分配器中，如何从分配掩码（gfp_mask）中确定可以从哪些zone中分配内存？

分配掩码是在内核代码中分成两类，一类叫zone modifiers，另一类是action modifiers。zone modifiers指定从哪一个zone中分配所需的页面。zone modifiers由分配掩码的最低4位来定义，分别是`___GFP_DMA`、`___GFP_HIGHMEM`、`___GFP_DMA32`和`___GFP_MOVABLE`。

```
/* If the above are modified, __GFP_BITS_SHIFT may need updating */

/*
 * GFP bitmasks..
 *
 * Zone modifiers (see linux/mmzone.h - low three bits)
 *
 * Do not put any conditional on these. If necessary modify the definitions
 * without the underscores and use them consistently. The definitions here may
 * be used in bit comparisons.
 */
#define __GFP_DMA	((__force gfp_t)___GFP_DMA)
#define __GFP_HIGHMEM	((__force gfp_t)___GFP_HIGHMEM)
#define __GFP_DMA32	((__force gfp_t)___GFP_DMA32)
#define __GFP_MOVABLE	((__force gfp_t)___GFP_MOVABLE)  /* Page is movable */
#define GFP_ZONEMASK	(__GFP_DMA|__GFP_HIGHMEM|__GFP_DMA32|__GFP_MOVABLE)
```

GFP_KERNEL分配掩码定义在gfp.h头文件上，是一个分配掩码的组合。详情参考伙伴分配内存的内容；

3. 页面分配器是按照什么方向来扫描zone的？

`get_page_from_freelist()`函数首先需要先判断可以从哪一个zone来分配内存。`for_each_zone_zonelist_nodemask`宏扫描内存节点中的zonelist去查找合适分配内存的zone。

例如：分配掩码GFP_HIGHUSER_MOVABLE的值为0x200da，那么gfp_zone(GFP_HIGHUSER_MOVABLE)函数等于2，即highest_zoneidx为2，而这个内存节点的第一个ZONE_HIGHME，其zone编号zone_index的值为1；

- 在`first_zones_zonelist()`函数中，由于第一个zone的zone_index值小于highest_zoneidx，因此会返回ZONE_HIGHMEM。

- 在`for_each_zone_zonelist_nodemask()`函数中，next_zones_zonelist(++z, highidx, nodemask)依然会返回ZONE_NORMAL；

- 因此这里会遍历ZONE_HIGHMEM和ZONE_NORMAL，这两个zone，但是会先遍历ZONE_HIGHMEM，然后才是ZONE_NORMAL。

  

​	4. 为用户进程分配物理内存，分配掩码应该选用GFP_KERNEL，还是GFP_HIGHUSER_MOVABLE呢？

用户进程用GFP_HIGHUSER_MOVABLE.

```
/* 以下三个用于为用户空间申请内存 */
#define GFP_USER    (__GFP_WAIT | __GFP_IO | __GFP_FS | __GFP_HARDWALL) /* 可以睡眠，可以有IO和VFS操作，只能从进程可运行的node上分配内存 */
#define GFP_HIGHUSER    (__GFP_WAIT | __GFP_IO | __GFP_FS | __GFP_HARDWALL | __GFP_HIGHMEM)  /* 优先从高端zone中分配内存 */
#define GFP_HIGHUSER_MOVABLE    (__GFP_WAIT | __GFP_IO | __GFP_FS | \
                 __GFP_HARDWALL | __GFP_HIGHMEM | __GFP_MOVABLE) /* 申请可移动的内存 */
```

