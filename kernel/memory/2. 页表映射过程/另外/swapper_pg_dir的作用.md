在内存系统初始化过程中，有如下代码：


```
   1 static void __init pagetable_init(void)
   2 {
   3     pgd_t pgd_base = swapper_pg_dir;
   4  
   5     permanent_kmaps_init(pgd_base);
   6 }
```
这里，我们看到了神秘的swapper_pg_dir，全局搜索一下，发现了

```
   1 
   2   Build a proper pagetable for the kernel mappings.  Up until this
   3   point, we've been running on some set of pagetables constructed by
   4   the boot process.
   5  
   6   If we're booting on native hardware, this will be a pagetable
   7   constructed in archx86kernelhead_32.S.  The root of the
   8   pagetable will be swapper_pg_dir.
   9  
  10   If we're booting paravirtualized under a hypervisor, then there are
  11   more options we may already be running PAE, and the pagetable may
  12   or may not be based in swapper_pg_dir.  In any case,
  13   paravirt_pagetable_setup_start() will set up swapper_pg_dir
  14   appropriately for the rest of the initialization to work.
  15  
  16   In general, pagetable_init() assumes that the pagetable may already
  17   be partially populated, and so it avoids stomping on any existing
  18   mappings.
  19  
  20 void __init early_ioremap_page_table_range_init(void)
  21 {
  22     pgd_t pgd_base = swapper_pg_dir;
  23     unsigned long vaddr, end;
  24  
  25     
  26       Fixed mappings, only the page table structure has to be
  27       created - mappings will be set by set_fixmap()
  28      
  29     vaddr = __fix_to_virt(__end_of_fixed_addresses - 1) & PMD_MASK;
  30     end = (FIXADDR_TOP + PMD_SIZE - 1) & PMD_MASK;
  31     page_table_range_init(vaddr, end, pgd_base);
  32     early_ioremap_reset();
  33 }
```

在head_32.S中，定义了如下的BSS段，BSS段是在内核映像文件中不占空间，但是在内核被加载到内存时，会保留相应的空间。

在BSS段，一共保留了4个页面的空间，分别用initial_page_table, initial_pg_fixmap, empty_zero_page和swapper_pg_dir来标志其地址。


```
   1 
   2   BSS section
   3  
   4 __PAGE_ALIGNED_BSS
   5     .align PAGE_SIZE
   6 #ifdef CONFIG_X86_PAE
   7 initial_pg_pmd
   8     .fill 1024KPMDS,4,0
   9 #else
  10 ENTRY(initial_page_table)
  11     .fill 1024,4,0
  12 #endif
  13 initial_pg_fixmap
  14     .fill 1024,4,0
  15 ENTRY(empty_zero_page)
  16     .fill 4096,1,0
  17 ENTRY(swapper_pg_dir)
  18     .fill 1024,4,0
```
通过如下代码，将initial_page_table设置为初始页目录


```
   1 
   2   Enable paging
   3  
   4     movl $pa(initial_page_table), %eax
   5     movl %eax,%cr3         set the page table pointer.. 
   6     movl %cr0,%eax
   7     orl  $X86_CR0_PG,%eax
   8     movl %eax,%cr0         ..and set paging (PG) bit 
   9     ljmp $__BOOT_CS,$1f     Clear prefetch and normalize %eip 
```
在内核初始化阶段，setup_arch调用了如下的函数：


```
   1 void __init setup_arch(char cmdline_p)
   2 {
   3  
   4 ......
   5  max_pfn_mapped is updated here 
   6 max_low_pfn_mapped = init_memory_mapping(0, max_low_pfnPAGE_SHIFT);
   7 max_pfn_mapped = max_low_pfn_mapped;
   8 ......
   9 x86_init.paging.pagetable_setup_start(swapper_pg_dir);
  10 paging_init();
  11 x86_init.paging.pagetable_setup_done(swapper_pg_dir);
  12  
  13 ......
  14 }
```
init_memory_mapping调用了kernel_physical_mapping_init，初始化swapper_pg_dir


