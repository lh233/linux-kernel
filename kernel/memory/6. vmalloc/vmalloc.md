kmalloc、vmalloc和malloc这三个常用的API函数具有相当的分量，三者看上去很相似，但在实现上大有讲究。kmalloc基于slab分配器，slab缓冲区建立在一个连续的物理地址的大块内存之上，所以缓冲对象也是物理地址连续的。如果在内核中不需要连续的物理地址，而仅仅需要内核空间里连续的虚拟地址的内存块，该如何处理呢？这时vmalloc()就派上用场了。

vmalloc()函数声明如下：

```
[mm/vmalloc.c]
/**
 *	vmalloc  -  allocate virtually contiguous memory
 *	@size:		allocation size
 *	Allocate enough pages to cover @size from the page level
 *	allocator and map them into contiguous kernel virtual space.
 *
 *	For tight control over page level allocator and protection flags
 *	use __vmalloc() instead.
 */
void *vmalloc(unsigned long size)
{
	return __vmalloc_node_flags(size, NUMA_NO_NODE,
				    GFP_KERNEL | __GFP_HIGHMEM);
}
```

vmalloc使用的分配掩码是“GFP_KERNEL|__GFP_HIGHMEM”，说明会优先使用高端内存High Memory。

```
static void *__vmalloc_node(unsigned long size, unsigned long align,
			    gfp_t gfp_mask, pgprot_t prot,
			    int node, const void *caller)
{
	return __vmalloc_node_range(size, align, VMALLOC_START, VMALLOC_END,
				gfp_mask, prot, 0, node, caller);
}
```

这里的VMALLOC_START和VMALLOC_END是vmalloc中最重要的宏，这两个宏定义在arch/arm/include/pgtable.h头文件中。ARM64架构定义在arch/arm64/include/asm/pgtable.h头文件中。VMALLOC_START是vmalloc区域的开始地址，它是在High_memory指定的高端内存开始地址再加上8MB大小的安全区域（VMALLOC_OFFSET）。在ARM Vexpress平台杀昂，vmalloc的内存范围是从0xf000_000到0xff00_0000，大小为240MB，high_memory全局变量的计算在sanity_check_meminfo()函数中。

```
[arch/arm/include/pgtable.h]
#define VMALLOC_OFFSET		(8*1024*1024)
#define VMALLOC_START		(((unsigned long)high_memory + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1))
#define VMALLOC_END		0xff000000UL
[vmalloc()-> __vmalloc_node() -> __vmalloc_node_range()]
void *__vmalloc_node_range(unsigned long size, unsigned long align,
			unsigned long start, unsigned long end, gfp_t gfp_mask,
			pgprot_t prot, unsigned long vm_flags, int node,
			const void *caller)
{
	struct vm_struct *area;
	void *addr;
	unsigned long real_size = size;

	size = PAGE_ALIGN(size);
	if (!size || (size >> PAGE_SHIFT) > totalram_pages)
		goto fail;

	area = __get_vm_area_node(size, align, VM_ALLOC | VM_UNINITIALIZED |
				vm_flags, start, end, node, gfp_mask, caller);
	if (!area)
		goto fail;

	addr = __vmalloc_area_node(area, gfp_mask, prot, node);
	if (!addr)
		return NULL;

	/*
	 * In this function, newly allocated vm_struct has VM_UNINITIALIZED
	 * flag. It means that vm_struct is not fully initialized.
	 * Now, it is fully initialized, so remove this flag here.
	 */
	clear_vm_uninitialized_flag(area);

	/*
	 * A ref_count = 2 is needed because vm_struct allocated in
	 * __get_vm_area_node() contains a reference to the virtual address of
	 * the vmalloc'ed block.
	 */
	kmemleak_alloc(addr, real_size, 2, gfp_mask);

	return addr;

fail:
	warn_alloc_failed(gfp_mask, 0,
			  "vmalloc: allocation failure: %lu bytes\n",
			  real_size);
	return NULL;
}
```

在__vmalloc_node_range()函数中，第9行代码vmalloc分配的大小要以页面大小对齐。如果vmalloc要分配的大小为10Byte，那么vmalloc还是会分配出一个页，剩下的4086Byte就浪费了。

第10行代码，判断要分配的内存大小不能为0或者不能大于系统的所有内存。

