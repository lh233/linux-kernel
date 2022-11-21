内存管理错综复杂，不仅要从用户态的相关API来窥探和理解Linux内核内存是如何运作，还要总结 Linux内核中常用的内存管理相关的API。前文中已经总结了内存管理相关的数据结构的关系，下面总结内存管理中内核常用的API。

内存管理错综复杂，不仅要从用户态的相关API来窥探和理解Linux内核内存是如何运作，还要总结 Linux内核中常用的内存管理相关的API。前文中已经总结了内存管理相关的数据结构的关系，下面总结内存管理中内核常用的API。

## 1．页表相关

页表相关的API可以概括为如下4类。

-   查询页表。
-   判断页表项的状态位。
-   修改页表。
-   page和pfn的关系。

```
//查询页表
#define pgd_offset_k(addr) pgd_offset(&init_mm,addr)
#define pgd_index(addr) ((addr) >>PGDIR_SHIFT)
#define pgd_offset (mm，addr) ( (mm ) ->pgd + pgd_index(addr ) )
#define pte_index(addr)  (((addr) >> PAGE_SHIFT)& (PTRS_PER_PTE -1))
#define pte_offset_kernel(pmd, addr)(pmd_page_vaddr(*(pmd)) + pte_index(addr))
#define pte_offset_map(pmd, addr)(_pte_map(pmd)+ pte_index(addr) )#define pte_unmap (pte)
pte_unmap(pte)
#define pte_offset_map_lock (mm，pmd,address, ptlp)
#define pte_offset_map(pmd, addr)(_pte_map(pmd)+ pte_index(addr) )
#define pte_unmap(pte)  pte_unmap(pte)
#define pte_offset_map_lock (mm，pmd,address, ptlp)

//判断页表项的状态位
#define pte_none(pte)(!pte_val(pte))
#define pte_present(pte)(pte_isset((pte),L_PTE_PRESENT))
#define pte_valid(pte)(pte_isset((pte),L_PTE_VALID))
#define pte_accessible(mm,pte)(mm_tlb_flush_pending(mm) ? pte_present (pte) : pte_valid(pte))
#define pte_write(pte)(pte_isclear ( (pte),L_PTE_RDONLY) )
#define pte_dirty(pte)(pte_isset((pte), L_PTE_DIRTY))
#define pte_young(pte)(pte_isset((pte), L_PTE_YOUNG))
#define pte_exec(pte)(pte_isclear((pte), L_PTE_XN))
//修改页表
#define mk_pte(page , prot) pfn_pte(page_to_pfn(page), prot)
static inline pte_t pte_mkdirty(pte_t pte)
static inline pte_t pte_mkold(pte_t pte)
static inline pte_t pte_mkclean(pte_t pte)
static inline pte_t pte_mkwrite(pte_t pte)
static inline pte_t pte_wrprotect (pte_t pte)
static inline pte_t pte_mkyoung(pte_t pte)
static inline void set pte_at(struct mm_struct *mm，unsigned long addr, pte_t *ptep, pte_t pteval)
int ptep_set_access_flags (struct vm_area_struct *vma, unsigned long address, pte_t *ptep,pte_t entry, int dirty)
//page和pn的关系
#define pte_pfn(pte)((pte_val(pte)& PHYS_MASK) >> PAGE_SHIFT)
#define pfn_pte(pfn , prot) __pte(_pfn_to_phys(pfn) | pgprot_val(prot))
```

## 2. 内存分配

内核中常用的内存分配API如下:

```
static inline struct page * alloc_pages(gfp_t gfp_mask,unsigned int order)
unsigned long _get_free_pages(gfp_t gfp_mask,unsigned int order)
struct page* __alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order, struct zonelist *zonelist, nodemask_t *nodemask)
void free_pages(unsigned long addr,，unsigned int order)
void _free_pages(struct page *page，unsigned int order)
//slab分配器
struct kmem_cache* kmem_cache_create(const char *name，size_t size,size_t align, unsigned long flags, void (*ctor) (void *))
void kmem_cache_destroy(struct kmem_cache *s)
void *kmem_cache_alloc(struct kmem_cache *cachep,gfp_t flags)
void kmem_cache_free(struct kmem_cache *cachep,void *objp)
static void *kmalloc(size_t size, gfp_t flags)
void kfree(const void*objp)
//vmalloc相关
void*vmalloc(unsigned long size)
void vfree (const void *addr)
```

