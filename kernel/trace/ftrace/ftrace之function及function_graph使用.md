## 一 用途

**（1）function 主要用于跟踪内核函数的调用栈（其被调用过程）**

**（2）function_graph 主要用于跟踪内核函数内部调用流程及耗时**

**这两个对内核性能分析的作用不大，主要用来梳理内核模块的逻辑**

## 二 使用

（1）function 使用

```
 
/sys/kernel/debug/tracing # echo nop > current_tracer           ----清空跟踪器
/sys/kernel/debug/tracing # echo drm_open > set_ftrace_filter   ----设置跟踪函数为drm_open
/sys/kernel/debug/tracing # echo function > current_tracer      ----设置当前跟踪器
/sys/kernel/debug/tracing # echo 1 > options/func_stack_trace   ----跟踪函数调用栈
/sys/kernel/debug/tracing # echo 1 > tracing_on                 ----开始跟踪
/sys/kernel/debug/tracing # /mnt/vkms_test ---运行vkms_test程序，其会调用到内核drm_open函数
/sys/kernel/debug/tracing # echo 0 > tracing_on                 ----关闭跟踪
/sys/kernel/debug/tracing # cat trace                           ----查看跟踪结果
# tracer: function
#
# entries-in-buffer/entries-written: 2/2   #P:4
#
#                                _-----=> irqs-off
#                               / _----=> need-resched
#                              | / _---=> hardirq/softirq
#                              || / _--=> preempt-depth
#                              ||| /     delay
#           TASK-PID     CPU#  ||||   TIMESTAMP  FUNCTION
#              | |         |   ||||      |         |
       vkms_test-90      [001] ....  5408.290697: drm_open <-drm_stub_open
       vkms_test-90      [001] ....  5408.291126: <stack trace>
 => drm_stub_open
 => chrdev_open
 => do_dentry_open
 => vfs_open
 => path_openat
 => do_filp_open
 => do_sys_openat2
 => do_sys_open
 => sys_open
 => ret_fast_syscall
 => 0xbebc3cdc
/sys/kernel/debug/tracing # 
```

shell脚本：

```
/mnt # cat trace_fun.sh 
# !/bin/sh
 
 
if [ $# -lt 2 ]; then
    echo "usage:"
    echo "  sh trace_func.sh function exec_file"
    exit -1
fi
 
cd /sys/kernel/debug/tracing/
echo nop > current_tracer
echo $1 > set_ftrace_filter
echo function > current_tracer
echo 1 > options/func_stack_trace
echo 1 > tracing_on
$2
echo 0 > tracing_on
cat trace
 
cd -
```

（2）function_graph使用