```
[vmalloc->__vmalloc_node_range()->__get_vm_area_node()]
static struct vm_struct *__get_vm_area_node(unsigned long size,
		unsigned long align, unsigned long flags, unsigned long start,
		unsigned long end, int node, gfp_t gfp_mask, const void *caller)
{
	struct vmap_area *va;
	struct vm_struct *area;

	BUG_ON(in_interrupt());
	if (flags & VM_IOREMAP)
		align = 1ul << clamp(fls(size), PAGE_SHIFT, IOREMAP_MAX_ORDER);

	size = PAGE_ALIGN(size);
	if (unlikely(!size))
		return NULL;

	area = kzalloc_node(sizeof(*area), gfp_mask & GFP_RECLAIM_MASK, node);
	if (unlikely(!area))
		return NULL;

	if (!(flags & VM_NO_GUARD))
		size += PAGE_SIZE;

	va = alloc_vmap_area(size, align, start, end, node, gfp_mask);
	if (IS_ERR(va)) {
		kfree(area);
		return NULL;
	}

	setup_vmalloc_vm(area, va, flags, caller);

	return area;
}
```

在__get_vm_area_node()函数中，第7行代码确保当前不在中断上下文中，因为这个函数有可能睡眠。

第8行代码又计算了一次对齐。

第10行代码分配了一个struct vm_struct数据结构来描述这个vmalloc区域。

第12行代码，如果flags中没有定义VM_NO_GUARD标志位，那么要多分配一个页来做安全垫，例如我们要分配4KB的大小内存，vmalloc分配了8KB的内存块。

下面重点要看下第15行代码的alloc_vmap_area()函数。

```
/*
 * Allocate a region of KVA of the specified size and alignment, within the
 * vstart and vend.
 */
static struct vmap_area *alloc_vmap_area(unsigned long size,
				unsigned long align,
				unsigned long vstart, unsigned long vend,
				int node, gfp_t gfp_mask)
{
	struct vmap_area *va;
	struct rb_node *n;
	unsigned long addr;
	int purged = 0;
	struct vmap_area *first;

	BUG_ON(!size);
	BUG_ON(size & ~PAGE_MASK);
	BUG_ON(!is_power_of_2(align));

	va = kmalloc_node(sizeof(struct vmap_area),
			gfp_mask & GFP_RECLAIM_MASK, node);
	if (unlikely(!va))
		return ERR_PTR(-ENOMEM);

	/*
	 * Only scan the relevant parts containing pointers to other objects
	 * to avoid false negatives.
	 */
	kmemleak_scan_area(&va->rb_node, SIZE_MAX, gfp_mask & GFP_RECLAIM_MASK);

retry:
	spin_lock(&vmap_area_lock);
	/*
	 * Invalidate cache if we have more permissive parameters.
	 * cached_hole_size notes the largest hole noticed _below_
	 * the vmap_area cached in free_vmap_cache: if size fits
	 * into that hole, we want to scan from vstart to reuse
	 * the hole instead of allocating above free_vmap_cache.
	 * Note that __free_vmap_area may update free_vmap_cache
	 * without updating cached_hole_size or cached_align.
	 */
	if (!free_vmap_cache ||
			size < cached_hole_size ||
			vstart < cached_vstart ||
			align < cached_align) {
nocache:
		cached_hole_size = 0;
		free_vmap_cache = NULL;
	}
	/* record if we encounter less permissive parameters */
	cached_vstart = vstart;
	cached_align = align;

	/* find starting point for our search */
	if (free_vmap_cache) {
		first = rb_entry(free_vmap_cache, struct vmap_area, rb_node);
		addr = ALIGN(first->va_end, align);
		if (addr < vstart)
			goto nocache;
		if (addr + size < addr)
			goto overflow;

	} else {
		addr = ALIGN(vstart, align);
		if (addr + size < addr)
			goto overflow;

		n = vmap_area_root.rb_node;
		first = NULL;

		while (n) {
			struct vmap_area *tmp;
			tmp = rb_entry(n, struct vmap_area, rb_node);
			if (tmp->va_end >= addr) {
				first = tmp;
				if (tmp->va_start <= addr)
					break;
				n = n->rb_left;
			} else
				n = n->rb_right;
		}

		if (!first)
			goto found;
	}

	/* from the starting point, walk areas until a suitable hole is found */
	while (addr + size > first->va_start && addr + size <= vend) {
		if (addr + cached_hole_size < first->va_start)
			cached_hole_size = first->va_start - addr;
		addr = ALIGN(first->va_end, align);
		if (addr + size < addr)
			goto overflow;

		if (list_is_last(&first->list, &vmap_area_list))
			goto found;

		first = list_entry(first->list.next,
				struct vmap_area, list);
	}

found:
	if (addr + size > vend)
		goto overflow;

	va->va_start = addr;
	va->va_end = addr + size;
	va->flags = 0;
	__insert_vmap_area(va);
	free_vmap_cache = &va->rb_node;
	spin_unlock(&vmap_area_lock);

	BUG_ON(va->va_start & (align-1));
	BUG_ON(va->va_start < vstart);
	BUG_ON(va->va_end > vend);

	return va;

overflow:
	spin_unlock(&vmap_area_lock);
	if (!purged) {
		purge_vmap_area_lazy();
		purged = 1;
		goto retry;
	}
	if (printk_ratelimit())
		pr_warn("vmap allocation for size %lu failed: "
			"use vmalloc=<size> to increase size.\n", size);
	kfree(va);
	return ERR_PTR(-EBUSY);
}
```

