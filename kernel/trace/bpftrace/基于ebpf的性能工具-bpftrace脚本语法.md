bpftrace 通过高度抽象的封装来使用 eBPF，大多数功能只需要寥寥几笔就可以运行起来，可以很快让我们搞清楚 eBPF 是什么样的，而暂时不关心 eBPF 复杂的内部机理。由于 bpftrace 深受 AWK 和 c 的影响，bpftrace 使用起来于 AWK 非常相似，那些内核 hook 注入点几乎可以按普通字符串匹配来理解，非常容易上手。

## bpftrace脚本语法

### 脚本格式

-   bpftrace脚本基本格式如下：

```
probe:filter: {
   actions;
}
```

-   bpftrace语法深受AWK的影响，{前的部分相当于AWK的condition，{}中的部分相当于AWK的action。只不过bpftrace执行actions的条件是触发probe名称指定的事件。
-   probe是探针的名称，我们知道内核中函数非常多，为了方便，内核对probe做了namespace处理，这里的probe通常是以冒号:分割的一组名称，比如:

```
tracepoint:timer:tick_stop
kprobe:do_sys_open
```

-   显然，最后一部分表示的是函数名称，其他部分则是namespace，这样做有两点好处：①便于查找函数；②便于定位不同模块中的同名函数。
-   bpftrace除了可以监听指定的probe事件，还有两个特殊的probe：BEGIN，END。这与AWK类似，它们分别在bpftrace程序执行开始、结束时，无条件的执行一些操作，比如完成一些初始化、清理工作等。

```
BEGIN{
    print("hello world.\n");
}

END {
    print("bye world.\n");
}
```

-   filter是可选的，有时候我们只需要探测特定条件下函数的行为，比如参数为某个值的时候，就可以用到filter，这需要了解bpftrace如何访问probe的变量，我们稍晚再说。



### probe参数

ebpf支持的probe：hardware，iter，kfunc，kprobe，software，tracepoint，uprobe。

![](https://developer.qcloudimg.com/http-save/yehe-3094129/1b82be2f4da064c7b62eeb3c619a5ae2.png)

1.  dynamic tracing

-   ebpf提供了内核和应用的动态trace，分别用于探测函数入口处和函数返回(ret)处的信息。
    -   ①面向内核的 kprobe/kretprobe，k = kernel
    -   ②面向应用的 uprobe/uretprobe，u = user land

-   kprobe/kretprobe 可以探测内核大部分函数，出于安全考虑，有部分内核函数不允许安装探针，另外也可以配合 offset 探测函数中任意位置的信息。
-   uprobe/uretprobe 则可以为应用的任意函数安装探针。
-   动态 trace 技术依赖内核和应用的符号表，对于那些 inline 或者 static 函数则无法直接安装探针，需要自行通过 offset 实现。可以借助 nm 或者 strings 指令查看应用的符号表。
-   这两种动态 trace 技术的原理与 GDB 类似，当对某段代码安装探针，内核会将目标位置指令复制一份，并替换为 int3 中断, 执行流跳转到用户指定的探针 handler，再执行备份的指令，如果此时也指定了 ret 探针，也会被执行，最后再跳转回原来的指令序列。
-   kprobe 和 uprobe 可以通过 arg0、arg1... ... 访问所有参数；kretprobe 和 uretprobe 通过 retval 访问函数的返回值。除了基本类型：char、int 等，字符串需通过 str() 函数才能访问。

2.  static tracing

-   静态 trace，所谓 “静态” 是指探针的位置、名称都是在代码中硬编码的，编译时就确定了。静态 trace 的实现原理类似 callback，当被激活时执行，关闭时不执行，性能比动态 trace 高一些。
    -   ① 内核中的静态trace：tracepoint
    -   ② 应用中的静态trace: usdt = Userland Statically Defined Tracing

-   静态 trace 已经在内核和应用中饱含了探针参数信息，可以直接通过 args->参数名 访问函数参数。tracepoint 的 参数 format 信息可以通过 bpftrace -v probe 查看：

```
youyeetoo@youyeetoo:~$ bpftrace -lv tracepoint:raw_syscalls:sys_exit
tracepoint:raw_syscalls:sys_exit
    long id
    long ret
youyeetoo@youyeetoo:~$ 
```

-   或者访问debugfs：

```
youyeetoo@youyeetoo:~$ cat /sys/kernel/debug/tracing/events/raw_syscalls/sys_exit/format
name: sys_exit
ID: 348
format:
 field:unsigned short common_type; offset:0; size:2; signed:0;
 field:unsigned char common_flags; offset:2; size:1; signed:0;
 field:unsigned char common_preempt_count; offset:3; size:1; signed:0;
 field:int common_pid; offset:4; size:4; signed:1;

 field:long id; offset:8; size:8; signed:1;
 field:long ret; offset:16; size:8; signed:1;

print fmt: "NR %ld = %ld", REC->id, REC->ret
youyeetoo@youyeetoo:~$  
```

