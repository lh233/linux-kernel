如前文所述，memcg的整体框架如下：

![](https://img-blog.csdnimg.cn/20190408184203278.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3pzajEwMDIxMw==,size_16,color_FFFFFF,t_70)

## 1      概述

### 1.1      应用背景

Cgroup的memory子系统，即memory cgroup(本文以下简称memcg)，提供了对系统中一组进程的内存行为的管理，从而对整个系统中对内存有不用需求的进程或应用程序区分管理，实现更有效的资源利用和隔离。

在实际业务场景中，为了防止一些应用程序对资源的滥用（可能因为应用本身的bug，如内存泄露），导致对同一主机上其他应用造成影响，我们往往希望可以控制应用程序的内存使用量，这是memcg提供的主要功能之一，当然它还可以做的更多。

Memcg的应用场景，往往来自一些虚拟化的业务需求，所以memcg往往作为cgroup的一个子系统与容器方案一起应用。在容器方案中，与一般的虚拟化方案不同，memcg在管理内存时，并不会在物理内存上对每个容器做区分，也就是说所有的容器使用的是同一个物理内存（有一种例外情况，如果存在多个内存节点，则可以通过cgroup中的cpuset子系统将不同的内存节点应用到不同的容器中）。对于共用的物理内存，memcg也不会对不同的容器做物理页面的预分配，也就是说同一个内存page，可能会被容器A使用，也可能被容器B使用。

所以memcg应用在容器方案中，虽然没有实现真正意义上的内存虚拟化，但是通过内核级的内存管理，依然可以实现某种意义上的虚拟化的内存管理，而且是真正的轻量级的。

### 1.2      功能简介

Memcg的主要应用场景有：

a.     隔离一个或一组应用程序的内存使用

对于内存饥渴型的应用程序，我们可以通过memcg将其可用内存限定在一定的数量以内，实现与其他应用程序内存使用上的隔离。

b.    创建一个有内存使用限制的控制组

比如在启动的时候就设置mem=XXXX。

c.     在虚拟化方案中，控制虚拟机的内存大小

比如可应用在LXC的容器方案中。

d.    确保应用的内存使用量

比如在录制CD/DVD时，通过限制系统中其他应用可以使用的内存大小，可以保证录制CD/DVD的进程始终有足够的内存使用，以避免因为内存不足导致录制失败。

e.     其他

各种通过memcg提供的特性可应用到的场景。

为了支撑以上场景，这里也简单列举一下memcg可以提供的功能特性：

a.     统计anonymous pages, file caches, [swap](https://so.csdn.net/so/search?q=swap&spm=1001.2101.3001.7020) caches的使用并限制它们的使用；

b.    所有page都链接在per-memcg的LRU链表中，将不再存在global的LRU；

c.     可以选择统计和限制memory+swap的内存；

d.    对hierarchical的支持；

e.     Soft limit；

f.     可以选择在移动一个进程的时候，同时移动对该进程的page统计计数；

g.     内存使用量的阈值超限通知机制；

h.    可以选择关闭oom-killer，并支持oom的通知机制；

i.      Root cgroup不存在任何限制；

## 2      总体设计

### 2.1      Memcg

Memcg在cgroup体系中提供memory隔离的功能，它跟cgroup中其他子系统一样可以由admin创建，形成一个树形结构，可以将进程加入到这些memcg中管理。

Memcg设计的核心是一个叫做res_counter的结构体，该结构体跟踪记录当前的内存使用和与该memcg关联的一组进程的内存使用限制值，每个memcg都有一个与之相关的res_counter结构。

![](https://img-blog.csdn.net/20180711152005405?watermark/2/text/aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3RhbnpoZTIwMTc=/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70)

而mem_cgroup结构体中通常会有两个res_counter结构，这是因为为了实现memory隔离，每个memcg主要要有两个维度的限制：

a.     Res – 物理内存；

b.    Memsw – memory + swap，即物理内存 + swap内存；

其中，memsw肯定是大于等于memory的。

另外，从res_counter结构中可以看出，每个维度又有三个指标：

a.     Usage – 组内进程已经使用的内存；

b.    Soft_limit – 软限制，非强制内存上限，usage超过这个上限后，组内进程使用的内存可能会加快步伐进行回收；

c.     Hard_limit – 硬限制，强制内存上限，usage不能超过这个上限，如果试图超过，则会触发同步的内存回收，或者触发OOM（详见OOM章节）。

其中，soft_limit和hard_limit都是admin在memcg的配置文件中进行配置的（soft_limit必须要小于hard_limit才能发挥作用），hard_limit是真正的内存限制，soft_limit只是为了实现更好的内存使用效果而做的辅助，而usage则是内核实时统计该组进程内存的使用值。

对于统计功能的实现，可以用一个简单的图表示：

![](https://img-blog.csdn.net/20180711152103555?watermark/2/text/aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3RhbnpoZTIwMTc=/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70)

该过程中涉及的各种实现细节，将在后面的章节进行分解描述。

### 2.2      Page & swap