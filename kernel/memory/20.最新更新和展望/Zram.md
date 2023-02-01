## 1. 技术背景
说到压缩这个词，我们并不陌生，应该都能想到是降低占用空间，使同样的空间可以存放更多的东西，类似于我们平时常用的文件压缩,内存压缩同样也是为了节省内存。

尽管当前android手机6GB，8GB甚至12GB的机器都较为常见了，但内存无论多大，总是会有不够用的时候。当系统内存紧张的时候，会将文件页丢弃或回写回磁盘（如果是脏页），还可能会触发LMK杀进程进行内存回收。这些被回收的内存如果再次使用都需要重新从磁盘读取，而这个过程涉及到较多的IO操作。就目前的技术而言，IO的速度远远慢于这RAM操作速度。因此，如果频繁地做IO操作，不仅影响flash使用寿命，还严重影响系统性能。内存压缩是一种让IO过程平滑过渡的做法, 即尽量减少由于内存紧张导致的IO，提升性能。

## 2. 主流内存压缩技术
目前linux内核主流的内存压缩技术主要有3种：zSwap, zRAM, zCache。

### 2.1 zSwap
zSwap是在memory与flash之间的一层“cache”,当内存需要swap出去磁盘的时候，先通过压缩放到zSwap中去，zSwap空间按需增长。达到一定程度后则会按照LRU的顺序(前提是使用的内存分配方法需要支持LRU)将就最旧的page解压写入磁盘swap device，之后将当前的page压缩写入zSwap。

![image](http://www.wowotech.net/content/uploadfile/202003/5e0cc315e337296bbcd4ad5be492dcc120200308003812.png)

zswap本身存在一些缺陷或问题:

1) 如果开启当zswap满交换出backing store的功能, 由于需要将zswap里的内存按LRU顺序解压再swap out, 这就要求内存分配器支持LRU功能。

2) 如果不开启当zswap满交换出backing store的功能, 和zRam是类似的。

### 2.2 zRram
zRram即压缩的内存， 使用内存模拟block device的做法。实际不会写到块设备中去，只会压缩后写到模拟的块设备中，其实也就是还是在RAM中，只是通过压缩了。由于压缩和解压缩的速度远比读写IO好，因此在移动终端设备广泛被应用。zRam是基于RAM的block device, 一般swap priority会比较高。只有当其满，系统才会考虑其他的swap devices。当然这个优先级用户可以配置。

zRram本身存在一些缺陷或问题:

1) zRam大小是可灵活配置的, 那是不是配置越大越好呢? 如果不是,配置多大是最合适的呢?

2) 使用zRam可能会在低内存场景由于频繁的内存压缩导致kswapd进程占CPU高, 怎样改善?

3) 增大了zRam配置,对系统内存碎片是否有影响?

要利用好zRam功能, 并不是简单地配置了就OK了, 还需要对各种场景和问题都做好处理, 才能发挥最优的效果。

### 2.3 zCache

zCache是oracle提出的一种实现文件页压缩技术，也是memory与block dev之间的一层“cache”,与zswap比较接近，但zcache目前压缩的是文件页，而zSwap和zRAM压缩是匿名页。

zcache本身存在一些缺陷或问题:

1) 有些文件页可能本身是压缩的内容, 这时可能无法再进行压缩了

2) zCache目前无法使用zsmalloc, 如果使用zbud,压缩率较低

3) 使用的zbud/z3fold分配的内存是不可移动的, 需要关注内存碎片问题


## 3. 内存压缩主流的内存分配器
### 3.2.1 Zsmalloc
zsmalloc是为ZRAM设计的一种内存分配器。内核已经有slub了， 为什么还需要zsmalloc内存分配器？这是由内存压缩的场景和特点决定的。zsmalloc内存分配器期望在低内存的场景也能很好地工作，事实上，当需要压缩内存进行zsmalloc内存分配时，内存一般都比较紧张且内存碎片都比较严重了。如果使用slub分配， 很可能由于高阶内存分配不到而失败。另外，slub也可能导致内存碎片浪费比较严重，最坏情况下，当对象大小略大于PAGE_SIZE/2时，每个内存页接近一般的内存将被浪费。

