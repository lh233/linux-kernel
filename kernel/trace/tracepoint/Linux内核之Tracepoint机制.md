## 引入tracepoint的背景

当需要获取内核的debug信息时，通常你会通过以下printk的方式打印信息：

```
void trace_func()
{
    //……
    printk("输出信息");
    //……
}
```

缺点：

-   内核中printk是统一控制的，各个模块的printk都会被打印，无法只打印需要关注的模块
-   如果需要修改/新增打印信息，需要修改所有受影响的printk语句。这些printk分散在代码多处，每个地方都需要修改
-   嵌入式系统中，如果printk信息量大，console（如果有）有大量的打印输出，用户无法在console输入命令，影响人机交互



## 内核解决方案

内核采用“插桩”的方法抓取log，“插桩”也称为Tracepoint，Tracepoint是Linux内核预先定义的静态探测点，它分布于内核的各个子系统中，每种Tracepoint有一个name、一个enable开关、一系列桩函数、注册桩函数的函数、卸载桩函数的函数。“桩函数”功能类似于printk，不过“桩函数”并不会把信息打印到console，而是输出到内核的ring buffer（环形缓冲区），缓冲区中的信息通过debugfs对用户呈现。每个tracepoint提供一个钩子来调用probe函数。一个tracepoint可以打开或关闭。打开时，probe函数关联到tracepoint；关闭时，probe函数不关联到tracepoint。tracepoint关闭时对kernel产生的影响很小，只是增加了极少的时间开销(一个分支条件判断)，极小的空间开销(一条函数调用语句和几个数据结构)。只有挂载了钩子函数才会真正启用trace功能。这个钩子函数可以由开发者编写内核module来实现，并且需要在钩子函数中获取我们调试所需要的信息并导出到用户态，这样就可以获取内核运行时的信息了。当一个tracepoint打开时，用户提供的probe函数在每次这个tracepoint执行都会被调用。