```cobol
/sys/kernel/debug/tracing # echo nop > current_tracer
/sys/kernel/debug/tracing # echo function_graph > current_tracer
/sys/kernel/debug/tracing # echo drm_open > set_graph_function
/sys/kernel/debug/tracing # echo 1 > options/funcgraph-tail ----增加函数尾部注释
/sys/kernel/debug/tracing # echo > set_ftrace_filter        ----清空，否则无法显示调用栈
/sys/kernel/debug/tracing # echo 1 > tracing_on 
/sys/kernel/debug/tracing # /mnt/vkms_test 
/sys/kernel/debug/tracing # echo 0 > tracing_on
/sys/kernel/debug/tracing # cat trace
# tracer: function_graph
#
# CPU  DURATION                  FUNCTION CALLS
# |     |   |                     |   |   |   |
 2)               |  drm_open() {
 2)               |    drm_minor_acquire() {
 2)   8.625 us    |      _raw_spin_lock_irqsave();
 2)   4.709 us    |      _raw_spin_unlock_irqrestore();
 2)               |      drm_dev_enter() {
 2)   5.750 us    |        __srcu_read_lock();
 2) + 17.167 us   |      } /* drm_dev_enter */
 2)   5.791 us    |      __srcu_read_unlock();
 2) + 75.458 us   |    } /* drm_minor_acquire */
 2)   6.833 us    |    __drm_dbg();
 2)               |    drm_file_alloc() {
 2)               |      kmem_cache_alloc_trace() {
 2)   5.583 us    |        should_failslab();
 2)               |        __slab_alloc.constprop.19() {
 2)               |          ___slab_alloc.constprop.20() {
 2)   3.583 us    |            _raw_spin_lock();
 2)               |            alloc_debug_processing() {
 2)               |              check_slab() {
 2)   4.750 us    |                slab_pad_check.part.8();
 2) + 12.375 us   |              } /* check_slab */
 2)               |              check_object() {
 2)   4.250 us    |                check_bytes_and_report();
 2)   4.166 us    |                check_bytes_and_report();
 2)   3.709 us    |                check_bytes_and_report();
 2)   3.250 us    |                check_bytes_and_report();
 2)   3.417 us    |                check_bytes_and_report();
 2) + 39.792 us   |              } /* check_object */
 2)               |              set_track() {
 2)               |                stack_trace_save() {
 2)               |                  save_stack_trace() {
 2)               |                    __save_stack_trace() {
 2)   3.042 us    |                      save_trace();
 2)   2.958 us    |                      save_trace();
 2)   2.959 us    |                      save_trace();
 2)   2.959 us    |                      save_trace();
 2)   2.958 us    |                      save_trace();
 2)   3.000 us    |                      save_trace();
 2)   3.166 us    |                      save_trace();
 2)   3.042 us    |                      save_trace();
 2)   3.000 us    |                      save_trace();
 2)   3.000 us    |                      save_trace();
 2)   3.042 us    |                      save_trace();
 2)   3.041 us    |                      save_trace();
 2)   3.041 us    |                      save_trace();
 2)   3.000 us    |                      save_trace();
 2)   3.125 us    |                      save_trace();
 2)   3.000 us    |                      save_trace();
 2)   3.042 us    |                      save_trace();
 2)   3.042 us    |                      save_trace();
 2)   3.042 us    |                      save_trace();
 2)   3.083 us    |                      save_trace();
 2) ! 125.875 us  |                    } /* __save_stack_trace */
 2) ! 132.041 us  |                  } /* save_stack_trace */
 2) ! 138.375 us  |                } /* stack_trace_save */
 2) ! 145.208 us  |              } /* set_track */
 2)   4.375 us    |              init_object();
 2) ! 218.625 us  |            } /* alloc_debug_processing */
 2)               |            deactivate_slab() {
 2)   4.042 us    |              _raw_spin_lock();
 2) + 16.833 us   |            } /* deactivate_slab */
 2) ! 257.708 us  |          } /* ___slab_alloc.constprop.20 */
 2) ! 272.709 us  |        } /* __slab_alloc.constprop.19 */
 2) ! 319.792 us  |      } /* kmem_cache_alloc_trace */
 2)               |      capable() {
 2)               |        ns_capable_common() {
 2)   6.500 us    |          cap_capable();
 2) + 20.375 us   |        } /* ns_capable_common */
 2) + 40.959 us   |      } /* capable */
 2)   5.958 us    |      __mutex_init();
 2)   6.792 us    |      __init_waitqueue_head();
 2)   5.791 us    |      __mutex_init();
 2)   7.084 us    |      drm_gem_open();
 2)               |      drm_prime_init_file_private() {
 2)   6.958 us    |        __mutex_init();
 2) + 32.208 us   |      } /* drm_prime_init_file_private */
 2) ! 492.709 us  |    } /* drm_file_alloc */
 2)               |    drm_master_open() {
 2)   7.667 us    |      mutex_lock();
 2)               |      drm_new_set_master() {
 2)               |        drm_master_create() {
 2)               |          kmem_cache_alloc_trace() {
 2)   5.709 us    |            should_failslab();
 2)               |            __slab_alloc.constprop.19() {
 2)               |              ___slab_alloc.constprop.20() {
 2)   3.792 us    |                _raw_spin_lock();
 2)               |                alloc_debug_processing() {
 2)               |                  check_slab() {
 2)   3.417 us    |                    slab_pad_check.part.8();
 2) + 10.291 us   |                  } /* check_slab */
 2)               |                  check_object() {
 2)   3.834 us    |                    check_bytes_and_report();
 2)   3.208 us    |                    check_bytes_and_report();
 2)   3.958 us    |                    check_bytes_and_report();
 2)   3.375 us    |                    check_bytes_and_report();
 2)   3.375 us    |                    check_bytes_and_report();
 2) + 53.583 us   |                  } /* check_object */
 2)               |                  set_track() {
 2)               |                    stack_trace_save() {
 2)               |                      save_stack_trace() {
 2)               |                        __save_stack_trace() {
 2)   3.416 us    |                          save_trace();
 2)   3.042 us    |                          save_trace();
 2)   3.000 us    |                          save_trace();
 2)   3.000 us    |                          save_trace();
 2)   2.958 us    |                          save_trace();
 2)   2.959 us    |                          save_trace();
 2)   3.042 us    |                          save_trace();
 2)   3.000 us    |                          save_trace();
 2)   3.125 us    |                          save_trace();
 2)   3.042 us    |                          save_trace();
 2)   3.000 us    |                          save_trace();
 2)   3.292 us    |                          save_trace();
 2)   3.083 us    |                          save_trace();
 2)   3.042 us    |                          save_trace();
 2)   2.959 us    |                          save_trace();
 2)   3.000 us    |                          save_trace();
 2)   3.000 us    |                          save_trace();
 2)   3.000 us    |                          save_trace();
 2)   3.000 us    |                          save_trace();
 2)   3.000 us    |                          save_trace();
 2)   3.000 us    |                          save_trace();
 2)   3.083 us    |                          save_trace();
 2) ! 137.583 us  |                        } /* __save_stack_trace */
 2) ! 143.833 us  |                      } /* save_stack_trace */
 2) ! 149.875 us  |                    } /* stack_trace_save */
 2) ! 156.166 us  |                  } /* set_track */
 2)   3.833 us    |                  init_object();
 2) ! 240.416 us  |                } /* alloc_debug_processing */
 2)               |                deactivate_slab() {
 2)   3.458 us    |                  _raw_spin_lock();
 2) + 12.000 us   |                } /* deactivate_slab */
 2) ! 273.333 us  |              } /* ___slab_alloc.constprop.20 */
 2) ! 285.708 us  |            } /* __slab_alloc.constprop.19 */
 2) ! 310.667 us  |          } /* kmem_cache_alloc_trace */
 2) ! 324.167 us  |        } /* drm_master_create */
 2) + 10.917 us   |        drm_set_master();
 2) ! 382.250 us  |      } /* drm_new_set_master */
 2)   6.333 us    |      mutex_unlock();
 2) ! 429.625 us  |    } /* drm_master_open */
 2)   6.500 us    |    mutex_lock();
 2)   5.708 us    |    mutex_unlock();
 2) # 1116.667 us |  } /* drm_open */
/sys/kernel/debug/tracing # 
```