alloc_vmap_area()在vmalloc整个空间中查找一块大小合适的并且没有人使用的空间，这段空间称为hole。注意这个参数vstart是指VMALLOC_START，vend是指VMALLOC_END。

第25行代码，free_vmap_cache、cached_hole_size和cached_vstart这几个变量是在几年前增加的一个优化选项中，核心思想是从上一次查找的结果中开始查找。这里假设暂时忽略free_vmap_cache这个优化，从47行代码开始看起。

查找的地址从VMALLOC_START开始，首先从vmap_area_root这颗红黑树上查找，这个红黑树里存放着系统中正在使用的vmalloc区块，遍历左子叶节点找区间地址最小区块。如果区块的开始地址等于VMALLOC_START，说明这区块是第一块vmalloc区块。如果红黑树没有一个节点，说明整个vmalloc区间都是空的，见第66行代码。

第54~64行代码，这里遍历的结果是返回起始地址最小vmalloc区块，这个区块有可能是VMALLOC_START开始的，也有可能不是。

然后从VMALLOC_START地址开始，查找每个已存在的vmalloc的区块的缝隙hole能否容纳目前要分配内存的大小。如果不能再已有vmalloc区块的缝隙中找到合适的hole，那么从最后一块vmalloc区块的结束地址开始一个新的vmalloc区域，见第71~83行代码。

第92行代码，找到新区块hole后，调用__insert_vmap_area()函数把这个hole注册到红黑树上。

```
static void __insert_vmap_area(struct vmap_area *va)
{
	struct rb_node **p = &vmap_area_root.rb_node;
	struct rb_node *parent = NULL;
	struct rb_node *tmp;

	while (*p) {
		struct vmap_area *tmp_va;

		parent = *p;
		tmp_va = rb_entry(parent, struct vmap_area, rb_node);
		if (va->va_start < tmp_va->va_end)
			p = &(*p)->rb_left;
		else if (va->va_end > tmp_va->va_start)
			p = &(*p)->rb_right;
		else
			BUG();
	}

	rb_link_node(&va->rb_node, parent, p);
	rb_insert_color(&va->rb_node, &vmap_area_root);

	/* address-sort this list */
	tmp = rb_prev(&va->rb_node);
	if (tmp) {
		struct vmap_area *prev;
		prev = rb_entry(tmp, struct vmap_area, rb_node);
		list_add_rcu(&va->list, &prev->list);
	} else
		list_add_rcu(&va->list, &vmap_area_list);
}
```

回到__get_vm_area_node()函数的第16行代码上，把刚找到的struct vmap_area *va的相关信息填到struct vm_struct *vm中。

```

static void setup_vmalloc_vm(struct vm_struct *vm, struct vmap_area *va,
			      unsigned long flags, const void *caller)
{
	spin_lock(&vmap_area_lock);
	vm->flags = flags;
	vm->addr = (void *)va->va_start;
	vm->size = va->va_end - va->va_start;
	vm->caller = caller;
	va->vm = vm;
	va->flags |= VM_VM_AREA;
	spin_unlock(&vmap_area_lock);
}
```

回到__vmalloc_node_range()函数中的第16行代码中的   __vmalloc_area_node()。