Android手机实测发现，anon pages的平均压缩比大约在1:3左右，所以compressed anon page size很多在1.2K左右。如果是Slub，为了分配大量1.2K的内存，可能内存浪费严重。zsmalloc分配器尝试将多个相同大小的对象存放在组合页（称为zspage）中，这个组合页不要求物理连续，从而提高内存的使用率。

![image](http://www.wowotech.net/content/uploadfile/202003/bffb0713fc01271ddea4e109a018f0e920200308003812.png)

需要注意的是, 当前zsmalloc不支持LRU功能, 旧版本内核分配的不可移动的页, 对内存碎片影响严重, 但最新版本内核已经是支持分配可移动类型内存了。

### 3.2.2 Zbud
zbud是一个专门为存储压缩page而设计的内存分配器。用于将2个objects存到1个单独的page中。zbud是可以支持LRU的, 但分配的内存是不可移动的。

### 3.2.3 Z3fold
z3fold是一个较新的内存分配器, 与zbud不同的是, 将3个objects存到1个单独的page中,也就是zbud内存利用率极限是1:2, z3fold极限是1:3。同样z3fold是可以支持LRU的, 但分配的内存是不可移动的。

## 4. 内存压缩技术与内存分配器组合对比分析
结合上面zSwap / zRam /zCache的介绍, 与zsmalloc/zbud/z3fold分别怎样组合最合适呢?

下面总结了一下, 具体原因可以看上面介绍的时候各类型的特点。



- |                            | zsmalloc  | zbud    | z3fold  |
| -------------------------- | --------- | ------- | ------- |
| zSwap（有实际swap device） | ×(不可用) | √(可用) | √(最佳) |
| zSwap（无实际swap device） | √(最佳)   | √(可用) | √(可用) |
| zRam                       | √(最佳)   | √(可用) | √(可用) |
| zCache                     | ×(不可用) | √(可用) | √(最佳) |




## 5. zRAM技术原理

本文重点介绍zRam内存压缩技术，它是目前移动终端广泛使用的内存压缩技术。

### 5.1 软件框架
下图展示了内存管理大体的框架， 内存压缩技术处于内存回收memory reclaim部分中。

![image](http://www.wowotech.net/content/uploadfile/202003/ebba4eccd9eb6d90a49573d80724f26b20200308003813.png)


再具体到zRam, 它的软件架构可以分为3部分， 分别是数据流操作，内存压缩算法 ，zram驱动。

![image](http://www.wowotech.net/content/uploadfile/202003/b085b16f660d3bf0e9b640e276d01ac720200308003814.png)


数据流操作:提供串行或者并行的压缩和解压操作。

内存压缩算法：每种压缩算法提供压缩和解压缩的具体实现回调接口供数据操作调用。

Zram驱动：创建一个基于ram的块设备， 并提供IO请求处理接口。

### 5.2 实现原理
Zram内存压缩技术本质上就是以时间换空间。通过CPU压缩、解压缩的开销换取更大的可用内存空间。

我们主要描述清楚下面这2个问题：

1） 什么时候会进行内存压缩？

2） 进行内存压缩/解压缩的流程是怎样的？

进行内存压缩的时机：

1） Kswapd场景：kswapd是内核内存回收线程， 当内存watermark低于low水线时会被唤醒工作， 其到内存watermark不小于high水线。

2） Direct reclaim场景：内存分配过程进入slowpath, 进行直接行内存回收。

![image](http://www.wowotech.net/content/uploadfile/202003/564cfb76a4daa43f2f496992ef5adb2b20200308003815.png)


下面是基于4.4内核理出的内存压缩、解压缩流程。

内存回收过程路径进行内存压缩。会将非活跃链表的页进行shrink, 如果是匿名页会进行pageout, 由此进行内存压缩存放到ZRAM中， 调用路径如下：

![image](http://www.wowotech.net/content/uploadfile/202003/3f273cc2792c82a95546d360bd36db3b20200308003816.png)


### 5.3 内存压缩算法
目前比较主流的内存算法主要为LZ0, LZ4, ZSTD等。下面截取了几种算法在x86机器上的表现。各算法有各自特点， 有以压缩率高的， 有压缩/解压快的等， 具体要结合需求场景选择使用。

![image](http://www.wowotech.net/content/uploadfile/202003/d6e53e3fe37da3c00a4a16cfcaa1922620200308003818.jpg)


## 6. zRAM技术应用
本节描述一下在使用ZRAM常遇到的一些使用或配置，调试的方法。

### 6.1 如何配置开启zRAM
1） 配置内存压缩算法

下面例子配置压缩算法为lz4

echo lz4 > /sys/block/zram0/comp_algorithm

2） 配置ZRAM大小

下面例子配置zram大小为2GB

echo 2147483648 > /sys/block/zram0/disksize

3） 使能zram

mkswap /dev/zram0

swapon /dev/zram0

### 6.2 swappiness含义简述

swappiness参数是内核倾向于回收匿名页到swap（使用的ZRAM就是swap设备）的积极程度， 原生内核范围是0~100， 参数值越大， 表示回收匿名页到swap的比例就越大。如果配置为0， 表示仅回收文件页，不回收匿名页。默认值为60。可以通过节点“/proc/sys/vm/swappiness”配置。

### 6.3 zRam相关的技术指标

1） ZRAM大小及剩余空间

Proc/meminfo中可以查看相关信息

SwapTotal：swap总大小, 如果配置为ZRAM, 这里就是ZRAM总大小

SwapFree：swap剩余大小, 如果配置为ZRAM, 这里就是ZRAM剩余大小

当然， 节点 /sys/block/zram0/disksize是最直接的。

2） ZRAM压缩率

/sys/block/zram/mm_stat中有压缩前后的大小数据， 由此可以计算出实际的压缩率

orig_data_size：压缩前数据大小， 单位为bytes

compr_data_size ：压缩后数据大小， 单位为bytes

3） 换出/换入swap区的总量, proc/vmstat中中有相关信息

pswpin:换入总量， 单位为page

pswout:换出总量， 单位为page

### 6.4 zRam相关优化
上面提到zRam的一些缺陷, 怎么去改善呢?

1) zRam大小是可灵活配置的, 那是不是配置越大越好呢? 如果不是配置多大是最合适的呢?

zRam大小的配置比较灵活, 如果zRam配置过大, 后台缓存了应用过多, 这也是有可能会影响前台应用使用的流畅度。另外, zRam配置越大, 也需要关注系统的内存碎片化情。因此zRam并不是配置越大越好,具体的大小需要根据内存总大小及系统负载情况考虑及实测而定。

2) 使用zRam,可能会存在低内存场景由于频繁的内存压缩导致kswapd进程占CPU高, 怎样改善?

zRam本质就是以时间换空间, 在低内存的情况下, 肯定会比较频繁地回收内存, 这时kswapd进程是比较活跃的, 再加上通过压缩内存, 会更加消耗CPU资源。 改善这种情况方法也比较多, 比如, 可以使用更优的压缩算法, 区别使用场景, 后台不影响用户使用的场景异步进行深度内存压缩, 与用户体验相关的场景同步适当减少内存压缩, 通过增加文件页的回收比例加快内存回收等等。

3) 增大了zRam配置,对系统内存碎片是否有影响?

使用zRam是有可能导致系统内存碎片变得更严重的, 特别是zsmalloc分配不支持可移动内存类型的时候。新版的内核zsmalloc已经支持可移动类型分配的， 但由于增大了zRam,结合android手机的使用特点, 仍然会有可能导致系统内存碎片较严重的情况,因些内存碎片问题也是需要重点关注的。解决系统内存碎片的方法也比较多, 可以结合具体的原因及场景进行优化。