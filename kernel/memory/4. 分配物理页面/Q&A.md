1. 请简述Linux内核在理想情况下页面分配器（pag allocator）是如何分配出连续的物理页面的。



2. 在页面分配器中，如何从分配掩码（gfp_mask）中确定可以从哪些zone中分配内存？



3. 页面分配器是按照什么方向来扫描zone的？



​	4. 为用户进程分配物理内存，分配掩码应该选用GFP_KERNEL，还是GFP_HIGHUSER_MOVABLE呢？