```
[vmalloc()->__vmalloc_node_range()->__vmalloc_area_node()]
static void *__vmalloc_area_node(struct vm_struct *area, gfp_t gfp_mask,
				 pgprot_t prot, int node)
{
	const int order = 0;
	struct page **pages;
	unsigned int nr_pages, array_size, i;
	const gfp_t nested_gfp = (gfp_mask & GFP_RECLAIM_MASK) | __GFP_ZERO;
	const gfp_t alloc_mask = gfp_mask | __GFP_NOWARN;

	nr_pages = get_vm_area_size(area) >> PAGE_SHIFT;
	array_size = (nr_pages * sizeof(struct page *));

	area->nr_pages = nr_pages;
	/* Please note that the recursion is strictly bounded. */
	if (array_size > PAGE_SIZE) {
		pages = __vmalloc_node(array_size, 1, nested_gfp|__GFP_HIGHMEM,
				PAGE_KERNEL, node, area->caller);
		area->flags |= VM_VPAGES;
	} else {
		pages = kmalloc_node(array_size, nested_gfp, node);
	}
	area->pages = pages;
	if (!area->pages) {
		remove_vm_area(area->addr);
		kfree(area);
		return NULL;
	}

	for (i = 0; i < area->nr_pages; i++) {
		struct page *page;

		if (node == NUMA_NO_NODE)
			page = alloc_page(alloc_mask);
		else
			page = alloc_pages_node(node, alloc_mask, order);

		if (unlikely(!page)) {
			/* Successfully allocated i pages, free them in __vunmap() */
			area->nr_pages = i;
			goto fail;
		}
		area->pages[i] = page;
		if (gfp_mask & __GFP_WAIT)
			cond_resched();
	}

	if (map_vm_area(area, prot, pages))
		goto fail;
	return area->addr;

fail:
	warn_alloc_failed(gfp_mask, order,
			  "vmalloc: allocation failure, allocated %ld of %ld bytes\n",
			  (area->nr_pages*PAGE_SIZE), area->size);
	vfree(area->addr);
	return NULL;
}
```

在__vmalloc_area_node()函数中，首先计算vmalloc分配内存大小有几个页面，然后使用alloc_page()这个API来分配物理页面，并且使用area->pages保存已分配的页面page数据结构指针，最后调用map_vm_area()函数来建立页面映射。

map_vm_area()函数最后调用vmap_page_range_noflush()来建立页面映射关系。

```
static int vmap_page_range_noflush(unsigned long start, unsigned long end,
				   pgprot_t prot, struct page **pages)
{
	pgd_t *pgd;
	unsigned long next;
	unsigned long addr = start;
	int err = 0;
	int nr = 0;

	BUG_ON(addr >= end);
	pgd = pgd_offset_k(addr);
	do {
		next = pgd_addr_end(addr, end);
		err = vmap_pud_range(pgd, addr, next, prot, pages, &nr);
		if (err)
			return err;
	} while (pgd++, addr = next, addr != end);

	return nr;
}
```

pgd_offset_k()首先从init_mm中获取指向PGD页面目录下的基地址，然后通过地址addr来找到对应的PGD表项。while循环里从开始地址addr到结束地址，按照PGDIR_SIZE的大小依次调用vmap_pud_range()来处理PGD页表。pgd_offset_k()宏定义如下：

```
#define pgd_index(addr)		((addr) >> PGDIR_SHIFT)
#define pgd_offset(mm, addr)	((mm)->pgd + pgd_index(addr))
#define pgd_offset_k(addr)		pgd_offset(&init_mm, addr)
#define pgd_addr_end(addr, end)
({		\
	unsigned long __boundary = ((addr) + PGDIR_SIZE) & PGDIR_MASK;	\
	(__boundary - 1 < (end) - 1) ? __boudary : (end);
}
)
```

vmap_pud_range()函数会依次调用vmap_pmd_range()。在ARM Vexpress平台中，页表是二级页表，所以PUD和PMD都指向PGD，最后直接调用vmap_pte_range()。

```
static int vmap_pte_range(pmd_t *pmd, unsigned long addr,
		unsigned long end, pgprot_t prot, struct page **pages, int *nr)
{
	pte_t *pte;

	/*
	 * nr is a running index into the array which helps higher level
	 * callers keep track of where we're up to.
	 */

	pte = pte_alloc_kernel(pmd, addr);
	if (!pte)
		return -ENOMEM;
	do {
		struct page *page = pages[*nr];

		if (WARN_ON(!pte_none(*pte)))
			return -EBUSY;
		if (WARN_ON(!page))
			return -ENOMEM;
		set_pte_at(&init_mm, addr, pte, mk_pte(page, prot));
		(*nr)++;
	} while (pte++, addr += PAGE_SIZE, addr != end);
	return 0;
}
```

在此场景中，对应的pmd页表项内容为空，即pmd_none(*(pmd))，所以需要新分配pte页表项。

```
static inline pte_t *
pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte = (pte_t *)__get_free_page(PGALLOC_GFP);
	if(pte)
		clean_pte_table(pte);
	return pte;
}
```

mk_pte()宏利用刚分配的page页面和页面属性prot来新生成一个PTE entry，最后通过set_pte_at()函数把PTE entry设置到硬件页表PTE页表项中。