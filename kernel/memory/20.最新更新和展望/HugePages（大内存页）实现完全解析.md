在《[一文读懂 HugePages的原理](https://mp.weixin.qq.com/s?__biz=MzA3NzYzODg1OA==&mid=2648464691&idx=2&sn=5a55c7171e591f0041779925957cdfda&scene=21#wechat_redirect)》一文中介绍了 HugePages（大内存页）的原理和使用，现在我们来分析一下 Linux 内核是怎么实现 HugePages 分配的。

> 本文使用 Linux 内核 2.6.23 版本

## HugePages分配器初始化

在内核初始化时，会调用 `hugetlb_init` 函数对 HugePages 分配器进行初始化，其实现如下：

```
 1static int __init hugetlb_init(void)
 2{
 3    unsigned long i;
 4
 5    // 1. 初始化空闲大内存页链表 hugepage_freelists, 
 6    //    内核使用 hugepage_freelists 链表把空闲的大内存页连接起来,
 7    //    为了分析简单，我们可以把 MAX_NUMNODES 当成 1
 8    for (i = 0; i < MAX_NUMNODES; ++i)          
 9        INIT_LIST_HEAD(&hugepage_freelists[i]); 
10
11    // 2. max_huge_pages 为系统能够使用的大页内存的数量,
12    //    由系统启动项 hugepages 指定,
13    //    这里主要申请大内存页, 并且保存到 hugepage_freelists 链表中.
14    for (i = 0; i < max_huge_pages; ++i) {
15        if (!alloc_fresh_huge_page())
16            break;
17    }
18
19    max_huge_pages = free_huge_pages = nr_huge_pages = i;
20
21    return 0;
22}
```

`hugetlb_init` 函数主要完成两个工作：

- 初始化空闲大内存页链表 `hugepage_freelists`，这个链表保存了系统中能够使用的大内存。

- 为系统申请空闲的大内存页，并且保存到 `hugepage_freelists` 链表中。

我们再来分析下 `alloc_fresh_huge_page` 函数是怎么申请大内存页的，其实现如下：

```
 1static int alloc_fresh_huge_page(void)
 2{
 3    static int prev_nid;
 4    struct page *page;
 5    int nid;
 6    ...
 7    // 1. 申请一个大的物理内存页...
 8    page = alloc_pages_node(nid, htlb_alloc_mask|__GFP_COMP|__GFP_NOWARN,
 9                            HUGETLB_PAGE_ORDER);
10
11    if (page) {
12        // 2. 设置释放大内存页的回调函数为 free_huge_page
13        set_compound_page_dtor(page, free_huge_page); 
14        ...
15        // 3. put_page 函数将会调用上面设置的 free_huge_page 函数把内存页放入到缓存队列中
16        put_page(page);
17
18        return 1;
19    }
20
21    return 0;
22}
```

所以，`alloc_fresh_huge_page` 函数主要完成三个工作：

- 调用 `alloc_pages_node` 函数申请一个大内存页（2MB）。

- 设置大内存页的释放回调函数为 `free_huge_page`，当释放大内存页时，将会调用这个函数进行释放操作。

- 调用 `put_page` 函数释放大内存页，其将会调用 `free_huge_page` 函数进行相关操作。

那么，我们来看看 `free_huge_page` 函数是怎么释放大内存页的，其实现如下：

```
1static void free_huge_page(struct page *page)
2{
3    ...
4    enqueue_huge_page(page);     // 把大内存页放置到空闲大内存页链表中
5    ...
6}
```

`free_huge_page` 函数主要调用 `enqueue_huge_page` 函数把大内存页添加到空闲大内存页链表中，其实现如下：

```
 1static void enqueue_huge_page(struct page *page)
 2{
 3    int nid = page_to_nid(page); // 我们假设这里一定返回 0
 4
 5    // 把大内存页添加到空闲链表 hugepage_freelists 中
 6    list_add(&page->lru, &hugepage_freelists[nid]);
 7
 8    // 增加计数器
 9    free_huge_pages++;
10    free_huge_pages_node[nid]++;
11}
```

从上面的实现可知，`enqueue_huge_page` 函数只是简单的把大内存页添加到空闲链表 `hugepage_freelists` 中，并且增加计数器。

假如我们设置了系统能够使用的大内存页为 100 个，那么空闲大内存页链表 `hugepage_freelists` 的结构如下图所示：

![](https://mmbiz.qpic.cn/mmbiz_png/ciab8jTiab9J7O12giaufKEicFnn0xF8KUU0RuZS8XqtR0udVt6J6NYld0b2A7fk3IXH8aOayqlUibjS53aNow2KvBQ/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

所以，HugePages 分配器初始化的调用链为：

```
 1hugetlb_init()
 2      |
 3      +——> alloc_fresh_huge_page()
 4                      |
 5                      |——> alloc_pages_node()
 6                      |——> set_compound_page_dtor()
 7                      +——> put_page()
 8                               |
 9                               +——> free_huge_page()
10                                            |
11                                            +——> enqueue_huge_page()
```

## hugetlbfs 文件系统

为系统准备好空闲的大内存页后，现在来了解下怎样分配大内存页。在《[一文读懂 HugePages的原理](https://mp.weixin.qq.com/s?__biz=MzA3NzYzODg1OA==&mid=2648464691&idx=2&sn=5a55c7171e591f0041779925957cdfda&scene=21#wechat_redirect)》一文中介绍过，要申请大内存页，必须使用 `mmap` 系统调用把虚拟内存映射到 `hugetlbfs` 文件系统中的文件中。

免去繁琐的文件系统挂载过程，我们主要来看看当使用 `mmap` 系统调用把虚拟内存映射到 `hugetlbfs` 文件系统的文件时会发生什么事情。

每个文件描述符对象都有个 `mmap` 的<mark>方法</mark>，此方法会在调用 `mmap` 函数映射到文件时被触发，我们来看看 `hugetlbfs` 文件的 `mmap` 方法所对应的真实函数，如下：

```
1const struct file_operations hugetlbfs_file_operations = {
2    .mmap               = hugetlbfs_file_mmap,
3    .fsync              = simple_sync_file,
4    .get_unmapped_area  = hugetlb_get_unmapped_area,
5};
```

从上面的代码可以发现，`hugetlbfs` 文件的 `mmap` 方法被设置为 `hugetlbfs_file_mmap` 函数。所以当调用 `mmap` 函数映射 `hugetlbfs` 文件时，将会调用 `hugetlbfs_file_mmap` 函数来处理。

而 `hugetlbfs_file_mmap` 函数最主要的工作就是把虚拟内存分区对象的 `vm_flags` 字段添加 `VM_HUGETLB` 标志位，如下代码：

![](https://mmbiz.qpic.cn/mmbiz_png/ciab8jTiab9J7O12giaufKEicFnn0xF8KUU0O9MawesngfHpttdmCcZbDQYzWNYbrZmG7oDMZxZ5VaMq9qxx4HkwMg/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

从上图可以看出，使用 HugePages 后，`页中间目录` 直接指向物理内存页。所以，`hugetlb_fault` 函数主要就是对 `页中间目录项` 进行填充。实现如下：

对 `hugetlb_fault` 函数进行精简后，主要完成两个工作：

- 通过触发 `缺页异常` 的虚拟内存地址找到其对应的 `页中间目录项`。

- 调用 `hugetlb_no_page` 函数对 `页中间目录项` 进行映射操作。

我们再来看看 `hugetlb_no_page` 函数怎么对 `页中间目录项` 进行填充：

```
 1static int
 2hugetlb_no_page(struct mm_struct *mm, struct vm_area_struct *vma,
 3                unsigned long address, pte_t *ptep, int write_access)
 4{
 5    ...
 6    page = find_lock_page(mapping, idx);
 7    if (!page) {
 8        ...
 9        // 1. 从空闲大内存页链表 hugepage_freelists 中申请一个大内存页
10        page = alloc_huge_page(vma, address);
11        ...
12    }
13    ...
14    // 2. 通过大内存页的物理地址生成页中间目录项的值
15    new_pte = make_huge_pte(vma, page, ((vma->vm_flags & VM_WRITE)
16                                            && (vma->vm_flags & VM_SHARED)));
17
18    // 3. 设置页中间目录项的值为上面生成的值
19    set_huge_pte_at(mm, address, ptep, new_pte);
20    ...
21    return ret;
22}
```

通过对 `hugetlb_no_page` 函数进行精简后，主要完成3个工作：

- 调用 `alloc_huge_page` 函数从空闲大内存页链表 `hugepage_freelists` 中申请一个大内存页。

- 通过大内存页的物理地址生成页中间目录项的值。

- 设置页中间目录项的值为上面生成的值。

至此，HugePages 的映射过程已经完成。

> 还有个问题，就是 CPU 怎么知道 `页中间表项` 指向的是 `页表` 还是 `大内存页` 呢？
> 
> 这是因为  `页中间表项` 有个 `PSE` 的标志位，如果将其设置为1<mark>，</mark>那么就表明其指向 `大内存页` ，否则就指向 `页表`。

## 总结

本文介绍了 HugePages 实现的整个流程，当然本文也只是介绍了申请内存的流程，释放内存的流程并没有分析，如果有兴趣的话可以自己查阅源
