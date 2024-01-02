Ftrace设计作为一个内部的tracer提供给系统的开发者和设计者，帮助他们弄清kernel正在发生的行为，它能够调式分析延迟和性能问题。对于前一章节，我们学习了Ftrace发展到现在已经不仅仅是作为一个function tracer了，它实际上成为了一个通用的trace工具的框架

- 一方面已经从function tracer扩展到irqsoff tracer、preemptoff tracer
- 另一方面静态的trace event也成为trace的一个重要组成部分

通过前面两节的学习，我们知道了什么是ftrace，能够解决什么问题，从这章开始我们主要是学习，怎么去使用ftreace解决问题。

## 1 ftrace基础用法

ftrace 通过 debugfs 向用户态提供访问接口。配置内核时激活 debugfs 后会创建目录 /sys/kernel/debug ，debugfs 文件系统就是挂载到该目录。要挂载该目录，需要将如下内容添加到 /etc/fstab 文件：

```
debugfs  /sys/kernel/debug  debugfs  defaults  0  0 1
```

或者可以在运行时挂载：

```
mount  -t  debugfs  debugfs  /sys/kernel/debug
```

激活内核对 ftrace 的支持后会在 debugfs 下创建一个 tracing 目录 /sys/kernel/debug/tracing 。该目录下包含了 ftrace 的控制和输出文件

```
root@100ask:/sys/kernel/debug/tracing# ls
available_events events README snapshot trace_pipe
available_filter_functions free_buffer saved_cmdlines stack_max_size trace_stat
available_tracers function_profile_enabled saved_cmdlines_size stack_trace tracing_cpumask
buffer_percent hwlat_detector saved_tgids stack_trace_filter tracing_max_latency
buffer_size_kb instances set_event synthetic_events tracing_on
buffer_total_size_kb kprobe_events set_event_pid timestamp_mode tracing_thresh
current_tracer kprobe_profile set_ftrace_filter trace uprobe_events
dynamic_events max_graph_depth set_ftrace_notrace trace_clock uprobe_profile
dyn_ftrace_total_info options set_ftrace_pid trace_marker
enabled_functions per_cpu set_graph_function trace_marker_raw
error_log printk_formats set_graph_notrace trace_options
```

其中重点关注以下文件

- trace查看选择器

查看支持的跟踪器available_tracers

> root@100ask:/sys/kernel/debug/tracing# cat available_tracers</br>
> hwlat blk mmiotrace function_graph wakeup_dl wakeup_rt wakeup function nop

| 类型             | 含义                                                                            |
| -------------- | ----------------------------------------------------------------------------- |
| function       | 函数调用追踪器，可以看出哪个函数何时调用，可以通过过滤器指定要跟踪的函数                                          |
| function_graph | 函数调用图表追踪器，可以看出哪个函数被哪个函数调用，何时返回                                                |
| blk            | block I/O追踪器,blktrace用户应用程序 使用的跟踪器                                            |
| mmiotrace      | MMIO(Memory Mapped I/O)追踪器，用于Nouveau驱动程序等逆向工程                                 |
| wakeup         | 跟踪进程唤醒信息，进程调度延迟追踪器                                                            |
| wakeup_rt      | 与wakeup相同，但以实时进程为对象                                                           |
| nop            | 不会跟踪任何内核活动，将 nop 写入 current_tracer 文件可以删除之前所使用的跟踪器，并清空之前收集到的跟踪信息，即刷新 trace 文件 |
| wakeup_dl      | 跟踪并记录唤醒SCHED_DEADLINE任务所需的最大延迟（如"wakeup”和"wakeup_rt”一样）                       |
| mmiotrace      | 一种特殊的跟踪器，用于跟踪二进制模块。它跟踪模块对硬件的所有调用                                              |
| hwlat          | 硬件延迟跟踪器。它用于检测硬件是否产生任何延迟                                                       |

查看当前的跟踪器current_tracer ，可以echo选择