## 3. VMA操作相关

```
struct vm_area_struct * find_vma(struct mm_struct * mm，unsigned long addr);
struct vm_ area_struct * find_vma prev(struct mm_struct * mm，unsigned long addr,struct vm_area_struct**pprev) ;
struct vm_area_struct * find_vma_intersection(struct mm_struct * mm，unsignedlong start_addr,unsigned long end_addr) ;
static int find_vma_links(struct mm_struct *mm，unsigned long addr,unsigned long end,struct vm_area_struct**pprev ,struct rb_node ***rb_link, struct rb_node **rb_parent) ;
int insert_vm_struct(struct mm_struct*mm，struct vm_area_struct *vma)
```

## 4．页面相关

内存管理的复杂之处是和页面相关的操作，内核中常用的API函数归纳如下。

-   PG_XXX标志位操作。
-   page引用计数操作。
-   匿名页面和KSM页面。
-   页面操作。
-   页面映射。
-   缺页中断。
-   LRU和页面回收。

```
//PG_xxx标志位操作
PageXXX()
SetPagexXX()
clearPageXXX()
TestsetPageXXX()
TestclearPageXXX()
void lock_page(struct page *page)
int trylock_page(struct page *page)
void wait_on_page_bit(struct page *page,int bit_nr) ;
void wake_up_page(struct page. *page, int bit)
static inline void wait_on_page_locked(struct page *page)
static inline void wait_on_page_writeback(struct page *page)

//page引用计数操作
void get_page(struct page *page)
void put_page(struct page *page);
#define page_cache_get(page) get_page(page)
#define page_cache_release(page) put_page(page)
static inline int page_count(struct page *page)
static inline int page_mapcount(struct page *page)
static inline int page_mapped(struct page *page)
static inline int put _page_testzero(struct page *page)

//匿名页面和KSM页面
static inline int PageAnon(struct page *page)
static inline int PageKsm (struct page *page)
struct address_space *page_mapping(struct page *page)
void page_add_new_anon_rmap(struct page *page,struct vm_area_struct*vma,unsigned long address)

//页面操作
struct page *follow_page(struct vm_area_struct *vma,unsigned long address,unsigned int foll_flags)
struct page *vm_normal_page(struct vm_area_struct *vma，unsigned long addr,
pte_t pte)
long get_user_pages(struct task_struct *tsk,struct mm_struct *mm,unsigned long start,unsigned long nr_pages, int write, int force，struct page **pages，struct vm_area_struct-**vmas)

//页面映射
void create_mapping_late(phys_addr_t phys,unsigned long virt,phys_addr_t size, pgprot_t prot)
unsigned long do_mmap_pgoff(struct file *file,unsigned long addr,unsigned long len, unsigned long prot,unsigned long flags, unsigned long pgoff，unsigned long *populate)
int remap_pfn_range(struct vm_area_struct *vma，unsigned long addr,unsigned long pfn，unsigned long size，pgprot_t prot)

//缺页中断
int do_page_fault(unsigned long addr，unsigned int fsr,struct pt_regs *regs)
int handle_pte_fault(struct mm_struct*mm, struct vm_area_struct *vma，unsigned long address,pte_t *pte, pmd_t *pmd, unsigned int flags)
static int do_anonymous_page(struct mm_struct *mm，struct vm_area_struct *vma,unsigned long address, pte_t *page_table,pmd_t *pmd ,unsigned int flags)
static int do_wp_page(struct mm_struct *mm，struct vm_area_struct *vma ,unsigned long address, pte_t *page_table,pmd_t *pmd,spinlock_t *ptl, pte_t orig pte)
static int do_wp_page(struct mm_struct *mm,struct vm_area_struct*vma ,unsigned long address, pte_t *page_table，pmd_t *pmd, spinlock_t *ptl, pte_t orig pte)

//LRU和页面回收
void lru_cache_add( struct page *page)
#define lru_to_page(_head) (list_entry((_head) ->prev,struct page，lru))
bool zone_watermark_ok(struct zone *z，unsigned int order，unsigned long mark,int classzone_idx, int alloc_flags)
```

