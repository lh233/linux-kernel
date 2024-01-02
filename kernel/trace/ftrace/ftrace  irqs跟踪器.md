当中断被关闭（俗称关中断)了，CPU就不能响应其他的事件，如果这时有一个鼠标中断，要在下一次开中断时才能响应这个鼠标中断，这段延迟称为中断延迟。向current_tracer 文件写入 irqsoff字符串即可打开irqsoff来跟踪中断延迟。

```
[root@linux tracing]# pwd 
/sys/kernel/debug/tracing 
[root@linux tracing]# echo 0 > tracing_on 
[root@linux tracing]# echo sched_switch > current_tracer 
[root@linux tracing]# echo 1 > tracing_on
# 让内核运行一段时间，这样 ftrace 可以收集一些跟踪信息，之后再停止跟踪
 
[root@linux tracing]# echo 0 > tracing_on
[root@linux tracing]# cat trace | head -10 
 
# 让内核运行一段时间，这样 ftrace 可以收集一些跟踪信息，之后再停止跟踪
 
[root@linux tracing]# echo 0 > tracing_enabled 
[root@linux tracing]# cat trace | head -35 
# tracer: irqsoff 
# 
# irqsoff latency trace v1.1.5 on 2.6.33.1 
# -------------------------------------------------------------------- 
# latency: 34380 us, #6/6, CPU#1 | (M:desktop VP:0, KP:0, SP:0 HP:0 #P:2) 
#    ----------------- 
#    | task: -0 (uid:0 nice:0 policy:0 rt_prio:0) 
#    ----------------- 
#  => started at: reschedule_interrupt 
#  => ended at:   restore_all_notrace 
# 
# 
#                  _------=> CPU#            
#                 / _-----=> irqs-off        
#                | / _----=> need-resched    
#                || / _---=> hardirq/softirq 
#                ||| / _--=> preempt-depth   
#                |||| /_--=> lock-depth       
#                |||||/     delay             
#  cmd    pid    |||||| time  |   caller      
#    \   /       ||||||   \   |   /           
 <idle>-0       1dN... 4285us!: trace_hardirqs_off_thunk <-reschedule_interrupt 
 <idle>-0       1dN... 34373us+: smp_reschedule_interrupt <-reschedule_interrupt 
 <idle>-0       1dN... 34375us+: native_apic_mem_write <-smp_reschedule_interrupt 
 <idle>-0       1dN... 34380us+: trace_hardirqs_on_thunk <-restore_all_notrace 
 <idle>-0       1dN... 34384us : trace_hardirqs_on_caller <-restore_all_notrace 
 <idle>-0       1dN... 34384us : <stack trace> 
=> trace_hardirqs_on_thunk 
[root@linux tracing]# cat tracing_max_latency 
34380

```