```
  1 
   2   This maps the physical memory to kernel virtual address space, a total
   3   of max_low_pfn pages, by creating page tables starting from address
   4   PAGE_OFFSET
   5  
   6 unsigned long __init
   7 kernel_physical_mapping_init(unsigned long start,
   8                  unsigned long end,
   9                  unsigned long page_size_mask)
  10 {
  11     int use_pse = page_size_mask == (1PG_LEVEL_2M);
  12     unsigned long last_map_addr = end;
  13     unsigned long start_pfn, end_pfn;
  14     pgd_t pgd_base = swapper_pg_dir;
  15     int pgd_idx, pmd_idx, pte_ofs;
  16     unsigned long pfn;
  17     pgd_t pgd;
  18     pmd_t pmd;
  19     pte_t pte;
  20     unsigned pages_2m, pages_4k;
  21     int mapping_iter;
  22     
  23     start_pfn = start  PAGE_SHIFT;
  24     end_pfn = end  PAGE_SHIFT;
  25     
  26     
  27       First iteration will setup identity mapping using largesmall pages
  28       based on use_pse, with other attributes same as set by
  29       the early code in head_32.S
  30      
  31       Second iteration will setup the appropriate attributes (NX, GLOBAL..)
  32       as desired for the kernel identity mapping.
  33      
  34       This two pass mechanism conforms to the TLB app note which says
  35      
  36           Software should not write to a paging-structure entry in a way
  37            that would change, for any linear address, both the page size
  38            and either the page frame or attributes.
  39      
  40     mapping_iter = 1;
  41     
  42     if (!cpu_has_pse)
  43         use_pse = 0;
  44     
  45     at
  46     pages_2m = pages_4k = 0;
  47     pfn = start_pfn;
  48     pgd_idx = pgd_index((pfnPAGE_SHIFT) + PAGE_OFFSET);
  49     pgd = pgd_base + pgd_idx;
  50     for (; pgd_idx  PTRS_PER_PGD; pgd++, pgd_idx++) {
  51         pmd = one_md_table_init(pgd);
  52     
  53         if (pfn = end_pfn)
  54             continue;
  55     ef CONFIG_X86_PAE
  56         pmd_idx = pmd_index((pfnPAGE_SHIFT) + PAGE_OFFSET);
  57         pmd += pmd_idx;
  58     e
  59         pmd_idx = 0;
  60     if
  61         for (; pmd_idx  PTRS_PER_PMD && pfn  end_pfn;
  62              pmd++, pmd_idx++) {
  63             unsigned int addr = pfn  PAGE_SIZE + PAGE_OFFSET;
  64     
  65             
  66               Map with big pages if possible, otherwise
  67               create normal page tables
  68              
  69             if (use_pse) {
  70                 unsigned int addr2;
  71                 pgprot_t prot = PAGE_KERNEL_LARGE;
  72                 
  73                   first pass will use the same initial
  74                   identity mapping attribute + _PAGE_PSE.
  75                  
  76                 pgprot_t init_prot =
  77                     __pgprot(PTE_IDENT_ATTR 
  78                          _PAGE_PSE);
  79     
  80                 addr2 = (pfn + PTRS_PER_PTE-1)  PAGE_SIZE +
  81                     PAGE_OFFSET + PAGE_SIZE-1;
  82     
  83                 if (is_kernel_text(addr) 
  84                     is_kernel_text(addr2))
  85                     prot = PAGE_KERNEL_LARGE_EXEC;
  86     
  87                 pages_2m++;
  88                 if (mapping_iter == 1)
  89                     set_pmd(pmd, pfn_pmd(pfn, init_prot));
  90                 else
  91                     set_pmd(pmd, pfn_pmd(pfn, prot));
  92     
  93                 pfn += PTRS_PER_PTE;
  94                 continue;
  95             }
  96             pte = one_page_table_init(pmd);
  97     
  98             pte_ofs = pte_index((pfnPAGE_SHIFT) + PAGE_OFFSET);
  99             pte += pte_ofs;
 100             for (; pte_ofs  PTRS_PER_PTE && pfn  end_pfn;
 101                  pte++, pfn++, pte_ofs++, addr += PAGE_SIZE) {
 102                 pgprot_t prot = PAGE_KERNEL;
 103                 
 104                   first pass will use the same initial
 105                   identity mapping attribute.
 106                  
 107                 pgprot_t init_prot = __pgprot(PTE_IDENT_ATTR);
 108     
 109                 if (is_kernel_text(addr))
 110                     prot = PAGE_KERNEL_EXEC;
 111     
 112                 pages_4k++;
 113                 if (mapping_iter == 1) {
 114                     set_pte(pte, pfn_pte(pfn, init_prot));
 115                     last_map_addr = (pfn  PAGE_SHIFT) + PAGE_SIZE;
 116                 } else
 117                     set_pte(pte, pfn_pte(pfn, prot));
 118             }
 119         }
 120     }
 121     if (mapping_iter == 1) {
 122         
 123           update direct mapping page count only in the first
 124           iteration.
 125          
 126         update_page_count(PG_LEVEL_2M, pages_2m);
 127         update_page_count(PG_LEVEL_4K, pages_4k);
 128     
 129         
 130           local global flush tlb, which will flush the previous
 131           mappings present in both small and large page TLB's.
 132          
 133         __flush_tlb_all();
 134     
 135         
 136           Second iteration will set the actual desired PTE attributes.
 137          
 138         mapping_iter = 2;
 139         goto repeat;
 140     }
 141     return last_map_addr;
 142 }
```
pgd_t pgd_base = swapper_pg_dir;

将swapper_pg_dir作为页目录地址，赋给pgd_base

  start_pfn = start  PAGE_SHIFT; br
    end_pfn = end  PAGE_SHIFT;


   pgd_idx = pgd_index((pfnPAGE_SHIFT) + PAGE_OFFSET); br
    pgd = pgd_base + pgd_idx;

pgd_idx,pgd代表着在页目录中的索引，以及相应的页目录项


```
  1 pgprot_t prot = PAGE_KERNEL;
   2 
   3   first pass will use the same initial
   4   identity mapping attribute.
   5  
   6 pgprot_t init_prot = __pgprot(PTE_IDENT_ATTR);
   7  
   8 if (is_kernel_text(addr))
   9     prot = PAGE_KERNEL_EXEC;
  10  
  11 pages_4k++;
  12 if (mapping_iter == 1) {
  13     set_pte(pte, pfn_pte(pfn, init_prot));
  14     last_map_addr = (pfn  PAGE_SHIFT) + PAGE_SIZE;
  15 } else
  16     set_pte(pte, pfn_pte(pfn, prot));
```
最后，通过两个回合的遍历，将属性设置到对应的页表项上去。

swapper_pg_dir涉及到很多内容，主要是用来设置内核系统页目录。