> root@100ask:/sys/kernel/debug/tracing# cat current_tracer </br>
> nop

- trace使能

tracing_on ：是否往循环buffer写跟踪记录，可以echo设置

> root@100ask:/sys/kernel/debug/tracing# cat tracing_on </br>
> 1

- trace过滤器选择（可选）
  
  - set_ftrace_filter/set_graph_notrace：（function跟踪器）函数过滤器，echo xxx设置要跟踪的函数，
    
    > root@100ask:/sys/kernel/debug/tracing# cat set_ftrace_filter </br>
    > all functions enabled

- trace数据读取
  
  - trace：可以cat读取跟踪记录的buffer内容（查看的时候会临时停止跟踪）
  - trace_pipe：类似trace可以动态读取的流媒体文件（差异是每次读取后，再读取会读取新内容）

所以对于ftrace的三步法为：

- 1 设置tracer类型
- 2 设置tracer参数
- 3 使能tracer

### 1.2 function trace实例

function，函数调用追踪器， 跟踪函数调用，默认跟踪所有函数，如果设置set_ftrace_filter， 则跟踪过滤的函数，可以看出哪个函数何时调用。

- available_filter_functions：列出当前可以跟踪的内核函数，不在该文件中列出的函数，无法跟踪其活动

- enabled_functions：显示有回调附着的函数名称。

- function_profile_enabled：打开此选项，在trace_stat中就会显示function的统计信息。

- set_ftrace_filter：用于指定跟踪的函数

- set_ftrace_notrace：用于指定不跟踪的函数

- set_ftrace_pid：用于指定要跟踪特定进程的函数

**Disable tracer：**

> echo 0 > tracing_on

设置 tracer 类型为 function：

> echo function > current_tracer

set_ftrace_filter 表示要跟踪的函数，这里我们只跟踪 dev_attr_show 函数：

> echo dev_attr_show > set_ftrace_filter

Enable tracer：

> echo 1 > tracing_on

提取trace结果：

