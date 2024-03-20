## 使能KSM

KSM只会处理通过madvise系统调用显式指定的用户进程地址空间，因此用户程序想使用这个功能就必须在分配地址空间时显式地调用madvise（addr，length，MADV_MERGEA BLE）。如果用户想在KSM中取消某一个用户进程地址空间的合并功能，也需要显式地调用madvise（addr，length,MADV_UNMERGEABLE)。 下面是测试KSM的test.c程序的代码片段，使用mmap()：来创建一个文件的私有映射，并且调用memset()写入这些私有映射的内容缓存页面中。

```
《测试KSM的test.c程序的代码片段》
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
int main(int argc,char *argv[])
{
    char *buf;
    char filename[64]="";
    struct stat stat;
    int size =100*4096;
    int fd =0;

    strcpy(filename, argv[1]);

    fd = open(filename,O_RDWR | O_CREAT,0664);

    fstat(fd, &stat);

    buf = mmap (NULL,stat.st_size,PROT_WRITE,MAP_PRIVATE,fd,0);

    memset(buf,0x55,stat.st_size);

    madvise(buf,stat.st_size, MADV_MERGEABLE);

    while (1)
        sleep(1);
}
```

编译上述test.c程序。

```
gcc test.c -o test
```

使用dd命令创建一个ksm.dat文件，即创建100MB大小的文件。

```
echo 1 >/sys/kernel/mm/ksm/run
```

运行test.c程序。

```
#./test ksm.dat
```

过一段时间之后，查看系统有多少页面合并了。

```
root@benshushu#cat /sys/kernel/mm/ksm/pages_sharing
25500
root@benshushu#cat /sys/kernel/mm/ksm/pages_shared
100
root@benshushu:/home#cat /sys/kernel/mm/ksm/pages_unshared
0
```

可以看到pages_shared为100说明系统有100个共享的页面。若有100个页面的内同，它们可以合并成一个页面，这时pages_shared为1。 pages_sharing 为25500说明有25500个页面合并了。 100MB的内存可存放25600个页面。因此，我们可以看到，KSM把这25600个页面分别合并成1共享的页面，每一个共享页面里共享了其他的255个页面，为什么会这样？我们稍后详细解析。 pages_unshared表示当前未合并页面的数量。

```
rootebenshushut cat /sys/kernel/mn/ksm/stable_node_chains
1
rootebenshushut cat /sys/kernel/mm/ksm/stable_node_dups
100
```

stable_node_chains表示包含了链式的稳定节点的个数，当前系统中为1，说明只有一个链式的稳定节点，但是这个稳定的节点里包含了链表。 stable_node_dups表示稳定的节点所在的链表包含的元素总数。 KSM的sysfs节点在/sys/kernel/mm/ksm/目录下，其主要节点的描述如下所示。

-   run：可以设置为0~2。若设置0，暂停ksmd内核线程；若设置1，启动ksmd内核线程；若设置2，取消所有已经合并好的页面
-   full_scans: 完整扫描和合并区域的次数
-   pages_volatile: 表示还没扫描和合并的页面数量。若由于页面内容更改过快导致两次计算的校验值不相等，那么这些页面是无法添加到红黑树里的
-   sleep_millisecs: ksmd内核线程扫描一次的时间间隔
-   pages_to_scan: 单次扫描页面的数量
-   pages_shared: 合并后的页面数。如果100个页面的内容相同，那么可以把它们合并成一个页面，这时pages_shared的值为1
-   pages_sharing: 共享的页面数。如果两个页面的内容相同，它们可以合并成一个页面，那么有一个页面要作为稳定的节点，这时pages_shared的值为1，pages_sharing也为1。第3个页面也合并进来后，pages_sharing的值为2，表示两个页面共享同一个稳定的节点
-   pages_unshared: 当前未合并页面数量
-   max_page_sharing: 这是在Linux4.3内核中新增的参数，表示一个稳定的节点最多可以合并的页面数量。这个值默认是256
-   stable_node_chains: 链表类型的稳定节点的个数。每个链式的稳定节点代表页面内容相同的KM页面。这个链式的稳定节点可以包含多个dup成员，每个dup成员最多包含256个共享的页面
-   stable_node_dups: 链表中dup成员的个数。这些dup成员会连接到链式的稳定节点的hlist链表中

KSM在初始化时会创建一个名为ksmd的内核线程。