![](https://pic3.zhimg.com/80/v2-f7d5f01c1dbdd3e154e25843f58dd1ca_720w.webp)

直接使用tracepoint并不是那么的容易，因此内核提供了event trace功能。event trace的实现依赖于tracepoint机制，内核提前帮我们实现了钩子函数并挂到tracepoint上，当使能一个event trace时，它会输出内容到ftrace ringbuffer中，这样就可以获取到内核运行信息了。当然有时候event trace并不符合我们的需要，那么此时就只得自己编写module来实现需求了。



## 查看tracepoint

可通过debugfs查看：

```
root: /5g_build/5g_Main/$ ls /sys/kernel/debug/tracing/events/
block             dma_fence   ext4      ftrace        hyperv     irq          libata   mpx   oom      qdisc         rcu     scsi    sunrpc    udp        writeback
bridge            drm         filelock  gpio          i2c        irq_vectors  mce      napi  pagemap  random        regmap  signal  syscalls  vmscan     xen
compaction        enable      filemap   header_event  intel_ish  jbd2         migrate  net   power    ras           rpm     skb     task      vsyscall   xhci-hcd
context_tracking  exceptions  fs_dax    header_page   iommu      kmem         module   nfs   printk   raw_syscalls  sched   sock    timer     workqueue
```

系统中定义的tracepoint都在该events目录中，比如查看block子系统中的tracepoint：

```
root: /5g_build/5g_Main/$ ls /sys/kernel/debug/tracing/events/block/
block_bio_backmerge  block_bio_complete    block_bio_queue  block_dirty_buffer  block_plug      block_rq_complete  block_rq_issue  block_rq_requeue  block_split         block_unplug  filter
block_bio_bounce     block_bio_frontmerge  block_bio_remap  block_getrq         block_rq_abort  block_rq_insert    block_rq_remap  block_sleeprq     block_touch_buffer  enable
```

还可以通过perf查看：

```
 root: /5g_build/5g_Main/$ perf list tracepoint
  block:block_bio_backmerge                          [Tracepoint event]
  block:block_bio_bounce                             [Tracepoint event]
  block:block_bio_complete                           [Tracepoint event]
  block:block_bio_frontmerge                         [Tracepoint event]
  block:block_bio_queue                              [Tracepoint event]
  block:block_bio_remap                              [Tracepoint event]
  block:block_dirty_buffer                           [Tracepoint event]
  block:block_getrq                                  [Tracepoint event]
  block:block_plug                                   [Tracepoint event]
  block:block_rq_abort                               [Tracepoint event]
  block:block_rq_complete                            [Tracepoint event]
  block:block_rq_insert                              [Tracepoint event]
  block:block_rq_issue                               [Tracepoint event]
  block:block_rq_remap                               [Tracepoint event]
  block:block_rq_requeue                             [Tracepoint event]
  block:block_sleeprq                                [Tracepoint event]
  block:block_split                                  [Tracepoint event]
  block:block_touch_buffer                           [Tracepoint event]
  block:block_unplug                                 [Tracepoint event]
  bridge:br_fdb_add                                  [Tracepoint event]
  bridge:br_fdb_external_learn_add                   [Tracepoint event]
  bridge:br_fdb_update                               [Tracepoint event]
  bridge:fdb_delete                                  [Tracepoint event]
  compaction:mm_compaction_isolate_freepages         [Tracepoint event]
  compaction:mm_compaction_isolate_migratepages      [Tracepoint event]
```

## Tracepoint数据格式

每个tracepoint都会按照自己定义的格式来输出信息，可以在用户态来查看tracepoint记录的内容格式，比如：

```
root: /5g_build/5g_Main/$ cat /sys/kernel/debug/tracing/events/syscalls/sys_enter_open/format
name: sys_enter_open
ID: 484
format:
        field:unsigned short common_type;       offset:0;       size:2; signed:0;
        field:unsigned char common_flags;       offset:2;       size:1; signed:0;
        field:unsigned char common_preempt_count;       offset:3;       size:1; signed:0;
        field:int common_pid;   offset:4;       size:4; signed:1;

        field:int nr;   offset:8;       size:4; signed:1;
        field:const char * filename;    offset:16;      size:8; signed:0;
        field:int flags;        offset:24;      size:8; signed:0;
        field:umode_t mode;     offset:32;      size:8; signed:0;

print fmt: "filename: 0x%08lx, flags: 0x%08lx, mode: 0x%08lx", ((unsigned long)(REC->filename)), ((unsigned long)(REC->flags)), ((unsigned long)(REC->mode))
```

格式信息可以用来解析二进制的trace流数据，也可以用这个格式中的内容来做trace filter，利用filter功能指定过滤条件后，将只会看到过滤后的事件数据。格式信息中包括两部分内容：

-   第一部分是通用的格式，这类通用字段都带有common前缀，这是所有的tracepoint event都具备的字段
-   第二部分就是各个tracepoint所自定义的格式字段，比如上面的nr，filename等等。格式信息的最后一列是tracepoint的打印内容格式，通过这个可以看到打印数据的字段来源。



## 如何使用Tracepoint

tracepoint复用了ftrace的ringbuffer，当使能了一个tracepoint之后，可以通过cat /sys/kernel/debug/tracing/trace来查看它的输出。另外上文也有提到，还可以通过设置filter过滤事件。

比如想要过滤所有的openat事件，指定过滤所有打开flags为0的事件：

```
echo flags==0 > /sys/kernel/debug/tracing/events/syscalls/sys_enter_openat/filter
echo 1 > /sys/kernel/debug/tracing/events/syscalls/sys_enter_openat/enable
cat /sys/kernel/debug/tracing/trace_pipe
```

如果想要把过滤条件清除，只需要写入0到指定的filter即可：

```
echo 0 > /sys/kernel/debug/tracing/events/syscalls/sys_enter_openat/filter
```

如果想要跟踪sched_switch这个tracepoint点，设置过滤条件prev_pid和next_pid都为1的进程：

```
root: /5g_build/5g_Main/$ echo "(prev_pid == 1 || next_pid == 1)" >  /sys/kernel/debug/tracing/events/sched/sched_switch/filter
root: /5g_build/5g_Main/$ echo 1 > /sys/kernel/debug/tracing/events/sched/sched_switch/enable
root: /5g_build/5g_Main/$ cat /sys/kernel/debug/tracing/trace_pipe
          <idle>-0     [006] d... 2741443.520814: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741443.522103: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741443.522141: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741443.522816: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741443.525867: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741443.526448: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741443.727718: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741443.729145: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741443.729190: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741443.729864: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741443.733095: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741443.733663: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741444.799623: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741444.799777: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741446.049617: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741446.049761: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741446.114969: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741446.116232: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741446.116298: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741446.116756: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741446.119312: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741446.119702: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741446.324820: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741446.326196: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741446.326251: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741446.326945: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [006] d... 2741446.329697: sched_switch: prev_comm=swapper/6 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [006] d... 2741446.330272: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/6 next_pid=0 next_prio=120
          <idle>-0     [000] d... 2741446.526418: sched_switch: prev_comm=swapper/0 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [000] d... 2741446.527526: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/0 next_pid=0 next_prio=120
          <idle>-0     [000] d... 2741446.527569: sched_switch: prev_comm=swapper/0 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [000] d... 2741446.528133: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/0 next_pid=0 next_prio=120
          <idle>-0     [000] d... 2741446.531700: sched_switch: prev_comm=swapper/0 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [000] d... 2741446.532191: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/0 next_pid=0 next_prio=120
          <idle>-0     [004] d... 2741446.730287: sched_switch: prev_comm=swapper/4 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [004] d... 2741446.731656: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/4 next_pid=0 next_prio=120
          <idle>-0     [004] d... 2741446.731683: sched_switch: prev_comm=swapper/4 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
         systemd-1     [004] d... 2741446.732349: sched_switch: prev_comm=systemd prev_pid=1 prev_prio=120 prev_state=S ==> next_comm=swapper/4 next_pid=0 next_prio=120
          <idle>-0     [004] d... 2741446.734980: sched_switch: prev_comm=swapper/4 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=systemd next_pid=1 next_prio=120
...
```

可以看到对应的prev_id或者next_pid为1时才会输出



### perf中使用tracepoint

借助perf工具，可以跟踪tracepoint事件，前文已经介绍了如何用perf查看tracepoint，假如想要跟踪网络驱动收发包的情况：

```
perf record -e 'net:netif_rx','net:net_dev_queue' -a   -- sleep 10
```

记录10秒后，使用：

```
perf script
```

来查看记录到的信息，截取部分示例如下：

```
           node  1072 [001] 2741584.673790: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5bb698af8 len=214
            node  1072 [001] 2741584.673794: net:net_dev_queue: dev=veth_558d087c skbaddr=0xffff92c5bb698af8 len=214
            node  1072 [001] 2741584.673815: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5bb698af8 len=264
            grep     0 [001] 2741584.680569:      net:netif_rx: dev=veth_558d087c skbaddr=0xffff92ca6e714c00 len=155
            node  1072 [001] 2741584.683656: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5bb698cf8 len=98
            node  1072 [001] 2741584.683661: net:net_dev_queue: dev=veth_558d087c skbaddr=0xffff92c5bb698cf8 len=98
            node  1072 [001] 2741584.683682: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5bb698cf8 len=148
            grep     0 [001] 2741584.744696:      net:netif_rx: dev=veth_558d087c skbaddr=0xffff92ca6e715800 len=40
            grep     0 [001] 2741584.744703:      net:netif_rx: dev=veth_558d087c skbaddr=0xffff92ca736b8800 len=40
             awk     0 [006] 2741584.862150: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5fea64100 len=66
       ovs-ofctl     0 [000] 2741584.882151: net:net_dev_queue: dev=eth0 skbaddr=0xffff92caa3dafef8 len=208
       ovs-ofctl     0 [000] 2741584.883350: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5d2a8c100 len=66
       ovs-ofctl     0 [000] 2741584.883375: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5d2a8db00 len=66
       ovs-ofctl     0 [001] 2741584.884316: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5bb69b6f8 len=263
       ovs-ofctl     0 [001] 2741584.885924: net:net_dev_queue: dev=eth0 skbaddr=0xffff92caa3cf9b00 len=66
       ovs-ofctl     0 [001] 2741584.888257: net:net_dev_queue: dev=eth0 skbaddr=0xffff92caa3cf9a00 len=66
       ovs-ofctl     0 [001] 2741584.888843: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5bb69b6f8 len=280
       ovs-ofctl     0 [001] 2741584.888924: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5bb698cf8 len=1107
       ovs-ofctl     0 [001] 2741584.888946: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5bb6984f8 len=104
       ovs-ofctl     0 [001] 2741584.889618: net:net_dev_queue: dev=eth0 skbaddr=0xffff92caa3cf9a00 len=66
       ovs-ofctl     0 [001] 2741584.892419: net:net_dev_queue: dev=eth0 skbaddr=0xffff92caa3cf9800 len=66
              wc     0 [001] 2741584.985872:      net:netif_rx: dev=veth_558d087c skbaddr=0xffff92caa3cf9a00 len=103
              wc     0 [001] 2741584.986657: net:net_dev_queue: dev=eth0 skbaddr=0xffff92caa3cf8000 len=66
            node  1072 [005] 2741584.989219: net:net_dev_queue: dev=eth0 skbaddr=0xffff92ca6fe748f8 len=109
            node  1072 [005] 2741584.989222: net:net_dev_queue: dev=veth_558d087c skbaddr=0xffff92ca6fe748f8 len=109
            node  1072 [005] 2741584.989244: net:net_dev_queue: dev=eth0 skbaddr=0xffff92ca6fe748f8 len=159
              wc     0 [005] 2741585.001911:      net:netif_rx: dev=veth_558d087c skbaddr=0xffff92caa3cf9b00 len=82
            node  1072 [005] 2741585.004758: net:net_dev_queue: dev=eth0 skbaddr=0xffff92ca6fe75af8 len=99
            node  1072 [005] 2741585.004762: net:net_dev_queue: dev=veth_558d087c skbaddr=0xffff92ca6fe75af8 len=99
            node  1072 [005] 2741585.004798: net:net_dev_queue: dev=eth0 skbaddr=0xffff92ca6fe75af8 len=149
           pidof     0 [005] 2741585.053168:      net:netif_rx: dev=veth_558d087c skbaddr=0xffff92ca702cf400 len=83
            node  1072 [000] 2741585.056084: net:net_dev_queue: dev=eth0 skbaddr=0xffff92caa3dac6f8 len=99
            node  1072 [000] 2741585.056089: net:net_dev_queue: dev=veth_558d087c skbaddr=0xffff92caa3dac6f8 len=99
            node  1072 [000] 2741585.056110: net:net_dev_queue: dev=eth0 skbaddr=0xffff92caa3dac6f8 len=149
             awk     0 [000] 2741585.103222:      net:netif_rx: dev=veth_558d087c skbaddr=0xffff92ca6e615d00 len=40
    cpptools-srv 28070 [000] 2741585.184113:      net:netif_rx: dev=veth_558d087c skbaddr=0xffff92ca6e615600 len=87
            node  1072 [004] 2741585.186539: net:net_dev_queue: dev=eth0 skbaddr=0xffff92c5d2f152f8 len=110
```



## 添加Tracepoint

| 数据结构                                                | 代码路径                        |
| ------------------------------------------------------- | ------------------------------- |
| DEFINE_TRACE(name)<br/>DECLARE_TRACE(name, proto, args) | include/linux/tracepoint.h      |
| struct tracepoint                                       | include/linux/tracepoint-defs.h |

1.  Tracepoint依次执行桩函数，每个桩函数实现不同的debug功能。内核通过register_trace\_\##name将桩函数添加到Tracepoint中，通过unregister_trace_##name从trace point中移除。（注：##表示字符串连接）
2.  Tracepoint依次执行桩函数，每个桩函数实现不同的debug功能。内核通过register_trace\_\##name将桩函数添加到Tracepoint中，通过unregister_trace_##name从trace point中移除。（注：##表示字符串连接）

```
tracepoint {
    const char *name;       /* Tracepoint name */
    struct static_key key;
    int (*regfunc)(void);
    void (*unregfunc)(void);
    struct tracepoint_func __rcu *funcs;
};
```

-   name：Tracepoint的名字，内核中通过hash表管理所有的trace point，找到对应的hash slot后，需要通过name来识别具体的trace_point
-   key：Tracepoint状态，1表示disable，0表示enable
-   regfunc：添加桩函数的函数
-   unregfunc：卸载桩函数的函数
-   funcs：Tracepoint中所有的桩函数链表

3.  内核通过#define DECLARE_TRACE(name, proto, args)定义trace point用到的函数，定义的函数原型如下（从代码中摘取了几个，不止以下3个）：

```
#define __DECLARE_TRACE(name, proto, args, cond, data_proto, data_args)
extern struct tracepoint __tracepoint_##name;
static inline void trace_##name(proto)
{
    if (static_key_false(&__tracepoint_##name.key))
        __DO_TRACE(&__tracepoint_##name,
        TP_PROTO(data_proto),
        TP_ARGS(data_args),
        TP_CONDITION(cond),,);
}
register_trace_##name(void (*probe)(data_proto), void *data)
{      
    return tracepoint_probe_register(&__tracepoint_##name,
    (void *)probe, data);
}
unregister_trace_##name(void (*probe)(data_proto), void *data)
{
    return tracepoint_probe_unregister(&__tracepoint_##name,(void *)probe, data);
}
```

第2行声明一个外部trace point变量。然后定义了一些trace point用到的公共函数。

第5行判断trace point是否disable，如果没有disable，那么调用__DO_TRACE遍历执行trace point中的桩函数（通过“函数指针”来实现执行桩函数）。

trace point提供了统一的框架，用void *指向任何函数，所以各个trace point取出桩函数指针后，需要转换成自己的函数指针类型，TP_PROTO(data_proto)传递函数指针类型用于转换，具体的转换在：

```
#define __DO_TRACE(tp, proto, args, cond, rcuidle)
do {
    struct tracepoint_func *it_func_ptr;
    void *it_func;
    void *__data;
    ……
    it_func_ptr = rcu_dereference_raw((tp)->funcs);
    if (it_func_ptr) {
        do {
            it_func = (it_func_ptr)->func;
            __data = (it_func_ptr)->data;
            ((void(*)(proto))(it_func))(args);
        } while ((++it_func_ptr)->func);
    }

    ……
}while(0)
```

4.  桩函数的proto的传递的例子

```
DEFINE_EVENT_CONDITION(f2fs__submit_page_bio, f2fs_submit_page_write,
    TP_PROTO(struct page *page, struct f2fs_io_info *fio),
    TP_ARGS(page, fio),
    TP_CONDITION(page->mapping)
);
```

第2行声明了桩函数原型

```
#define DEFINE_EVENT_CONDITION(template, name, proto, args, cond)
     DEFINE_EVENT(template, name, PARAMS(proto), PARAMS(args))
```

```
#define DEFINE_EVENT_CONDITION(template, name, proto, args, cond)
     DEFINE_EVENT(template, name, PARAMS(proto), PARAMS(args))
```

```
#define DECLARE_TRACE(name, proto, args)
    __DECLARE_TRACE(name, PARAMS(proto), PARAMS(args),             
    cpu_online(raw_smp_processor_id()),             
    PARAMS(void *__data, proto),                   
    PARAMS(__data, args))
```

至此执行到__DECLARE_TRACE宏，参考前面说明，提到了何时转换成桩函数指针类型。

从上面可以看出trace point的机制很简单，就是把用于debug的函数指针组织在一个struct trace point变量中，然后依次执行各个函数指针。不过为了避免各个模块重复写代码，内核用了比较复杂的宏而已。

另外也可以发现，使用trace point必须要通过register_trace_##name将桩函数（也就是我们需要的debug函数）添加到trace point中，这个工作只能通过moudule或者修改内核代码实现，对于开发者来说，操作比较麻烦。ftrace开发者们意识到了这点，所以提供了trace event功能，开发者不需要自己去注册桩函数了，易用性较好，后面文章会谈到trace event是如何实现的以及如何使用。

如果用户准备为kernel加入新的tracepoint，每个tracepoint则以下列格式声明：

```
#include <linux/tracepoint.h>
DECLARE_TRACE(tracepoint_name,
    TPPROTO(trace_function_prototype),
    TPARGS(trace_function_args));
```

上面的宏定义了一个新的tracepoint叫tracepoint_name。与这个tracepoint关联的probe函数必须与TPPROTO宏定义的函数prototype一致，probe函数的参数列表必须与TPARGS宏定义的一致。

或许用一个例子来解释会比较容易理解。Kernel里面已经包含了一些tracepoints，其中一个叫做sched_wakeup，这个tracepoint在每次scheduler唤醒一个进程时都会被调用。它是这样定义的：

```
DECLARE_TRACE(sched_wakeup,
    TPPROTO(struct rq *rq, struct task_struct *p),
    TPARGS(rq, p))
```

实际在kernel中插入这个tracepoint点的是一行如下代码：

```
trace_sched_wakeup(rq, p);
```

注意，插入tracepoint的函数名就是将trace_前缀添加到tracepoint_name的前面。除非有一个实际的probe函数关联到这个tracepoint，trace_sched_wakeup()这个只是一个空函数。下面的操作就是将一个probe函数关联到一个tracepoint：

```
void my_sched_wakeup_tracer(struct rq *rq, struct task_struct *p);
register_trace_sched_wakeup(my_sched_wakeup_tracer);
```

register_trace_sched_wakeup()函数实际上是DEFINE_TRACE()定义的，它把probe函数my_sched_wakeup_tracer()和tracepoint sched_wakeup关联起来



## 如何使用 TRACE_EVENT() 宏来创建跟踪点

### TRACE_EVENT() 宏的剖析

自动跟踪点具有各种必须满足的要求：

-   它必须创建一个可以放在内核代码中的跟踪点
-   它必须创建一个可以挂接到此跟踪点的回调函数
-   回调函数必须能够以最快的方式将传递给它的数据记录到跟踪环缓冲区中
-   它必须创建一个函数，可以解析记录到环形缓冲区的数据，并将其转换为跟踪程序可以显示给用户的可读格式

为此，TRACE_EVENT() 宏分为六个组件，它们对应于宏的参数:

```
TRACE_EVENT(name, proto, args,struct, assign, print)
```

-   name- 要创建的跟踪点的名称
-   prototype- 跟踪点回调函数的原型
-   args- 与函数原型匹配的参数
-   struct - 跟踪点可以使用（并不是必须的）这个结构来存储传入到跟踪点的数据。
-   assign- 将数据分配给结构的类似于C的方法
-   print- 以人类可读的ASCII格式输出结构的方法



**举例**
这里可以找到sched_switch跟踪点定义的一个很好的例子。下面将使用该定义来描述TRACE_EVENT()宏的每个部分

除第一个参数外，所有参数都用另一个宏（TP_PROTO、TP_ARGS、TP_STRUCT_uentry、TP_fast_assign和TP_printk）封装。这些宏在处理过程中提供了更多的控制，还允许在TRACE_EVENT()宏中使用逗号

**Name**
第一个参数是名称。

```
TRACE_EVENT(sched_switch
```

这是用于调用此跟踪点的名称。实际使用的跟踪点的名称前面有trace_前缀（即trace_sched_switch）

**Prototype**
下一个参数是原型。

```
TP_PROTO(struct rq *rq, struct task_struct *prev, struct task_struct *next)
```

原型的编写就像您要直接声明跟踪点一样：

trace_sched_switch(struct rq *rq, struct task_struct *prev, struct task_struct *next);

它用作添加到内核代码的跟踪点和回调函数的原型。请记住，跟踪点调用回调函数，就好像在跟踪点的位置调用回调函数一样

**Arguments**
第三个参数是原型使用的参数。

```
TP_ARGS(rq, prev, next)
```

这可能看起来很奇怪，但它不仅是TRACE_EVENT()宏所必需的，而且也是下面的跟踪点基础结构所必需的。跟踪点代码在激活时将调用回调函数（可以将多个回调分配给给定的跟踪点）。创建跟踪点的宏必须同时访问原型和参数。下面是跟踪点宏完成此操作所需的示例：

```
#define TRACE_POINT(name, proto, args) //\
        void trace_##name(proto)            \
        {                                   \
                if (trace_##name##_active)  \
                        callback(args);     \
        }
```

**Structure**

```
TP_STRUCT__entry(
	__array(	char,	prev_comm,	TASK_COMM_LEN	)
	__field(	pid_t,	prev_pid			)
	__field(	int,	prev_prio			)
	__field(	long,	prev_state			)
	__array(	char,	next_comm,	TASK_COMM_LEN	)
	__field(	pid_t,	next_pid			)
	__field(	int,	next_prio			)
),
```

此参数描述将存储在跟踪器环缓冲区中的数据的结构布局。结构的每个元素都由另一个宏定义。这些宏用于自动创建结构，而不是功能类似的。注意宏之间没有任何分隔符（没有逗号或分号）

sched_switch tracepoint使用的宏有：

-   \_\_field(type, name)- 它定义了一个普通的结构元素，比如int var；其中type是int，name是var

-   \_\_array(type, name, len)- 这将定义一个数组项，相当于int name[len]；其中type是int，数组的名称是array，数组中的项数是len

    还有其他元素宏将在后面的文章中介绍。来自sched_switch tracepoint的定义将生成一个如下所示的结构：

```
struct {
	char   prev_comm[TASK_COMM_LEN];
	pid_t  prev_pid;
	int    prev_prio;
	long   prev_state;
	char   next_comm[TASK_COMM_LEN];
	pid_t  next_pid;
	int    next_prio;
};
```



**Assignment**

第五个参数定义了将参数中的数据保存到环形缓冲区的方式

```
TP_fast_assign(
		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
		__entry->prev_pid	= prev->pid;
		__entry->prev_prio	= prev->prio;
		__entry->prev_state	= prev->state;
		memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
		__entry->next_pid	= next->pid;
		__entry->next_prio	= next->prio;
),
```

-   TP_fast_assign()中的代码是普通的C代码。一个特殊的变量 \_\_entry表示指向由TP_STRUCT__entry项定义的结构类型的指针，并直接指向环缓冲区。TP_fast_assign用于填充TP_STRUCT_entry中创建的所有字段。然后，可以使用TP_PROTO()和TP_ARGS()定义的参数的变量名来将适当的数据分配到该输入结构中



**Print**

最后一个参数定义如何使用printk()从TP_STRUCT_uentry结构中打印字段

```
TP_printk("prev_comm=%s prev_pid=%d prev_prio=%d prev_state=%s ==> " \
 		  "next_comm=%s next_pid=%d next_prio=%d",
		__entry->prev_comm, __entry->prev_pid, __entry->prev_prio,
		__entry->prev_state ?
		  __print_flags(__entry->prev_state, "|",
				{ 1, "S"} , { 2, "D" }, { 4, "T" }, { 8, "t" },
				{ 16, "Z" }, { 32, "X" }, { 64, "x" },
				{ 128, "W" }) : "R",
		__entry->next_comm, __entry->next_pid, __entry->next_prio)
```

变量 \_\_entry 再次被用来引用包含数据的结构体的指针。Format string 就像其他 printf 的 format 一样。__print_flags() 是 TRACE_EVENT() 附带的一组帮助函数的一部分，不要创建新的跟踪点特定的帮助程序，因为这会混淆用户空间工具，这些工具知道TRACE_EVENT() helper宏，但不知道如何处理为单个跟踪点创建的宏



**头文件**

TRACE_EVENT()宏不能仅仅放置在期望它能与Ftrace或任何其他跟踪程序一起工作的任何位置。包含TRACE_EVENT()宏的头文件必须遵循特定格式。这些头文件通常位于include/trace/events目录中，但不需要。如果它们不在此目录中，则需要其他配置

TRACE_EVENT()头中的第一行不是普通的ifdef_TRACE_SCHED_H，而是

此示例用于调度程序跟踪事件，其他事件头将使用除sched和trace_sched_H之外的其他内容。trace_HEADER_MULTI_READ test允许此文件包含多次；这对于trace_event()宏的处理非常重要。还必须为文件定义跟踪系统，并且跟踪系统必须在if的保护范围之外。TRACE_系统定义文件中TRACE_EVENT()宏所属的组。这也是事件将在debugfs tracing/events目录中分组的目录名。此分组对Ftrace很重要，因为它允许用户按组启用或禁用事件

然后，该文件包含TRACE_EVENT()宏内容所需的任何头。（例如，include<linux/sched.h>）。tracepoint.h文件是必需的

```
#include <linux/tracepoint.h>
```

现在可以使用trace_EVENT()宏定义所有跟踪事件。请在TRACE_EVENT()宏上方包含描述跟踪点的注释。以include/trace/events/sched.h为例。文件结尾为：

```
#endif /* _TRACE_SCHED_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
```

define_trace.h是创建跟踪点的所有魔力所在，这个文件必须包含在跟踪头文件的底部，而不受endif的保护



### 使用tracepoint

如果跟踪点不在任何地方使用，那么定义它就没有意义。要使用跟踪点，必须包含跟踪头，但在包含跟踪之前，还必须定义一个C文件（并且只有一个）的创建跟踪点。这将导致define_trace.h创建生成跟踪事件所需的必要函数。在kernel/sched.c中定义了以下内容：

```
#define CREATE_TRACE_POINTS
#include <trace/events/sched.h>
```

如果另一个文件需要使用跟踪文件中定义的跟踪点，则它只需要包含跟踪文件，而不需要定义创建跟踪点。为同一头文件定义多次将在生成时导致链接器错误。例如，在kernel/fork.c中，只包含头文件：

```
#include <trace/events/sched.h>
```

最后，在代码中使用跟踪点，就像在TRACE_EVENT()宏中定义的一样：

```
static inline void
context_switch(struct rq *rq, struct task_struct *prev,
				struct task_struct *next)
{
	struct mm_struct *mm, *oldmm;
	
	prepare_task_switch(rq, prev, next);
	trace_sched_switch(rq, prev, next);
	mm = next->mm;
	oldmm = prev->active_mm;
```



### 实际使用

-   module_traceevent.h

```
#undef TRACE_SYSTEM
#define TRACE_SYSTEM xhr_test

#if !defined(_TRACE_TE_TEST_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TE_TEST_H

#include <linux/tracepoint.h>

TRACE_EVENT(te_test,
    TP_PROTO(int num),
    TP_ARGS(num),
    TP_STRUCT__entry(
        __field(int, output)
        __field(int, count)
    ),
    TP_fast_assign(
        __entry->count++;
        __entry->output = num;
    ),
    TP_printk("count=%d output=%d",
        __entry->count, __entry->output)
);

#endif /* _TRACE_TE_TEST_H */
    
/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE module_traceevent

#include <trace/define_trace.h>
```



-   module_entry.c

```
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>

#define CREATE_TRACE_POINTS
#include "module_traceevent.h"

static int xhr_thread(void *arg)
{
    static unsigned long count;

    while (!kthread_should_stop())
    {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(HZ);
        trace_te_test(count);
        count++;
    }

    return 0;
}

static struct task_struct *xhr_task;

static __init int __module_init(void)
{
    printk("Hello, %s.\n", __func__);

    xhr_task = kthread_run(xhr_thread, NULL, "xhr-thread");
    if (IS_ERR(xhr_task))
        return -1;

    return 0;
}
static __exit void __module_exit(void)
{
    kthread_stop(xhr_task);
    printk("Hello, %s.\n", __func__);
    return;
}

module_init(__module_init);
module_exit(__module_exit);
MODULE_LICENSE("GPL");
```

-   Makefile

```
INCLUDES = -I. -I$(KDIR)/include
MODULE := xhr
obj-m := $(MODULE).o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

CFLAGS_module_entry.o = -I$(src)

.PHONY: all
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

.PHONY: clean
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

$(MODULE)-objs += module_entry.o
```

-   user space

```
sudo -s
insmod xhr.ko
cd /sys/kernel/debug/tracing/
echo 1 > tracing_on
echo 1 > events/xhr_test/te_test/enable
cat trace

output:
# entries-in-buffer/entries-written: 8/8   #P:8
#
#                              _-----=> irqs-off
#                             / _----=> need-resched
#                            | / _---=> hardirq/softirq
#                            || / _--=> preempt-depth
#                            ||| /     delay
#           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |       |   ||||       |         |
      xhr-thread-3732  [000] ....  1739.034184: te_test: count=1 output=48
      xhr-thread-3732  [000] ....  1740.058510: te_test: count=1 output=49
      xhr-thread-3732  [000] ....  1741.081934: te_test: count=1 output=50
      xhr-thread-3732  [000] ....  1742.106095: te_test: count=1 output=51
```

