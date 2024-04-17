## 1. 简介

Android的ION子系统的目的主要是通过在硬件设备和用户空间之间分配和共享内存，实现设备之间零拷贝共享内存。说来简单，其实不易。在Soc硬件中，许多设备可以进行DMA，这些设备可能有不同的能力，以及不同的内存访问机制。

ION是Google在Android 4.0 ICS中引入，用于改善对于当前不同的android设备，有着各种不同内存管理接口管理相应内存的状况。当前存在着各种不同的但是功能却类似的内存管理接口，例如在NVIDIA Tegra有一个“NVMAP”机制、在TI OMAP有一个“CMEM”机制、在Qualcomm MSM有一个“PMEM”机制，ION将其进行通用化，通过其接口，可集中分配各类不同内存（heap），同时上述三个芯片厂商也正将其内存管理策略切换至ION上。

另外，ION在内核空间和用户空间分别有一套接口，它不仅能管理内存，还可在其clients（来自内核的或者来自用户空间的）之间共享内存。

综上，ION主要功能：

-   内存管理器：提供通用的内存管理接口，通过heap管理各种类型的内存。
-   共享内存：可提供驱动之间、用户进程之间、内核空间和用户空间之间的共享内存。



## 2. 原理

在ION中，用不同heap代表不同类型的内存，每种heap有自己的内存分配策略。

主要的heap：

-   ION_HEAP_TYPE_SYSTEM: 使用vmalloc分配，这个对应ion_heap_ops中的map_user函数，多页不连续
-   ION_HEAP_TYPE_SYSTEM_CONTIG: 通过kmalloc分配，伙伴系统
-   ION_HEAP_TYPE_CARVEOUT: 在启动的时候就保留的物理上连续的内存块
-   ION_HEAP_TYPE_CHUNK: 模块
-   ION_HEAP_TYPE_DMA: memory allocated via DMA API