```
<mm/ksm.c>
static int __init ksm_init(void)
{
    ksm_thread = kthread_run(ksm_scan_thread, NULL, "ksmd");
}
subsys_initcall(ksm_init);
```

在tes.c程序中创建私有映射（MAP_PRIVATE）之后，显式地调用madvise系统调用把用户进程地址空间添加到 Linux内核的KSM系统中。

```
<madvise()->ksm_madvise()-> ksm_enter()>

int __ksm_enter(struct mm_struct *mm)
{
    mm_slot = alloc_mm_slot();
    insert_to_mm_slots_hash(mm, mm_slot);
    list_add_tail(&mm slot->mm_list, &ksm_scan.mm_slot->mm list);
    set_bit(MME_VM_MERGEABLE, &mm->flags);
}
```

ksm_enter()函数会把当前的 mm_struct数据结构添加到 mm_slots_hash哈希表中。另外把 mm_slot添加到 ksm_scan.mm_slot->mm_list 链表中。最后，设置mm->flags中的 MMF_VM_MERGEABLE标志位，表示这个进程已经被添加到KSM系统中.

```
<ksm内核线程>

static int ksm_scan_thread(void *nothing)
{
    while (!kthread_should_stop())
        if (ksmd_should_run())
            ksm_do_scan(ksm_thread_pages_to_scan)
    if (ksmd_should_run()) {
        sleep_ms =READ_ONCE(ksm_thread_sleep_millisecs);
        wait_event_interruptible_timeout(ksm_iter_wait,
        sleep_ms != READ_ONCE(ksm_thread_sleep_millisecs),
        secs_to_jiffies(sleep_ms));
    }
    return 0;
}
```

ksm_scan_thread()是ksmd内核线程的主干，它运行 ksm_do_scan()函数，扫描和合并100个页面，见 ksm_thread_pages_to_scan参数，然后等待20ms，见 ksm_thread_sleep_millisecs参数，这两个参数可以在/sys/kernel/mm/ksm目录下设置和修改。

```
<ksmd内核线程>

static void ksm_do_scan(unsignd int scan_npages)
{
    while(scan_npages-- && likely(!freezing(current))) {
        cond_resched();
        rmap_item = scan_get_next_rmap_item(&page);
        if (!rmap_item)
            return;
        cmp_and_merge_page(page, rmap_item);
        put_page(page)
    }
}
```

ksm_do_scan()函数在while循环中尝试合并scan_npages个页面， scan_get_next_rmap_item()获取一个合适的匿名页面。 cmp_and_merge_page()函数会让页面在KSM中稳定和不稳定的两棵红黑树中查找是否有可以合并的对象，并且尝试合并他们。



## KSM基本实现

为了让读者先有一个初步的认识，本节先介绍Lnux4.13内核之前的KSM实现，后文会介绍Linux5.0内核中的实现。

KSM机制下采用两棵红黑树来管理扫描的页面和己经合并的页面。第一棵红黑树称为不稳定红黑树，里面存放了还没有合并的页面；第二棵红黑树称为稳定红黑树，已经合并的页面会生成一个节点，这个节点为稳定节点。如两个页面的内容是一样的，KSM扫描并发现了它们，因此这两个页面就可以合并成一个页面。对于这个合并后的页面，会设置只读属性，其中一个页面会作为稳定的节点挂载到稳定的红黑树中之后，另外一个页面就会被释放了。但是这两个页面的 rmap_item数据结构会被添如到稳定节点中的 hist 链表中，如下图所示。

![](https://carlyleliu.github.io/picture/linux/kernel/mem/ksm1.png)

我们假设有3个VMA（表示进程地址空间，VMA的大小正好是一个页面的大小，分别有3个页面映射这3个VMA。这3个页面准备通过KSM来扫描和合并，这3个页面的内容是相同的。具体步骤如下。

-   3个页面会被添加到KSM中，第一轮扫描中分别给这3个页面分配 rmap_item数据结构来描述它们，并且分别给它计算校验和，如图（a）所示
-   第二轮扫描中，先扫描page0，若当前稳定的红黑树没有成员，那么不能比较和加入稳定的红黑树。接着，第二次计算校验值，如果 page0的校验值没有发生变化，那么把page0的rmap_item()添加到不稳定的红黑树中，如图（b）所示。如果此时校验值发生了变化，说明页面内容发生变化，这种页面不适合添加到不稳定的红黑树中
-   