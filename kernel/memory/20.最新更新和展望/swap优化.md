当系统内存紧张时，SWAP子系统把匿名页面写入SWAP分区从而释放出空闲页面，长期以来swapping动作是一个低效的代名词。当系统有大量的匿名页面要向SWAP分区写入时，用户会感觉系统卡顿，所以很多Linux用户关闭了SWAP功能。

如何提高页面回收的效率一直是内核社区中热烈讨论的问题，主要集中在如下两个方面。

-   优化LRU算法和页面回收机制。
-   优化 SWAP性能。

前者是很热门的方向，内核社区先后提出了很多大大小小的优化补丁，例如过滤只读一次的page cache风暴、调整活跃LRU和不活跃LRU的比例、调整匿名页面和 page cache之间的比例、Refault Distance算法、把LRU 从zone迁移到node节点等。

后者则相对冷门很多。回收匿名页面要比回收page cache要复杂很多，一是如果page cache的内容没有被修改过，那么这些页面不需要回写到磁盘文件中，直接丢弃即可;二是通常page cache都是从磁盘中读取或者写入大块连续的空间，而匿名页面通常是分散地写入磁盘交换分区中，scattered IO操作是很浪费时间的。随着SSD的普及，swapping 的性能也有很大程序的提高。Tim Chen等社区专家最近在Linux 4.8内核上对SWAP子系统做了大量的研究和测试后，提出了很棒的优化补丁，该补丁主要集中优化如下两方面。

（1）CPU操作 swap磁盘时需要获取一个全局的spinlock锁，该锁在swap_info_struct数据结构中，通常是一个swap分区有一个swap _info_struct 数据结构。当swapping任务很重时，对该锁的争用会变得很激烈，这样会导致swap的性能下降。

优化的方法如下。

-   不需要一个全局的锁，每个swap cluster_info中新定义一个spinlock 锁即可，这样减小了锁的粒度。
-   另一个重要的优化是采用Per-cpu Slots Cache。在swap out时需要在swap分区上分配swap slot,我们一次分配多个slots,把暂时不用的slots放在Per-cpu Slots Cache中。这样下次再需要swap slot时就不用争用swap_info_struct的锁。同样，在swapin结束释放swap slots时也把不用的swap slots放在 Per-cpu Slots Cache 中，积聚一定量的swap slots后才一次性地将它们释放，减少对swap_info_struct的锁争用。

(2) struct address_space数据结构指针用于描述内存页面和其对应的存储关系，例如swap分区。那么改变swap分配信息需要更新address_space指向的基数树（radix tree)，基数树有一个全局的锁来保护，因此这里也遇到了锁争用的问题。

解决办法是在每个64MB的swap空间中新增一个锁，相当于减小锁粒度。