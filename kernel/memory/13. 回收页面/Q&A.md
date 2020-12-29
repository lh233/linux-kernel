- kswapd内核线程何时会被唤醒？

  答：分配内存时，当在zone的WMARK_LOW水位分配失败时，会去唤醒kswapd内核线程来回收页面。

- LRU链表如何知道page的活动频繁程度？

  答：LRU链表按照先进先出的逻辑，页面首先进入LRU链表头，然后慢慢挪动到链表尾，这有一个老化的过程。另外，page中有PG_reference/PG_active标志位和页表的PTE_YOUNG位来实现第二次机会法。

- kswapd按照什么原则来换出页面？

  答：页面在活跃LRU链表，需要从链表头到链表尾的一个老化过程才能迁移到不活跃LRU链表。在不活跃LRU链表中又经过一个老化过程后，首先剔除那些脏页面或者正在回写的页面，然后那些在不活跃LRU链表老化过程中没有被访问引用的页面是最佳的被换出的候选者，具体请看shrink_page_list()函数。

- kswapd 按照什么方向来扫描zone?

  答：从低zone到高zone，和分配页面的方向相反。

- kswapd以什么标准来退出扫描LRU?

  答：页面在活跃LRU链表，需要从链表头到链表尾的一个老化过程才能迁移到不活跃LRU链表。在不活跃LRU链表中又经过一个老化过程后，首先剔除那些脏页面或者正在回写的页面，然后那些在不活跃LRU链表老化过程中没有被访问引用的页面是最佳的被换出的候选者，具体请看shrink_page_list()函数。

- 手持设备例如Android系统，没有swap分区或者swap文件，kswapd会扫描匿名页面LRU吗？

  答：没有swap分区不会扫描匿名页面LRU链表，详见get_scancount()函数。

- swappiness的含义是什么？kswapd如何计算匿名页面和page cache之间的扫描比重？

  答：swappiness用于设置向swap分区写页面的活跃程度，详见get_scan_count()函数。

- 当系统中充斥着大量只访问一次的文件访问（use-one streaming IO）时，kswapd如何来规避这种风暴？

  答：page_check_reference()函数设计了一个简易的过滤那些短时间只访问一次的page cache的过滤器，详见page_check_references()函数。

- 在回收page cache时，对于dirty的page cache,kswapd会马上回写吗？

  答：不会，详见shrink_page_list()函数。

- 内核中有哪些页面会被kswapd写回交换分区?

  答：匿名页面，还有一种特殊情况，是利用shmem机制建立的文件映射，其实也是使用的匿名页面，在内存紧张时，这种页面也会被swap到交换分区。





在Linux系统中，当内存有盈余时，内核会尽量多地使用内存作为文件缓存（page cache），从而提高系统的性能。文件缓存页面会加入到文件类型的LRU链表中，当系统内存紧张时，文件缓存页面会被丢弃，或者被修改的文件缓存会被回写到存储设备中，与块设备同步之后便可释放出物理内存。现在的应用程序越来越转向内存密集型，无论系统中有多少物理内存都是不够用的，因此Linux系统会使用存储设备当作交换分区，内核将很少使用的内存换出到交换分区，以便释放出物理内存，这个机制称为页交换（swapping），这些处理机制统称为页面回收（page reclaim)。

