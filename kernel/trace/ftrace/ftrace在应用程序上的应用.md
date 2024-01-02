之前介绍通过命令行配置和使用ftrace功能，但是实际中，我们也会希望抓C/C++程序中某段代码的调度情况。笔者前不久就遇到这种问题，某个函数调用时延概率超过100ms，是为什么？这时候就需要在他们代码中使能ftrace抓执行此函数时候，任务的调度情况。

观察某段代码执行过程中的情况，ftrace提供了trace markers功能，可通过写入trace_marker接口在ftrace中留下记录。如下

```
[tracing]# echo hello world > trace_marker
    [tracing]# cat trace
    # tracer: nop
    #
    #           TASK-PID    CPU#    TIMESTAMP  FUNCTION
    #              | |       |          |         |
               <...>-3718  [001]  5546.183420: 0: hello world
```

利用tracing_on和trace_marker接口，可以很好的trace任务的执行情况（前提是任务源码可见）。

```
int main()
{
    int fd_mark = open("/sys/kernel/debug/tracing/trace_marker", O_CREAT|O_RDWR, 0666);
    int fd_trace = open("/sys/kernel/debug/tracing/tracing_on", O_CREAT|O_RDWR, 0666);
    /* enable trace */
    write(fd_trace, "1", 2);
    /* add trace mark */
    write(fd_mark, "start time", 11);
    /* Run something */

    /* add trace mark */
    write(fd_mark, "end time", 11);
    /* disable trace */
    write(fd_trace, "0", 2);
    close(fd_mark);
    close(fd_trace);
}
```

