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



