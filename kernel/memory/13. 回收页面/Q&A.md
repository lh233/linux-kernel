- kswapd内核线程何时会被唤醒？
- LRU链表如何知道page的活动频繁程度？
- kswapd按照什么原则来换出页面？
- kswapd 按照什么方向来扫描zone?
- kswapd以什么标准来退出扫描LRU?
- 手持设备例如Android系统，没有swap分区或者swap文件，kswapd会扫描匿名页面LRU吗？
- swappiness的含义是什么？kswapd如何计算匿名页面和page cache之间的扫描比重？
- 当系统中充斥着大量只访问一次的文件访问（use-one streaming IO）时，kswapd如何来规避这种风暴？
- 在回收page cache时，对于dirty的page cache,kswapd会马上回写吗？
- 内核中有哪些页面会被kswapd写回交换分区?





在Linux系统中，当内存有盈余时，内核会尽量多地使用内存作为文件缓存（page cache），从而提高系统的性能。文件缓存页面会加入到文件类型的LRU链表中，当系统内存紧张时，文件缓存页面会被丢弃，或者被修改的文件缓存会被回写到存储设备中，与块设备同步之后便可释放出物理内存。现在的应用程序越来越转向内存密集型，无论系统中有多少物理内存都是不够用的，因此Linux系统会使用存储设备当作交换分区，内核将很少使用的内存换出到交换分区，以便释放出物理内存，这个机制称为页交换（swapping），这些处理机制统称为页面回收（page reclaim)。