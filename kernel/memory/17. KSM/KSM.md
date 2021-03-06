内存资源是计算机中比较宝贵的资源，在系统里的物理页面无时不刻不在循环着重新分配和释放，那么是否会有一些内存页面在它们生命周期里某个瞬间页面内容完全一致呢？

在阅读本节前请思考如下小问题。

- KSM是基于什么原理来合并页面的?
- 在KSM机制里，合并过程中把page设置成写保护的函数write protect_page0有这样一个判断：

```
if (page mapcount(page)+1+swapped!=page_count(page)){
	goto out_unlock;
}
```

请问这个判断的依据是什么？



- 如果多个VMA的虚拟页面同时映射了同一个匿名页面，那么此时page->index应该等于多少？

KSM全称Kernel SamePage Merging，用于合并内容相同的页面。KSM的出现是为了优化虚拟化中产生的冗余页面，因为虚拟化的实际应用中在同一台宿主机上会有许多相同的操作系统和应用程序，那么许多内存页面的内容有可能都是相同的，因此它们可以被合并，从而释放内存供其他应用程序使用。

KSM允许合并同一个进程或不同进程之间内容相同的匿名页面，这对应用程序来说是不可见的。把这些相同的页面被合并成一个只读的页面，从而释放出来物理页面，当应用程序需要改变页面内容时，会发生写时复制（copy-on-write，COW)。