![](https://ahutxl.cn/images/2022/02/17/image.png)

每个heap中可分配若干个buffer，每个client通过handle管理对应的buffer。每个buffer只能有一个handle对应，每个用户进程只能有一个client，每个client可能有多个handle。两个client通过文件描述符fd（和handle有所对应，通过handle获取），通过映射方式，将相应内存映射，实现共享内存。

![](https://ahutxl.cn/images/2022/02/17/image7bb2c9a02315c8b0.png)

## 3. 使用方法

### 3.1 用户空间使用ION的方法

用户空间可以使用libion库实现对ion的操作，这里不讲述该库的操作方法，用户程序直接通过ioctl和驱动打交道，ion常见的ioctl命令为：

-   ION_IOC_ALLOC: 分配内存
-   ION_IOC_FREE: 释放内存
-   ION_IOC_MAP: 获得一个只想mmap映射的内存的文件描述符
-   ION_IOC_SHARE: 创建一个指向共享的内存的文件描述符
-   ION_IOC_IMPORT: 引入一个共享的文件描述符
-   ION_IOC_CUSTOM: 调用平台自定义的ioctl

具体使用示例可以参见该库的文件实现（system/core/lib/ion/），或如下：



#### 3.1.1 获取一个ION client

需要打开ION设备：

```
open("/dev/ion", O_RDONLY)
```

这里，进程要有访问权限，虽然是使用O_RDONLY标记但是也返回一个可写的内存。返回的文件描述符号做为表示一个ION client的handle。每个用户进程只能有一个client。用户空间的client通过ioctl()系统调用接口和ION交互。

#### 3.1.2 设置获取buffer的参数

填充结构ion_allocation_data，主要包含如下成员：

```
struct ion_allocation_data {
    size_t len;
    size_t align;
    unsigned int heap_mask;
    unsigned int flags;
    struct ion_handle *handle;
};
```

这里 handle做为输出 (struct ion_handle 类型)，需要填充的是其中除handle成员之外的成员（整个buffer是struct ion_allocation_data 类型）。对于其他参数，注意在文档 （http://lwn.net/Articles/480055/） 中并没有给出heap_mask，只说flags是一个比特掩码，标识一个或者多个将要分配所使用的ION heap（结合后面，它的解释是错误的，应该是对heap_mask的解释），但是从源代码中的注释看，这些参数的含义如下：

-   len：分配的大小。
-   align：分配所需的对齐参数。
-   heap_mask：待分配所使用的所有heaps的掩码（如：ION_HEAP_SYSTEM_MASK）。
-   flags：传给heap的标志（如：ION_FLAG_CACHED），ion系统使用低16位，高16位用于各自heap实现使用。

具体各自取值和实现，请参见ion驱动头文件定义和驱动代码。



#### 3.1.3 分配buffer

将设置好的buffer参数传递给ioctl：

```
int ioctl(int client_fd, ION_IOC_ALLOC, struct ion_allocation_data *allocation_data)
```

这里，client_fd就是刚刚打开的/dev/ioc文件描述符号。分配的buffer通过返回的上述结构的 struct ion_handle *handle 成员来引用，但是这个handle并不是一个CPU访问的地址。一个client不能有两个handle指向同样的buffer。



#### 3.1.4 共享buffer

将设置好的buffer参数传递给ioctl：

```
int ioctl(int client_fd, ION_IOC_ALLOC, struct ion_allocation_data *allocation_data)
```

这里，client_fd就是刚刚打开的/dev/ioc文件描述符号。分配的buffer通过返回的上述结构的 struct ion_handle *handle 成员来引用，但是这个handle并不是一个CPU访问的地址。一个client不能有两个handle指向同样的buffer。



#### 3.1.5 传递待共享的文件描述符号

在android设备中，可能会通过Binder机制将共享的文件描述符fd发送给另外一个进程。

为了获得被共享的buffer，第二个用户进程必须通过首先调用 open("/dev/icon", O_RDONLY) 获取一个client handle，ION通过进程ID跟踪它的用户空间clients。 在同一个进程中重复调用open("/dev/icon", O_RDONLY)将会返回另外一个文件描述符号，这个文件描述符号会引用内核同样的client结构 。

获取到共享文件描述符fd后，共享进程可以通过mmap来操作共享内存。



#### 3.1.6 释放

为了释放缓存，第二个client需要通过munmap来取消mmap的效果，第一个client需要关闭通过ION_IOC_SHARE命令获得的文件描述符号，并且使用ION_IOC_FREE如下：

```
int ioctl(int client_fd, ION_IOC_FREE, struct ion_handle_data *handle_data);
```

其中：

```
struct ion_handle_data {
    struct ion_handle *handle;
}
```

命令会导致handle的引用计数减少1。当这个引用计数达到0的时候，ion_handle对象会被析构，同时ION的索引数据结构被更新。

用户进程也可与内核驱动共享ION buffer。



### 3.2 内核空间内使用ION的方法

具体参见参考资料，这里简略介绍。

#### 3.2.1 获取一个ION Client

```
struct ion_client *ion_client_create(struct ion_device *dev,unsigned int heap_mask, const char *debug_name)
```

内核中可以有多个ION clients，每个使用ION的driver拥有一个client。这里，参数dev就是对应/dev/ion的设备，为何需要这个参数，目前还不确切；参数heap_mask和前面叙述一样，用于选择一个或多个ion heaps类型标识堆类型。flags参数前面说过了。



#### 3.2.2 共享来自用户空间的ion buffer

用户传递 ion共享文件描述符 给内核驱动，驱动 转成ion_handle ：

```
struct ion_handle *ion_import_fd(struct ion_client *client, int fd_from_user);
```

在许多包含多媒体中间件的智能手机中，用户进程经常从ion中分配buffer，然后使用ION_IOC_SHARE命令获取文件描述符号，然后将文件描述符号传递给内核驱动。内核驱动调用ion_import_fd()将文件描述符转换成ion_handle对象。内核驱动使用ion_handle对象做为对共享buffer的client本地引用。该函数查找buffer的物理地址一确认是否这个client是否之前分配了同样的buffer，如果是，则仅增加相应handle的引用计数。

有些硬件块只能操作物理地址连续的buffer，所以相应的驱动应 对ion_handle转换 ：

int ion_phys(struct ion_client *client, struct ion_handle *handle, ion_phys_addr_t *addr, size_t *len)
若buffer的物理地址不连续，这个调用会失败。

在处理client的调用之时，ion始终会对input file descriptor,client,和handle arguments进行确认。例如：当import一个file descriptor（文件描述符）之时，ion会保证这个文件描述符确实是通过ION_IOC_SHARE命令创建的。当ion_phys()被调用之时，ION会验证buffer handle是否在client允许访问的handles列表中，若不是，则返回错误。这些验证机制减少了期望之外的访问与资源泄露。

## 4.ION 调试

关于ION debug，在 /sys/kernel/debug/ion/ 提供一个debugfs 接口。

每个heap都有自己的debugfs目录，client内存使用状况显示在 /sys/kernel/debug/ion/<<heap name>>

```
$cat /sys/kernel/debug/ion/ion-heap-1
 client              pid             size
test_ion             2890            16384
```

每个由pid标识的client也有一个debugfs目录/sys/kernel/debug/ion

```
$cat /sys/kernel/debug/ion/2890
heap_name:    size_in_bytes
ion-heap-1:    40960 11
```