![](https://pic4.zhimg.com/80/v2-fa1931d45a9600ffcafc63642c8b5bcf_720w.jpg)

**从上图可以看到 function trace 一个函数的方法基本就是三板斧：**

- 设置 current_tracer 为 function
- 设置要 trace 的函数
- 打开 trace 开关，开始 trace
- 从 trace 信息我们可以获取很多重要信息：
- 进程信息，TASK-PID，对应任务的名字
- 进程运行的 CPU
- 执行函数时的系统状态，包括中断，抢占等状态信息
- 执行函数的时间辍，字段 TIMESTAMP 是时间戳，其格式为“.”，表示执行该函数时对应的时间戳
- FUNCTION 一列则给出了被跟踪的函数，函数的调用者通过符号 “<-” 标明，这样可以观察到函数的调用关系。

function 跟踪器可以跟踪内核函数的调用情况，可用于调试或者分析 bug ，还可用于了解和观察 Linux 内核的执行过程。同时ftrace允许你对一个特定的进程进行跟踪，在/sys/kernel/debug/tracing目录下，文件set_ftrace_pid的值要更新为你想跟踪的进程的PID。

```text
echo $PID > set_ftrace_pid
```

### 1.3 function_graph Trace 实例

function_graph 跟踪器则可以提供类似 C 代码的函数调用关系信息。通过写文件 set_graph_function 可以显示指定要生成调用关系的函数，缺省会对所有可跟踪的内核函数生成函数调用关系图。

函数图跟踪器对函数的进入与退出进行跟踪，这对于跟踪它的执行时间很有用。函数执行时间超过10微秒的标记一个“+”号，超过1000微秒的标记为一个“！”号。通过echo function_graph > current_tracer可以启用函数图跟踪器。

与 function tracer 类似，设置 function_graph 的方式如下：

设置 tracer 类型为 function_graph：

> echo function_graph > current_tracer

set_graph_function 表示要跟踪的函数：

> echo __do_fault > set_graph_function  
> echo 1 > tracing_on

**捕捉到的 trace 内容**

![](https://pic4.zhimg.com/80/v2-9f1c9168725f5d84ae7e398ed878fc07_720w.jpg)

我们跟踪的是 __do_fault 函数，但是 function_graph tracer 会跟踪函数内的调用关系和函数执行时间，可以协助我们确定代码执行流程。比如一个函数内部执行了很多函数指针，不能确定到底执行的是什么函数，可以用 function_graph tracer 跟踪一下。

- CPU 字段给出了执行函数的 CPU 号，本例中都为 1 号 CPU。
- DURATION 字段给出了函数执行的时间长度，以 us 为单位。
- FUNCTION CALLS 则给出了调用的函数，并显示了调用流程。

### 1.4 wakeup

wakeup tracer追踪普通进程从被唤醒到真正得到执行之间的延迟。

![](https://pic1.zhimg.com/80/v2-2f3765f8ce399b4d0d568f624f832fdc_720w.jpg)

### **1.5 wakeup-rt**

non-RT进程通常看平均延迟。RT进程的最大延迟非常有意义，反应了调度器的性能

## 二，trace event 用法

### **2.1 trace event 简介**

trace event 就是利用 ftrace 框架，实现低性能损耗，对执行流无影响的一种信息输出机制。相比 printk，trace event：

- 不开启没有性能损耗
- 开启后不影响代码流程
- 不需要重新编译内核即可获取 debug 信息

### **2.2 使用实例**

上面提到了 function 的 trace，在 ftrace 里面，另外用的多的就是 event 的 trace，我们可以在 events 目录下面看支持那些事件：

![](https://pic4.zhimg.com/80/v2-57c3e82ac17710a758d2fff4826460eb_720w.jpg)

上面列出来的都是分组的，我们可以继续深入下去，譬如下面是查看 sched 相关的事件

![](https://pic4.zhimg.com/80/v2-f624e6f156620ed652aba9046ebd823b_720w.jpg)

对于某一个具体的事件，我们也可以查看：

![](https://pic1.zhimg.com/80/v2-2bf04acdb17143935c2b4959a498b9d0_720w.jpg)

上述目录里面，都有一个 enable 的文件，我们只需要往里面写入 1，就可以开始 trace 这个事件。譬如下面就开始 trace sched_wakeup 这个事件：

![](https://pic2.zhimg.com/80/v2-87dd1de27862c30537e724581bdaad69_720w.jpg)

我们也可以 trace sched 里面的所有事件：

![](https://pic2.zhimg.com/80/v2-e20aff1449eccb13b7bb6d17bf1e9fed_720w.jpg)

## 三，高级技巧

### **查看函数调用栈**

查看函数调用栈是内核调试最最基本得需求，常用方法：

- 函数内部添加 WARN_ON(1)
- ftrace

trace 函数的时候，设置 echo 1 > options/func_stack_trace 即可在 trace 结果中获取追踪函数的调用栈。

**以 dev_attr_show 函数为例，看看 ftrace 如何帮我们获取调用栈：**

```text
#cd /sys/kernel/debug/tracing
#echo 0 > tracing_on
#echo function > current_tracer
#echo schedule > set_ftrace_filter
// 设置 func_stack_trace
#echo 1 > options/func_stack_trace
#echo 1 > tracing_on
```

![](https://pic3.zhimg.com/v2-9dbb1f48c681d879dfc7dc0767002606_r.jpg)

**如何跟踪一个命令，但是这个命令执行时间很短**

我们可以设置ftrace过滤器控制相关文件：

- set_ftrace_filter function tracer ：只跟踪某个函数
- set_ftrace_notrace function tracer ：不跟踪某个函数
- set_graph_function function_graph tracer ：只跟踪某个函数
- set_graph_notrace function_graph tracer ：不跟踪某个函数
- set_event_pid trace event ：只跟踪某个进程
- set_ftrace_pid function/function_graph tracer ：只跟踪某个进程

如果这时候问：如何跟踪某个进程内核态的某个函数？

答案是肯定的，将被跟踪进程的 pid 设置到 set_event_pid/set_ftrace_pid 文件即可。

但是如果问题变成了，我要调试 kill 的内核执行流程，如何办呢？

因为 kill 运行时间很短，我们不能知道它的 pid，所以就没法过滤了。

调试这种问题的小技巧，即 脚本化，这个技巧在很多地方用到：

```text
sh -c "echo $$ > set_ftrace_pid; echo 1 > tracing_on; kill xxx; echo 0 > tracing_on"
```

**如何跟踪过滤多个进程？多个函数？**

- 函数名雷同，可以使用正则匹配

```text
# cd /sys/kernel/debug/tracing
# echo 'dev_attr_*' > set_ftrace_filter
# cat set_ftrace_filter
dev_attr_store
dev_attr_show
```

- 追加某个函数

用法为：echo xxx >> set_ftrace_filter，例如，先设置 dev_attr_*：

```text
# cd /sys/kernel/debug/tracing
# echo 'dev_attr_*' > set_ftrace_filter
# cat set_ftrace_filter
dev_attr_store
dev_attr_show
```

再将 ip_rcv 追加到跟踪函数中：

```text
# cd /sys/kernel/debug/tracing
# echo ip_rcv >> set_ftrace_filter
# cat set_ftrace_filter
dev_attr_store
dev_attr_show
ip_rcv
```

基于模块过滤

格式为：<function>:<command>:<parameter>，例如，过滤 ext3 module 的 write* 函数：

```text
$ echo 'write*:mod:ext3' > set_ftrace_filter
```

从过滤列表中删除某个函数，使用“感叹号”

感叹号用来移除某个函数，把上面追加的 ip_rcv 去掉：

```text
# cd /sys/kernel/debug/tracing
# cat set_ftrace_filter
dev_attr_store
dev_attr_show
ip_rcv
# echo '!ip_rcv' >> set_ftrace_filter
# cat set_ftrace_filter
dev_attr_store
```

## 四，前端工具

我们可以手工操作/sys/kernel/debug/tracing路径下的大量的配置文件接口，来使用ftrace的强大功能。但是这些接口对普通用户来说太多太复杂了，我们可以使用对ftrace功能进行二次封装的一些命令来操作。

trace-cmd就是ftrace封装命令其中的一种。该软件包由两部分组成

- trace-cmd：提供了数据抓取和数据分析的功能
- kernelshark：可以用图形化的方式来详细分析数据，也可以做数据抓取

### 4.1 trace-cmd

下载编译ARM64 trace-cmd方法：

```text
git clone [https://github.com/rostedt/trace-cmd.git](https://github.com/rostedt/trace-cmd.git)
export CROSS_COMPILE=aarch64-linux-gnu-
export ARCH=arm64
make
```

![](https://pic1.zhimg.com/80/v2-637b4ca183b521c71db06510236df7e0_720w.jpg)

先通过 record 子命令将结果记录到 trace.dat，再通过 report 命令进行结果提取。命令解释：

- -p：指定当前的 tracer，类似 echo function > current_tracer，可以是支持的 tracer 中的任意一个
- -l：指定跟踪的函数，可以设置多个，类似 echo function_name > set_ftrace_filter
- --func-stack：记录被跟踪函数的调用栈

**在很有情况下不能使用函数追踪，需要依赖 事件追踪 的支持，例如：**

![](https://pic1.zhimg.com/80/v2-75d957ee457de579980342ff0a69d848_720w.jpg)

### **4.2 kernelshark图形化分析数据**

trace-cmd report主要是使用统计的方式来找出热点。如果要看vfs_read()一个具体的调用过程，除了使用上一节的trace-cmd report命令，还可以使用kernelshark图形化的形式来查看，可以在板子上使用trace-cmd record 记录事件，把得到的trace.data放到linux 桌面系统，用kernelshark打开，看到图形化的信息