shell脚本：

```cobol
/mnt # cat trace_fun_graph.sh 
# !/bin/sh
 
 
if [ $# -lt 2 ]; then
    echo "usage:"
    echo "  sh trace_func_graph.sh function exec_file"
    exit -1
fi
 
cd /sys/kernel/debug/tracing/
echo nop > current_tracer
echo function_graph > current_tracer
echo $1 > set_graph_function
echo 1 > options/funcgraph-tail
echo > set_ftrace_filter
echo 1 > tracing_on
$2
echo 0 > tracing_on
cat trace
 
cd -
```



## 三、过滤条件：

```
不想被覆盖需要先将current_trace设置为nop才可以
通用    buffer_total_size_kb    显示所有CPU ring buffer 大小之和
通用    trace_options    trace 过程的复杂控制选项 控制Trace打印内容或者操作跟踪器 也可通过 options/目录设置
通用    options/    显示 trace_option 的设置结果
也可以直接设置，作用同 trace_options
func    function_profile_enabled    打开此选项，trace_stat就会显示function的统计信息
echo 0/1 > function_profile_enabled
func    set_ftrace_pid    设置跟踪的pid
func    set_ftrace_filter    用于显示指定要跟踪的函数
func    set_ftrace_notrace    用于指定不跟踪的函数，缺省为空
graph    max_graph_depth    函数嵌套的最大深度
graph    set_graph_function    设置要清晰显示调用关系的函数
缺省对所有函数都生成调用关系
Stack    stack_max_size    当使用stack跟踪器时，记录产生过的最大stack size
Stack    stack_trace    显示stack的back trace
Stack    stack_trace_filter    设置stack tracer不检查的函数名称
```

