## kprobe是什么？

kprobe 是一种动态调试机制，用于debugging，动态跟踪，性能分析，动态修改内核行为等，2004年由IBM发布，是名为Dprobes工具集的底层实现机制[1][2]，2005年合入Linux kernel。probe的含义是像一个探针，可以不修改分析对象源码的情况下，获取Kernel的运行时信息。

kprobe的实现原理是把指定地址（探测点）的指令替换成一个可以让cpu进入debug模式的指令，使执行路径暂停，跳转到probe 处理函数后收集、修改信息，再跳转回来继续执行。

![](https://img2020.cnblogs.com/blog/2276022/202101/2276022-20210110075907892-825572189.png#center)

 kprobe一直在X86系统上使用，ARM64的平台支持在2015年合入kernel [8]。

## kprobe代码示例

-   sample/kprobe/kprobe_example.c

```
// sample/kprobe/kprobe_example.c 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>

#define MAX_SYMBOL_LEN  64
static char symbol[MAX_SYMBOL_LEN] = "_do_fork";
module_param_string(symbol, symbol, sizeof(symbol), 0644);

/* For each probe you need to allocate a kprobe structure */
static struct kprobe kp = {
        .symbol_name    = symbol,
};

/* kprobe pre_handler: called just before the probed instruction is executed */
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
        pr_info("<%s> pre_handler: p->addr = 0x%p, pc = 0x%lx,"
                        " pstate = 0x%lx\n",
                p->symbol_name, p->addr, (long)regs->pc, (long)regs->pstate);

        /* A dump_stack() here will give a stack backtrace */
        return 0;
}

/* kprobe post_handler: called after the probed instruction is executed */
static void handler_post(struct kprobe *p, struct pt_regs *regs,
                                unsigned long flags)
{
        pr_info("<%s> post_handler: p->addr = 0x%p, pstate = 0x%lx\n",
                p->symbol_name, p->addr, (long)regs->pstate);
}

static int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnr)
{
        pr_info("fault_handler: p->addr = 0x%p, trap #%dn", p->addr, trapnr);
        /* Return 0 because we don't handle the fault. */
        return 0;
}

static int __init kprobe_init(void)
{
        int ret;
        kp.pre_handler = handler_pre;
        kp.post_handler = handler_post;
        kp.fault_handler = handler_fault;

        ret = register_kprobe(&kp);
        if (ret < 0) {
                pr_err("register_kprobe failed, returned %d\n", ret);
                return ret;
        }
        pr_info("Planted kprobe at %p\n", kp.addr);
        return 0;
}

static void __exit kprobe_exit(void)
{
        unregister_kprobe(&kp);
        pr_info("kprobe at %p unregistered\n", kp.addr);
}

```

-   sample/kprobe/jprobe_example.c

```
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>

/* Proxy routine having the same arguments as actual _do_fork() routine */
static long j_do_fork(unsigned long clone_flags, unsigned long stack_start,
              unsigned long stack_size, int __user *parent_tidptr,
              int __user *child_tidptr, unsigned long tls)
{
        pr_info("jprobe: clone_flags = 0x%lx, stack_start = 0x%lx "
                "stack_size = 0x%lx\n", clone_flags, stack_start, stack_size);

        /* Always end with a call to jprobe_return(). */
        jprobe_return();
        return 0;
}

static struct jprobe my_jprobe = {
        .entry                  = j_do_fork,
        .kp = {
                .symbol_name    = "_do_fork",
        },
};

static int __init jprobe_init(void)
{
        int ret;

        ret = register_jprobe(&my_jprobe);
        if (ret < 0) {
                pr_err("register_jprobe failed, returned %d\n", ret);
                return -1;
        }
        pr_info("Planted jprobe at %p, handler addr %p\n",
               my_jprobe.kp.addr, my_jprobe.entry);
        return 0;
}

static void __exit jprobe_exit(void)
{
        unregister_jprobe(&my_jprobe);
        pr_info("jprobe at %p unregistered\n", my_jprobe.kp.addr);
}

module_init(jprobe_init)
module_exit(jprobe_exit)
MODULE_LICENSE("GPL");
```

-   sample/kprobe/kretprobe_example.c

```
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/sched.h>

static char func_name[NAME_MAX] = "_do_fork";
module_param_string(func, func_name, NAME_MAX, S_IRUGO);
MODULE_PARM_DESC(func, "Function to kretprobe; this module will report the"
                        " function's execution time");

/* per-instance private data */
struct my_data {
        ktime_t entry_stamp;
};

/* Here we use the entry_hanlder to timestamp function entry */
static int entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
        struct my_data *data;

        if (!current->mm)
                return 1;       /* Skip kernel threads */

        data = (struct my_data *)ri->data;
        data->entry_stamp = ktime_get();
        return 0;
}

static int ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
        unsigned long retval = regs_return_value(regs);
        struct my_data *data = (struct my_data *)ri->data;
        s64 delta;
        ktime_t now;

        now = ktime_get();
        delta = ktime_to_ns(ktime_sub(now, data->entry_stamp));
        pr_info("%s returned %lu and took %lld ns to execute\n",
                        func_name, retval, (long long)delta);
        return 0;
}

static struct kretprobe my_kretprobe = {
        .handler                = ret_handler,
        .entry_handler          = entry_handler,
        .data_size              = sizeof(struct my_data),
        /* Probe up to 20 instances concurrently. */
        .maxactive              = 20,
};

static int __init kretprobe_init(void)
{
        int ret;

        my_kretprobe.kp.symbol_name = func_name;
        ret = register_kretprobe(&my_kretprobe);
        if (ret < 0) {
                pr_err("register_kretprobe failed, returned %d\n", ret);
                return -1;
        }
        pr_info("Planted return probe at %s: %p\n",
                        my_kretprobe.kp.symbol_name, my_kretprobe.kp.addr);
        return 0;
}

static void __exit kretprobe_exit(void)
{
        unregister_kretprobe(&my_kretprobe);
        pr_info("kretprobe at %p unregistered\n", my_kretprobe.kp.addr);

        /* nmissed > 0 suggests that maxactive was set too low. */
        pr_info("Missed probing %d instances of %s\n",
                my_kretprobe.nmissed, my_kretprobe.kp.symbol_name);
}

```



## kprobe源码和接口

### 源码

```
./include/linux/kprobes.h             # kprobe头文件
./include/asm-generic/kprobes.h
./kernel/kprobes.c                    # kprobe核心实现
./arch/arm64/include/asm/kprobes.h
./arch/arm64/kernel/probes/kprobes.c  # kprobe arm64支持
./arch/arm64/kernel/probes/kprobes_trampoline.S
./samples/kprobes/kprobe_example.c    # kprobe使用例子程序
./samples/kprobes/jprobe_example.c
./samples/kprobes/kretprobe_example.c
```

### 对外接口

```
struct kprobe {
    struct hlist_node hlist; // hash list保存所有kprobe，key是指令地址
    struct list_head list;   // 链接一个地址上注册的多个kprobe
    unsigned long nmissed;   // 被临时disabled的次数

    kprobe_opcode_t *addr;   // 探测点地址
    const char *symbol_name; // 探测点函数名
    unsigned int offset;     // 探测点在函数内的偏移

~~~~    kprobe_pre_handler_t pre_handler;       // 前处理函数
    kprobe_post_handler_t post_handler;     // 后处理函数
    kprobe_fault_handler_t fault_handler;   // 探测指令发生fault时的处理函数
    kprobe_break_handler_t break_handler;   // 在前三个handler里发生break trap的处理函数

    /* Saved opcode (which has been replaced with breakpoint) */
    kprobe_opcode_t opcode;  // 被breakpoint替换了的操作码

    /* copy of the original instruction */
    struct arch_specific_insn ainsn;  // 保存平台相关的被探测指令和下一条指令

    u32 flags;    // 状态标记
};

struct jprobe {
    struct kprobe kp;                                   
    void *entry;    /* probe handling code to jump to */
};
struct kretprobe {
    struct kprobe kp;                 
    kretprobe_handler_t handler;
    kretprobe_handler_t entry_handler;
    int maxactive;                    
    int nmissed;
    size_t data_size;
    struct hlist_head free_instances; 
    raw_spinlock_t lock;              
};
```

```
int register_kprobe(struct kprobe *p);                                       
int register_kprobes(struct kprobe **kps, int num);       
int register_jprobe(struct jprobe *p);                                       
int register_jprobes(struct jprobe **jps, int num);   
int register_kretprobe(struct kretprobe *rp);                                
int register_kretprobes(struct kretprobe **rps, int num);    
int disable_kprobe(struct kprobe *kp);
int enable_kprobe(struct kprobe *kp);                
void jprobe_return(void);                                                    
void unregister_jprobe(struct jprobe *p);                                    
void unregister_jprobes(struct jprobe **jps, int num);                       
void unregister_kprobe(struct kprobe *p);                                    
void unregister_kprobes(struct kprobe **kps, int num);                       
void unregister_kretprobe(struct kretprobe *rp);                             
void unregister_kretprobes(struct kretprobe **rps, int num);
```

kprobe的使用比较简单，只需要指定探测点地址，或者使用符号名+偏移的方式，定义xxx_handler，注册即可。注册后探测指令被替换，可以使用kprobe_enable/disable函数动态开关。

jprobe是kprobe的一种方便检查函数参数的实现方式，通过定义一个和探测函数相同原型的函数实现。

kretprobe用于检查函数返回值，可以定义函数入口handler和返回时的handler。

可以参看kernel源码中的samples/kprobes/下的例子程序。



## kprobe的实现及Arm64支持

### kprobe的管理

kprobe可以支持大量的探测点，为了快速查询和插入，使用哈希链表管理所有的kprobes，hash的key值是探测点的地址值。

![](https://img2020.cnblogs.com/blog/2276022/202101/2276022-20210110080012466-2123935918.png)

```
static struct hlist_head kprobe_table[KPROBE_TABLE_SIZE];
struct kprobe *get_kprobe(void *addr)
{
    struct hlist_head *head;
    struct kprobe *p;

    head = &kprobe_table[hash_ptr(addr, KPROBE_HASH_BITS)];
    hlist_for_each_entry_rcu(p, head, hlist) {
        if (p->addr == addr)
            return p;
    }
    return NULL;
}
int register_kprobe(struct kprobe *p)
{..

	INIT_HLIST_NODE(&p->hlist);                                    
	hlist_add_head_rcu(&p->hlist,                                  
	           &kprobe_table[hash_ptr(p->addr, KPROBE_HASH_BITS)]);
..}
```

### kprobe的注册

kprobe注册时传入的是struct kprobe结构体，里面包含指令地址或者函数名地址和函数内偏移，传入地址需要先检查是否在代码段里，且不在blacklist里，blacklist包含不能probe的函数，主要是kprobe本身的函数。

然后调用arch_prepare_kprobe解码指令，看指令是否是一些分支等特殊指令，需要特别处理。如果是正常可以probe的指令，调用arch_prepare_ss_slot把探测点的指令备份到slot page里，把下一条指令存入struct arch_probe_insn结构的restore成员里，在post_handler之后恢复执行。

arch_prepare_krpobe无误后把kprobe加入kprobe_table哈希链表。

然后调用arch_arm_kprobe替换探测点指令为BRK64_OPCODE_KPROBES指令。

![](https://img2020.cnblogs.com/blog/2276022/202101/2276022-20210110080122391-1568500813.png)



### kprobe的触发和处理

kprobe的触发和处理是通过brk exception和single step 单步exception执行的，每次的处理函数中会修改被异常中断的上下文（struct pt_regs）的指令寄存器，实现执行流的跳转。

异常机制依赖于CPU的体系结构，所以这部分代码是在arch/arm64/kernel/probes/kprobes.c 中实现的，这也是各个体系结构支持kprobe的主要功能。



#### 异常处理的注册

异常处理的注册在arch/arm64/kernel/debug-monitors.c， 是arm64的通用debug模块，kgdb也基于这个模块。

```
static int __init debug_traps_init(void)                                 
{
    hook_debug_fault_code(DBG_ESR_EVT_HWSS, single_step_handler, SIGTRAP,   // 单步异常处理函数
                  TRAP_TRACE, "single-step handler");
    hook_debug_fault_code(DBG_ESR_EVT_BRK, brk_handler, SIGTRAP,            // 断点异常处理函数
                  TRAP_BRKPT, "ptrace BRK handler");
    return 0; 
}
```

hook_debug_fault_code是替换arch/arm64/mm/fault.c 中的debug_fault_info异常表项：

```
/*      
 * __refdata because early_brk64 is __init, but the reference to it is
 * clobbered at arch_initcall time.
 * See traps.c and debug-monitors.c:debug_traps_init().
 */ 
static struct fault_info __refdata debug_fault_info[] = {
    { do_bad,   SIGTRAP,    TRAP_HWBKPT,    "hardware breakpoint"   },
    { do_bad,   SIGTRAP,    TRAP_HWBKPT,    "hardware single-step"  },
    { do_bad,   SIGTRAP,    TRAP_HWBKPT,    "hardware watchpoint"   },
    { do_bad,   SIGBUS,     0,      "unknown 3"     },
    { do_bad,   SIGTRAP,    TRAP_BRKPT, "aarch32 BKPT"      },
    { do_bad,   SIGTRAP,    0,      "aarch32 vector catch"  },
    { early_brk64,  SIGTRAP,    TRAP_BRKPT, "aarch64 BRK"       },
    { do_bad,   SIGBUS,     0,      "unknown 7"     },
};

void __init hook_debug_fault_code(int nr,
                  int (*fn)(unsigned long, unsigned int, struct pt_regs *),
                  int sig, int code, const char *name)
{                 
    BUG_ON(nr < 0 || nr >= ARRAY_SIZE(debug_fault_info));

    debug_fault_info[nr].fn     = fn;
    debug_fault_info[nr].sig    = sig;
    debug_fault_info[nr].code   = code;
    debug_fault_info[nr].name   = name;
}
```

#### BRK和SS异常执行流程

```
arch/arm64/kernel/entry.S:
el1_dbg:
      bl  do_debug_exception
            fault_info *inf = debug_fault_info + DBG_ESR_EVT(esr);
                  inf->fn(addr, esr, regs)
                        brk_handler()
                              kprobe_breakpoint_handler()
			single_step_handler()
			      kprobe_single_step_handler()
```

breakpoint断点执行流程主要任务是执行pre_handler，把slot中保存的原指令设置进regs上下文的指令寄存器里，这样退出brk异常后，会单步执行被probe的指令。

![](https://img2020.cnblogs.com/blog/2276022/202101/2276022-20210110080205124-2029164662.png)

单步执行探测点的指令后，会触发单步异常，进入single_step_handler，调用kprobe_single_step_handler，主要任务是恢复执行路径，调用用户注册的post_handler。

首先检查当前单步异常的指令地址是否是之前设定的下一条指令地址；然后关闭单步状态，即只执行一次单步。

最后设置寄存器上下文中的指令寄存器为探测点指令的下一条指令。调用post_handler，结束这个kprobe的工作。

![](https://img2020.cnblogs.com/blog/2276022/202101/2276022-20210110080235584-624249302.png)

#### 其它未详细介绍的部分

1.  被替换的指令放在哪里？
    -   slot page，使用了module_alloc分配可以执行的内存页
2.  一个探测点由多个probe注册怎么处理
    -   aggrprobe
3.  SMP、中断、抢占时可能有kprobe重入，如何处理
    -   实现了reenter检查机制，允许probe嵌套
4.  kprobe的性能
    -   break指令导致CPU执行停止，时间开销较大
    -   x86实现了优化机制，使用jmp指令替换int3 这种break指令，速度提升10倍；ARM64中未实现。



![](https://img2020.cnblogs.com/blog/2276022/202101/2276022-20210110080302591-104822113.png)

### kprobe 的问题

1.  probe函数的定义要非常小心，否则会引起kernel panic或其它异常，比如probe的一个函数临界区被调用，而你的probe handler是睡眠的，就会报错
2.  需要依赖kernel源码树或build文件，编写kernel 模块

### kprobe的优势

1.  可以不重编kernel对生产系统进行探测，在PC和Server中比较有意义
2.  可以动态观察、修改几乎任意代码路径的状态，比Ftrace有更强的定制性





## kprobe适用场景

#### 1.观察 Observability

-   kprobe观察任一点的寄存器状态，全局变量，局部变量，jprobe观察函数参数，kretprobe观察返回值
-   函数调用，profiling等
    -   这块Ftrace可以替代
-   数据收集
    -   例如：打印genpool的bitmap，观察ION carveout的使用情况

#### 2. 篡改 Hacking

-   error-injection
    -   修改函数返回值或者判断条件，使代码进入错误处理路径，提高测试覆盖率
-   data-injection
    -   动态修改数据，仿真测试数据，比如注入温度传感器温度，模仿各种测试值测试
    -   实验：对pvt做温度抽样值注入，测试各种温度下的计算问题
-   动态补丁



#### 3. 调试

#### 4. 其它？

## kprobe的应用

### 1.trace_kprobe
kernel ftrace子系统基于kprobe实现了kprobe_event，可以probe任意函数。

优点是无需写代码，方便简单的函数trace，可以获得函数参数值和返回值

缺点是不能做动态修改

使用方法参看kernel/Documentation/trace/kprobetrace.txt

-   kprobe_event使用说明和示例

```
 24 Synopsis of kprobe_events
 25 -------------------------
 26   p[:[GRP/]EVENT] [MOD:]SYM[+offs]|MEMADDR [FETCHARGS]  : Set a probe
 27   r[MAXACTIVE][:[GRP/]EVENT] [MOD:]SYM[+0] [FETCHARGS]  : Set a return probe
 28   -:[GRP/]EVENT                     : Clear a probe
 29 
 30  GRP        : Group name. If omitted, use "kprobes" for it.
 31  EVENT      : Event name. If omitted, the event name is generated
 32           based on SYM+offs or MEMADDR.
 33  MOD        : Module name which has given SYM.
 34  SYM[+offs] : Symbol+offset where the probe is inserted.
 35  MEMADDR    : Address where the probe is inserted.
 36  MAXACTIVE  : Maximum number of instances of the specified function that
 37           can be probed simultaneously, or 0 for the default value
 38           as defined in Documentation/kprobes.txt section 1.3.1.
 39 
 40  FETCHARGS  : Arguments. Each probe can have up to 128 args.
 41   %REG      : Fetch register REG
 42   @ADDR     : Fetch memory at ADDR (ADDR should be in kernel)
 43   @SYM[+|-offs] : Fetch memory at SYM +|- offs (SYM should be a data symbol)
 44   $stackN   : Fetch Nth entry of stack (N >= 0)
 45   $stack    : Fetch stack address.
 46   $retval   : Fetch return value.(*)
 47   $comm     : Fetch current task comm.
 48   +|-offs(FETCHARG) : Fetch memory at FETCHARG +|- offs address.(**)
 49   NAME=FETCHARG : Set NAME as the argument name of FETCHARG.
 50   FETCHARG:TYPE : Set TYPE as the type of FETCHARG. Currently, basic types
 51           (u8/u16/u32/u64/s8/s16/s32/s64), hexadecimal types
 52           (x8/x16/x32/x64), "string" and bitfield are supported.
 53 
 54   (*) only for return probe.
 55   (**) this is useful for fetching a field of data structures.

echo 'p:myprobe do_sys_open filename=+0(%x1):string flags=%x2 mode=%x3' > /sys/kernel/debug/tracing/kprobe_events
echo 'r:myretprobe do_sys_open $retval ' >> /sys/kernel/debug/tracing/kprobe_events
echo > trace
echo 1 > events/kprobes/enable

root@x3dvbj3-hynix2G-2666:/sys/kernel/debug/tracing# cat trace
# tracer: nop
#
#                              _-----=> irqs-off
#                             / _----=> need-resched
#                            | / _---=> hardirq/softirq
#                            || / _--=> preempt-depth
#                            ||| /     delay
#           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |       |   ||||       |         |
              sh-1499  [002] d...  1902.985663: myprobe: (do_sys_open+0x0/0x208) filename="/etc/passwd" flags=0xa0000 mode=0x0
              sh-1499  [002] d..1  1902.985730: myretprobe: (SyS_openat+0x3c/0x50 <- do_sys_open) arg1=0x3
              ps-12596 [001] d...  1903.694056: myprobe: (do_sys_open+0x0/0x208) filename="/etc/ld.so.cache" flags=0xa0000 mode=0x0
            grep-12597 [002] d...  1903.694056: myprobe: (do_sys_open+0x0/0x208) filename="/etc/ld.so.cache" flags=0xa0000 mode=0x0
            grep-12597 [002] d..1  1903.694103: myretprobe: (SyS_openat+0x3c/0x50 <- do_sys_open) arg1=0x3
              ps-12596 [001] d..1  1903.694103: myretprobe: (SyS_openat+0x3c/0x50 <- do_sys_open) arg1=0x3
            grep-12597 [002] d...  1903.694147: myprobe: (do_sys_open+0x0/0x208) filename="/lib/libm.so.6" flags=0xa0000 mode=0x0
              ps-12596 [001] d...  1903.694148: myprobe: (do_sys_open+0x0/0x208) filename="/lib/libm.so.6" flags=0xa0000 mode=0x0
            grep-12597 [002] d..1  1903.694179: myretprobe: (SyS_openat+0x3c/0x50 <- do_sys_open) arg1=0x3
              ps-12596 [001] d..1  1903.694179: myretprobe: (SyS_openat+0x3c/0x50 <- do_sys_open) arg1=0x3
            grep-12597 [002] d...  1903.694364: myprobe: (do_sys_open+0x0/0x208) filename="/lib/libresolv.so.2" flags=0xa0000 mode=0x0
              ps-12596 [001] d...  1903.694381: myprobe: (do_sys_open+0x0/0x208) filename="/lib/libresolv.so.2" flags=0xa0000 mode=0x0
            grep-12597 [002] d..1  1903.694398: myretprobe: (SyS_openat+0x3c/0x50 <- do_sys_open) arg1=0x3
              ps-12596 [001] d..1  1903.694413: myretprobe: (SyS_openat+0x3c/0x50 <- do_sys_open) arg1=0x3

```

通过offset和类型打印，实现结构体内部成员的打印，但是需要知道寄存器和参数的对应关系和结构体成员的偏移。[13]提到了新的function_event机制，可以直接传递参数名。

-   获取数据结构偏移的例子：打印ip_rcv的网络设备名和收发包数

```
$ aarch64-linux-gnu-gdb vmlinux
(gdb) ptype/o struct net_device
/* offset    |  size */  type = struct net_device {
/*    0      |    16 */    char name[16];      //设备名
/*   16      |    16 */    struct hlist_node {
/*   16      |     8 */        struct hlist_node *next;
/*   24      |     8 */        struct hlist_node **pprev;
...
/*  272      |   184 */    struct net_device_stats {
/*  272      |     8 */        unsigned long rx_packets;
/*  280      |     8 */        unsigned long tx_packets;
/*  288      |     8 */        unsigned long rx_bytes;

(gdb) print (int)&((struct net_device *)0)->stats
$3 = 272

cd /sys/kernel/debug/tracing/
echo 'p:net ip_rcv name=+0(%x1):string rx_pkts=+272(%x1):u64 tx_pkts=+280(%x1):u64 ' > kprobe_events
echo 1 > events/kprobes/enable

root@x3dvbj3-hynix2G-2666:/sys/kernel/debug/tracing# cat trace
# tracer: nop
#
#                              _-----=> irqs-off
#                             / _----=> need-resched
#                            | / _---=> hardirq/softirq
#                            || / _--=> preempt-depth
#                            ||| /     delay
#           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |       |   ||||       |         |
          <idle>-0     [000] d.s1   837.456030: net: (ip_rcv+0x0/0x3c8) name="eth0" rx_pkts=2445 tx_pkts=131
        dropbear-2776  [000] d.s1   837.457538: net: (ip_rcv+0x0/0x3c8) name="eth0" rx_pkts=2446 tx_pkts=133
          <idle>-0     [000] d.s1   837.662158: net: (ip_rcv+0x0/0x3c8) name="eth0" rx_pkts=2447 tx_pkts=133
          <idle>-0     [000] d.s1   837.668020: net: (ip_rcv+0x0/0x3c8) name="eth0" rx_pkts=2448 tx_pkts=135
root@x3dvbj3-hynix2G-2666:/sys/kernel/debug/tracing# ifconfig
eth0      Link encap:Ethernet  HWaddr 00:70:64:7b:59:23  
          inet addr:192.168.1.10  Bcast:192.168.1.255  Mask:255.255.255.0
          inet6 addr: fe80::270:64ff:fe7b:5923/64 Scope:Link
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:2450 errors:0 dropped:0 overruns:0 frame:0
          TX packets:136 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000 
          RX bytes:196959 (192.3 KiB)  TX bytes:16262 (15.8 KiB)
          Interrupt:44 Base address:0x4000

```

### 2.systemtap

systemap 实现了一种脚本语言，把脚本翻译成C代码，编译成kernel 模块，注册成kprobe handler，对kernel进行探测[12]。

由Redhat开发，在Redhat系统里运行比较稳定，其它环境易用性较差。

-   Systemtap脚本示例

```
# cat inode-watch.stp
probe kernel.function ("vfs_write"),
kernel.function ("vfs_read")
{
	if (@defined($file->f_path->dentry)) {
		dev_nr = $file->f_path->dentry->d_inode->i_sb->s_dev
		inode_nr = $file->f_path->dentry->d_inode->i_ino
	} else {
		dev_nr = $file->f_dentry->d_inode->i_sb->s_dev
		inode_nr = $file->f_dentry->d_inode->i_ino
	}
	if (dev_nr == ($1 << 20 | $2) # major/minor device
				&& inode_nr == $3)
	printf ("%s(%d) %s 0x%x/%u\n",
	execname(), pid(), ppfunc(), dev_nr, inode_nr)
}
# stat -c "%D %i" /etc/crontab
fd03 133099
# stap inode-watch.stp 0xfd 3 133099
more(30789) vfs_read 0xfd00003/133099
more(30789) vfs_read 0xfd00003/133099

```

### 3. perf probe
perf 的probe命令提供了添加动态探测点的功能， 参看 kernel/tools/perf/Documentation/perf-probe.txt

````
236 EXAMPLES
237 --------
238 Display which lines in schedule() can be probed:
239 
240  ./perf probe --line schedule
241 
242 Add a probe on schedule() function 12th line with recording cpu local variable:
243 
244  ./perf probe schedule:12 cpu
245  or
246  ./perf probe --add='schedule:12 cpu'
247 
248 Add one or more probes which has the name start with "schedule".
249 
250  ./perf probe schedule*
251  or
252  ./perf probe --add='schedule*'
253 
254 Add probes on lines in schedule() function which calls update_rq_clock().
255 
256  ./perf probe 'schedule;update_rq_clock*

````

### 4. eBPF

eBPF (Extended Berkeley Packet Filter)是最近比较流行的内核探测技术，通过在Kernel中运行一个虚拟机，执行用户传入的字节码，实现对kernel内部信息的探测能力，比较安全易用。

其动态探测部分是基于kprobe和kretprobe实现的，可以实现对任意函数的探测、数据处理等能力。

## Appendix

### ARM64 BRK 指令

![](https://img2020.cnblogs.com/blog/2276022/202101/2276022-20210110080524971-1167512348.png)

IMM16是自定义的，用于标识是哪种brk用途，比如kgdb或者kprobe。主要使用的几个BRK立即数定义如下

```
#define FAULT_BRK_IMM           0x100    
#define KGDB_DYN_DBG_BRK_IMM        0x400
#define KGDB_COMPILED_DBG_BRK_IMM   0x401
#define BUG_BRK_IMM         0x800
#define BRK64_ESR_KPROBES   0x0004
#define BRK64_ESR_UPROBES   0x0005

#define AARCH64_BREAK_MON   0xd4200000                                        

#define BRK64_OPCODE_KPROBES    (AARCH64_BREAK_MON | (BRK64_ESR_KPROBES << 5))                                        
#define BRK64_OPCODE_UPROBES    (AARCH64_BREAK_MON | (BRK64_ESR_UPROBES << 5))
#define AARCH64_BREAK_FAULT (AARCH64_BREAK_MON | (FAULT_BRK_IMM << 5))        
#define AARCH64_BREAK_KGDB_DYN_DBG  (AARCH64_BREAK_MON | (KGDB_DYN_DBG_BRK_IMM << 5)) 
```

### ABI

写kprobe驱动要知道函数参数和寄存器的对应关系，需要了解对应体系架构的ABI （Application Binary Interface）

X86 and X86_64 ABI :

```
1. User-level applications use as integer registers for passing the sequence
%rdi, %rsi, %rdx, %rcx, %r8 and %r9. The kernel interface uses %rdi,
%rsi, %rdx, %r10, %r8 and %r9.
2. A system-call is done via the syscall instruction. The kernel destroys
registers %rcx and %r11.
3. The number of the syscall has to be passed in register %rax.
4. System-calls are limited to six arguments, no argument is passed directly on
the stack.
5. Returning from the syscall, register %rax contains the result of the
system-call. A value in the range between -4095 and -1 indicates an error,
it is -errno.
6. Only values of class INTEGER or class MEMORY are passed to the kernel.
```

ARM64 ABI : [ARM64 Parameters in general-purpose registers](https://developer.arm.com/documentation/den0024/a/the-abi-for-arm-64-bit-architecture/register-use-in-the-aarch64-procedure-call-standard/parameters-in-general-purpose-registers)

![](https://img2020.cnblogs.com/blog/2276022/202101/2276022-20210110080420210-479753030.png)