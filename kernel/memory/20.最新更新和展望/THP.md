## THP机制

使用huge page，可以在TLB容量固定的情况下，提高TLB的命中率，即便TLB miss，因为减少了页表级数，也可以减少查找页表的时间。在内存虚拟化中，由于地址转换需要的级数更多，huge page能发挥的作用就显得更为重要。

针对64位的x86-64系统，huge page的大小是2MB或者1GB，初始数目由启动时的"vm.nr_hugepages" 内核参数确定，对于2MB的huge page，运行过程中可通过"/proc/sys/vm/nr_hugepages"修改。

![](https://pic3.zhimg.com/v2-37dd46aa653c08718413669d5eea1272_r.jpg)

但对于1GB的huge page，就只能在启动时分配（且分配后不能释放），而不支持在运行时修改（系统起来后再要倒腾出1GB连续的物理内存，也怪难为内核的）：

![](https://pic1.zhimg.com/80/v2-ab748f91aea266d1a78e4edfe5952098_720w.webp)

Linux针对huge page提供了一种特殊的hugetlbfs文件系统：

```
mount -t hugetlbfs hugetlbfs /dev/hugepages
```

同tmpfs类似，基于hugetlbfs创建文件后，再进行mmap()映射，就可以访问这些huge page了（使用方法可参考内核源码"tools/testing/selftests/vm"中的示例）。

### 【另一种huge page】

在Linux中，除了这种普通的huge page，自2.6.38版本开始支持THP。在应用需要huge page的时候，可通过memory compaction操作移动页面，以形成一个huge page，因为该过程不会被应用感知到，所以被称为"transparent"。

与之相对的，可以把普通的huge page称为"static huge page"。一个是动态的，一个是静态的。两者的关系，类似于上文介绍的CMA和预留DMA的关系。如果以向云厂商购买机器来做个类比的话，静态huge page就是 dedicate 的物理服务器，底层的硬件资源只属于你，而THP就是虚拟机，资源是动态调配的。

THP最开始只支持一种huge page的大小（比如2MB），自3.8版本加入这个patch之后，利用shmget()/mmap()的flag参数中未使用的bits（参考这篇文章），可以支持其他的huge page大小（比如1GB）。

### 【应用的限制】

此外，早期的THP还只支持anonymous pages，而不支持page cache，从它在"/proc/meminfo"中的名字"AnonHugePages"也可以看出来。

这一是因为anonymous pages通常只能通过mmap映射访问，而page cache还可以通过直接调用read()和write()访问，huge page区别于normal page的体现就是少了一级页表转换，不通过映射访问的话，对huge page的使用就比较困难。

二是如果使用THP的话，需要文件本身足够大，才能充分利用huge page带来的好处，而现实中大部分的文件都是比较小的（参考这篇文章）。

不过在某些场景下，让THP支持page cache的需求还是存在的。实现的方法大致说来有两种：一种是借助既有的compoud page的概念实现的支持THP的tmpfs，另一种是使用一个新的表达一组pages的概念，即team page（参考这篇文章）。

选择tmpfs入手是因为作为一个文件系统，它很特殊，从某种意义上说，它不算一个“货真价实”的文件系统。但它这种模棱两可的特性正好是一个绝佳的过渡，实现了THP对tmpfs的支持之后，可以进一步推广到ext4这种标准的磁盘文件系统中去，但……还很多问题要解决，比如磁盘文件系统的readahead机制，预读窗口通常是128KB，这远小于一个huge page的大小（参考这篇文章）。

### 【存在的问题】

静态huge page拥有一套独立的内存系统，跟4KB的normal page构成的普通内存可以说是井水不犯河水。而且，静态huge page也是不支持swap操作的，不能被换出到外部存储介质上。

THP和静态huge page看起来样子差不多，但在Linux中的归属和行为却完全不同。这么说吧，后者是一体成型的，而前者就像是焊接起来的。THP虽然勉强拼凑成了huge page的模样，但骨子里还是normal page，还和normal page同处一个世界。

在它诞生之初，面对这个庞然大物，既有的内存管理子系统的机制还没做好充分的应对准备，比如THP要swap的时候怎么办啊？这个时候，只能调用split_huge_page()，将THP重新打散成normal page。

swap out的时候打散，swap in的时候可能又需要重新聚合回来，这对性能的影响是不言而喻的。一点一点地找到空闲的pages，然后辛辛苦苦地把它们组合起来，现在到好，一切都白费了（路修了又挖，挖了又修……）。虽然动态地生成huge page确实能更充分利用物理内存，但其带来的收益，有时还真不见得能平衡掉这一来一去的损耗。

不过呢，内核开发者也在积极努力，希望能够实现THP作为一个整体被swap out和swap in（参考这篇文章），但这算是对“牵一发而动全身”的内存子系统的一次重大调整，所以更多的regression测试还在进行中。

可见啊，THP并没有想象的那么美好，用还是不用，怎么用，就成了一个需要思考和选择的问题。

### 【使用策略】

以RedHat的发行版为例，自RHEL 6开始，THP都是默认打开的，如果要禁止，应该在内核的启动参数里设置"transparent_hugepage=never"。系统运行起来后，也可以动态地调整（on-the-fly）：

![](https://pic2.zhimg.com/80/v2-3b32d8e71f1014536e270222c7e2a0f5_720w.webp)

"always"和"never"的意义比较明显，而"madvise"的意思是只有显式地使用了madvise(MADV_HUGEPAGE) 相关的接口，才启用THP。Linux中使用一个单独的线程khugepaged来负责实现THP，不管设置为"always"还是"madvise"，khugepaged都是会被启动的（要时刻做好准备嘛）。

需要注意的是，如果动态地将"enable"更改为"never" ，则只能保证之后不能生成新的THP了，但之前的THP还会继续存在，不会被打散为normal page。

前面的文章说过，memory compaction的意义不仅在于当前能够分配一段连续的物理内存，还需要未雨绸缪，通过defragmentation操作为将来的huge page分配提供便利，对此，内核同样提供了相关的调整参数：

![](https://pic4.zhimg.com/80/v2-3dde3097abd3006fcae037f2a6891e73_720w.webp)

这些选项实际提供了对“抗碎页”激进程度的把控，比如使用"defer"，那么就会借助kcompactd内核线程来挑选和移动空闲的normal pages，达到一定数量后，再由khugepaged将这些normal pages合并为THP。

总的来说，目前THP优劣的平衡还存在一定的不确定性，在虚拟化的应用中，如果物理内存充足，通常建议使用单独划分的静态huge page，而不使用THP（参考华为鲲鹏平台的优化建议）。

