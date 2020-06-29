1. slab分配器是如何分配和释放小内存块的？

   slab分配小内存块时用kmem_cache_alloc，从函数的名称、参数、返回值、注释中，我们很容易知道kmem_cache_alloc()函数从给定的slab高速缓存中获取一个指向空闲对象的指针。实际上，进行获取空闲对象的时候，会先从per-CPU缓存中也就是array_cache中查找空闲对象，如果没有则会从kmem_cache_node中获取空闲对象，如果也没有则需要利用伙伴算法分配新的连续页框，然后从新的页框中获取空闲对象。从kmem_cache_alloc()到大致的调用链如下：

   ```
   kmem_cache_alloc()——>slab_alloc()——>__do_cache_alloc()——>____cache_alloc()——>cpu_cache_get()（这里实际上是从array_cache中获取空闲对象）——>cache_alloc_refill()（这里会在array_cache中没有空闲对象时执行）——>cpu_cache_get()（经过cache_alloc_refill()的执行基本保证array_cache中有空闲对象）——>返回可用空闲对象
   ```

   对cache_alloc_refill()函数执行步骤分解如下：

   ```
   cache_alloc_refill()——>尝试从被一个NUMA节点所有CPU共享的缓冲中获取空闲对象（源代码注释中写道：See if we can refill from the shared array），如果有则返回可用对象，refill结束——>从kmem_cache_node中的slab中获取空闲对象，有则返回，没有就执行下一步——>kmem_getpages()
   ```

   释放小内存块的具体看这篇文章：

   [Linux内存管理 (5)slab分配器](https://www.cnblogs.com/arnoldlu/p/8215414.html)

2. slab有一个着色的概念，着色有什么作用？

同一硬件高速缓存行可以映射RAM中很多不同的内存块。相同大小的对象倾向于存放在硬件高速缓存内相同的偏移量处。在不同的SLAB内具有相同偏移量的度下行最终很有可能映射在同一硬件高速缓存行中。高速缓存的硬件可能因此而花费内存周期在同一高速缓存行与RAM内存单元之间来来往往传送这两个对象，而其他的硬件高速缓存行并未充分使用（以上语句出自《深入理解Linux内核》第三版第334页）。SLAB分配器为了降低硬件高速缓存的这种行为，采用了SLAB着色（slab  coloring）的策略。所谓着色，简单来说就是给各个slab增加不同的偏移量，设置偏移量的过程就是着色的过程。通过着色尽量使得不同的对象对应到硬件不同的高速缓存行上，以最大限度的利用硬件高速缓存，提升系统效率。

3. slab分配器中的slab对象有没有根据Per-CPU做一些优化？

   提前分配好一些资源放在一个per-cpu cache中。这样做可以减少不同CPU之间对锁的竞争，也可以减少对slab中各种链表的操作。

4. slab增加并导致大量不用的空闲对象，该如何解决？

    Linux内核中将对象释放到slab中上层所用函数为kfree()或kmem_cache_free()。两个函数都会调用__cache_free()函数。

   1. 当本地CPU cache中空闲对象数小于规定上限时，只需将对象放入本地CPU cache中；

   2. 当本地 cache中对象过多（大于等于规定上限），需要释放一批对象到slab三链中。由函数cache_flusharray()实现。

      1）如果三链中存在共享本地cache，那么首先选择释放到共享本地cache中，能释放多少是多少；

      2）如果没有shared local cache，释放对象到slab三链中，实现函数为free_block()。对于free_block()函数，当三链中的空闲对象数过多时，销毁此cache。不然，添加此slab到空闲链表。因为在分配的时候我们看到将slab结构从cache链表中脱离了，在这里，根据page描述符的lru找到slab并将它添加到三链的空闲链表中。
      