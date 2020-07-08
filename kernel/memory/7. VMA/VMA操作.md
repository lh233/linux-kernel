在32位操作系统中，每个用户都可以拥有3GB大小的虚拟地址空间，通常要远大于物理内存，那么如何管理这些虚拟地址空间呢？用户进程通常会多次调用malloc()或使用mmap()接口映射文件到用户空间来进行读写等操作，这些操作都会要求在虚拟地址空间中分配内存块，这些内存块基本上都是来进行离散的。malloc()是用户态常用的分配内存的接口API函数，在第2.8节中将详细介绍其内核实现机制：mmap()是用户态蟾宫的用于建立文件映射和匿名映射的函数。在2.9节中将详细介绍其内核实现机制。这些进程地址空间在内核中使用struct vm_area_struct数据结构来描述，简称VMA。也被称为进程地址空间或进程线性区。由于这些地址空间归属各个用户进程，所以在用户进程的struct mm_struct数据结构中也有相应的成员，用于对这些VMA进行管理。

VMA数据结构定义在mm_types.h文件中。

```
[include/linux/mm_types.h]
struct vm_area_struct {
	/* The first cache line has the info for VMA tree walking. */

	unsigned long vm_start;		/* Our start address within vm_mm. */
	unsigned long vm_end;		/* The first byte after our end address
					   within vm_mm. */

	/* linked list of VM areas per task, sorted by address */
	struct vm_area_struct *vm_next, *vm_prev;

	struct rb_node vm_rb;

	/*
	 * Largest free memory gap in bytes to the left of this VMA.
	 * Either between this VMA and vma->vm_prev, or between one of the
	 * VMAs below us in the VMA rbtree and its ->vm_prev. This helps
	 * get_unmapped_area find a free area of the right size.
	 */
	unsigned long rb_subtree_gap;

	/* Second cache line starts here. */

	struct mm_struct *vm_mm;	/* The address space we belong to. */
	pgprot_t vm_page_prot;		/* Access permissions of this VMA. */
	unsigned long vm_flags;		/* Flags, see mm.h. */

	/*
	 * For areas with an address space and backing store,
	 * linkage into the address_space->i_mmap interval tree.
	 */
	struct {
		struct rb_node rb;
		unsigned long rb_subtree_last;
	} shared;

	/*
	 * A file's MAP_PRIVATE vma can be in both i_mmap tree and anon_vma
	 * list, after a COW of one of the file pages.	A MAP_SHARED vma
	 * can only be in the i_mmap tree.  An anonymous MAP_PRIVATE, stack
	 * or brk vma (with NULL file) can only be in an anon_vma list.
	 */
	struct list_head anon_vma_chain; /* Serialized by mmap_sem &
					  * page_table_lock */
	struct anon_vma *anon_vma;	/* Serialized by page_table_lock */

	/* Function pointers to deal with this struct. */
	const struct vm_operations_struct *vm_ops;

	/* Information about our backing store: */
	unsigned long vm_pgoff;		/* Offset (within vm_file) in PAGE_SIZE
					   units, *not* PAGE_CACHE_SIZE */
	struct file * vm_file;		/* File we map to (can be NULL). */
	void * vm_private_data;		/* was vm_pte (shared mem) */

#ifndef CONFIG_MMU
	struct vm_region *vm_region;	/* NOMMU mapping region */
#endif
#ifdef CONFIG_NUMA
	struct mempolicy *vm_policy;	/* NUMA policy for the VMA */
#endif
};
```

struct vm_area_struct 数据结构各个成员的含义如下。

- vm_start和vm_end：指定VMA在进程地址空间的起始地址和结束地址。
- vm_next和vm_prev：进程的VMA都连接成一个链表。
- vm_rb：VMA作为一个节点加入红黑树中，每个进程的struct mm_struct数据结构中都有这样的一颗红黑树mm->mm_rb。
- vm_mm：指向该VMA所属的进程struct mm_struct数据结构
- vm_page_prot：VMA的访问权限
- vm_flags：描述该VMA的一组标志位
- anon_vma_chain和anon_vma：用于管理RMAP反向映射。
- vm_ops：指向许多方法的集合，这些方法用于在VMA中执行各种操作，通常用于文件映射。
- vm_pgoff：指定文件映射的偏移量，这个变量的单位不是Byte，而是页面的大小（PAGE_SIZE）。对于匿名页面来说，它的值可以是0或者是vm_addr/PAGE_SIZE；
- vm_file：指向file的实例。描述一个被映射的文件。

struct mm_struct数据结构是描述进程内存管理的核心数据结构，该数据结构也提供了管理VMA所需要的信息，这些信息概况如下：

```
[include/linux/mm_types.h]
struct mm_struct {
	struct vm_area_struct *mmap;
	struct rb_root mm_rb;
	.....
}
```

每个VMA都要连接到mm_struct中的链表和红黑树中，以方便查找。

- mmap形成一个单链表，进程中所有VMA都链接到这个链表中，链表头mm_struct->mmap。
- mm_rb是红黑树的根节点，每个进程都有一颗VMA的红黑树。

VMA按照起始地址以递增的方式插入mm_struct->mmap链表中。当进程拥有大量VMA时，扫描链表和查找特定的VMA是非常低效的操作，例如在云计算的机器中，内核中通常要靠红黑树来协助，以便提高查找速度。