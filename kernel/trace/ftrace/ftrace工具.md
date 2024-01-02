# linux性能工具--ftrace基础篇

我们做内核开发的时候，我们经常要去跟踪linux内核的函数调用关系，对于我们来说ftrace是一个十分好用的工具，值得我们好好学习。ftrace不只是一个函数跟踪工具，它的跟踪能力之强大，还能调试和分析诸如延迟、意外代码路径、性能问题等一大堆问题。它也是一种很好的学习工具。本章的主要是学习：

- ftrace是什么

- ftrace来解决什么问题



## 1 什么是ftrace

首先，在学习ftrace之前，我们要知道它是什么？根据[linux ftrace](https://www.kernel.org/doc/Documentation/trace/ftrace.txt)的详细介绍，ftrace是一个linux内部的一个trace工具，用于帮助开发者和系统设计者知道内核当前正在干什么，从而更好的去分析性能问题。

### 1.1 ftrace的由来

ftrace是由Steven Rostedy和Ingo Molnar在内核2.6.27版本中引入的，那个时候，systemTap已经开始崭露头角，其它的trace工具包括LTTng等已经发展多年，那么为什么人们还需要开发一个trace工具呢？

> SystemTap项目是 Linux 社区对 SUN Dtrace 的反应，目标是达到甚至超越 Dtrace 。因此 SystemTap 设计比较复杂，Dtrace 作为 SUN 公司的一个项目开发了多年才最终稳定发布，况且得到了 Solaris 内核中每个子系统开发人员的大力支持。 SystemTap 想要赶超 Dtrace，困难不仅是一样，而且更大，因此她始终处在不断完善自身的状态下，在真正的产品环境，人们依然无法放心的使用她。不当的使用和 SystemTap 自身的不完善都有可能导致系统崩溃。

Ftrace的设计目标简单，本质上是一种**静态代码插装技术**，**不需要**支持某种编程接口让用户自定义 trace 行为。静态代码插装技术更加可靠，不会因为用户的**不当使用**而导致**内核崩溃**。 ftrace 代码量很小，稳定可靠。同时`Ftrace` 有重大的创新：



- Ftrace 只需要在函数入口插入一个外部调用：mcount

- Ftrace 巧妙的拦截了函数返回的地址，从而可以在运行时先跳到一个事先准备好的统一出口，记录各类信息，然后再返回原来的地址

- Ftrace 在链接完成以后，把所有插入点地址都记录到一张表中，然后默认把所有插入点都替换成为空指令（nop），因此默认情况下 Ftrace 的开销几乎是 0

- Ftrace 可以在运行时根据需要通过 Sysfs 接口使能和使用，即使在没有第三方工具的情况下也可以方便使用
  

### 1.2 ftrace 原理

ftrace的名字由function trace而来。function trace是利用gcc编译器在编译时在每个函数的入口地址放置一个probe点，这个probe点会调用一个probe函数（gcc默认调用名为mcount的函数），这样这个 probe函数会对每个执行的内核函数进行跟踪（其实有少数几个内核函数不会被跟踪），并打印log到一个内核中的环形缓存（ring buffer）中，而用户可以通过debugfs来访问这个环形缓存中的内容。

各类[tracer](https://so.csdn.net/so/search?q=tracer&spm=1001.2101.3001.7020)往ftrace主框架注册，不同的trace则在不同的probe点把信息通过probe函数给送到ring buffer中，再由暴露在用户态debufs实现相关控制。其主要的框架图如下图所示

![](https://img-blog.csdnimg.cn/ce2dc592eef547c988211f01ee01a493.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3UwMTI0ODkyMzY=,size_16,color_FFFFFF,t_70%23pic_center)


