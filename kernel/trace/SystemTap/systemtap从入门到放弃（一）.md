## 前言

内核开发从业者，都知道一个代码调试"大杀器"：printk ！除此之外大家依据自己的习惯，还经常用一些诸如kdump这类的复杂工具。对于systemtap，有人可能熟悉有人可能没听过，本文从入门层次简介systemtap的原理和安装使用，分为两篇，本篇主要介绍原理和脚本语法。文章冗长，多处包含"劝退"功能，下面跟我一起"从入门到放弃" 吧 -_-

## 什么是systemtap ？

systemtap是一个用于简化linux系统运行形态信息收集的开源工具。它立足于性能诊断和bug调试，消除了开发人员在收集数据时需要经历的繁琐和破坏性的工具、重新编译、安装和重新引导的过程。这是systemtap官方wiki给出的介绍，这里不深究它的原理，网上有个图可以清晰得展现systemtap的五脏六腑：

![](https://pic4.zhimg.com/80/v2-bfdc273305930bf91298ddf3e3922b9b_720w.webp)

systemtap可以动态得hook内核代码，其底层就是使用的kprobe接口。systemtap在kprobe的基础上，加上脚本解析和内核模块编译运行单元，使开发人员在应用层即可实现hook内核，大大简化了开发流程。工作原理是通过将脚本语句翻译成C语句，编译成内核模块。模块加载之后，将所有探测的事件以钩子的方式挂到内核上，当任何处理器上的某个事件发生时，相应钩子上句柄就会被执行。最后，当systemtap会话结束之后，钩子从内核上取下，移除模块。整个过程用一个命令 stap 就可以完成。

相比Kprobe提供的kernel API 和debugfs接口，systemtap用起来更加简单，提供给用户简单的命令行接口，以及编写内核指令的脚本语言。对于开发人员，systemtap是一款难得的工具，对于bug调试、性能分析、源码学习非常有用。

使用systemtap的门槛是其有自己的脚本语法，使用的时候还需要对其已经实现的"库函数"很熟悉。不过其语法相对简单，很多用法和C语言类似，因此对于底层发开人员来说，还是比较友好的！



## systemtap语法简介

systemtap 的核心思想是定义一个事件（event），以及给出处理该事件的句柄（Handler）。当一个特定的事件发生时，内核运行该处理句柄，就像快速调用一个子函数一样，处理完之后恢复到内核原始状态。这里有两个概念：

-   事件（Event）：systemtap 定义了很多种事件，例如进入或退出某个内核函数、定时器时间到、整个systemtap会话启动或退出等等。
-   句柄（Handler）：就是一些脚本语句，描述了当事件发生时要完成的工作，通常是从事件的上下文提取数据，将它们存入内部变量中，或者打印出来。

因此编写systemtap脚本就是一个找到符合需求的事件，并编写事件处理流程的过程。下面介绍该脚本语言常用的几个元素格式：

【1】脚本命名

脚本名字符合linux文件命名即可。一般名字后辍使用".stp"，方便人们知道它是systemtap脚本，比如"memory.stp"；

【2】注释

脚本支持多种注释方式，# 、//、/**/ 均可。可依据个人习惯使用。另外类似其他脚本，systemtap脚本在开头也需要标明脚本解析器的路径，一般是"#!/usr/bin/stap"，不确定的可以通过命令"whereis stap"找到脚本解析器位置；

【3】变量

变量需要字母开头，一般由字母、数字组成，当然还可以包括美元符号和下划线字符。变量可以在函数的任意处声明，也可以直接使用（通过第一次使用探测变量类型）。变量默认作用域是函数或括号内部，定义全局变量需要加"global"（写在函数外任意处）。

【4】数组

数组必须被定义成"global"变量，默认大小不超过2048(MAXMAPENTRIES)，定义时可以省略大小，除非是想定义超过2048的大数组：

```
global mybigarr[20000]
global myarr
probe begin 
{
	myarr[0]=1
	myarr[1]=2
	myarr[3]=4
	foreach(x in myarr) {
		printf("%d\n",myarr[x])
	}
}
```

另外还支持关联数组(哈希数组)，关联数组中的索引或键由一个或多个字符串或整数值(逗号隔开)组成：

```
# key值就是索引
arr1[“foo”] = 14
arr2[“coords”,3,42,7] = “test”
# 删除数组
delete myarr
# 删除数组元素
delete myarr[tid()] 
```

【5】条件语句

用法和C语言一样：

```
if (xxx)
    xxx
else
    xxx
```

【6】循环

基础用法和C语言一样，比如：

```
for(i=0;i<10;i++) { ... }
while (i<10) { ... }
```

除此之外，还提供一种用于数组的特殊循环“foreach”:

```
global myarr
probe begin 
{
	myarr[0]=1
	myarr[1]=2
	myarr[3]=4
	foreach(x in myarr) {
		printf("%d\n",myarr[x])
	}
}
```

【7】函数

普通函数使用function声明，函数返回值类型通过":"跟在函数名后面；参数类型通过":"跟在函数参数后面，多个参数通过","隔开，例如：

```
# 返回值和参数均为long
function is_open_creating:long (flag:long)
{
      CREAT_FLAG = 4
      if (flag & CREAT_FLAG){
          return 1
      }
      return 0
}
```

另外一种函数是probe函数，下面以探测内核函数和模块函数为例，介绍几种常见用法：

-   probe内核和模块函数通用格式：

```
# kernel
probe kernel.function("kernel_function_name"){ ... }
# module：
probe module("module_name").function("module_function_name") { ... }
```

-   另外，函数名支持通配符，例如：

```\
# 所有的ext3_get* 前缀函数
probe module("ext3").function("ext3_get*") { ... }
```

-   对于使用相同“handle”函数的probe函数，可以叠加定义：

```
# 可以叠加多个,如果probe的函数不存在，在编译时就会保错
probe module("ext3").function("ext3_get*") ,
probe module("ext3").function("ext3_get*")
{ 
    print("getting or setting something here\n") 
}
```

-   有时候因为内核版本不同，有些函数名字不一样，或者某些版本里函数不存在，systemtap提供了几种“条件函数”和“可选择函数”供灵活使用：

```
# 通过条件符号,如果函数存在才生效：
kernel.function("may_not_exist") ? { ... }

# 如果第一个不存在再判断后续的；如果存在只会probe靠前的
kernel.function("this_might_exist") !,
kernel.function("if_not_then_this_should") !,
kernel.function("if_all_else_fails") { ... }

#通过条件语句,一般用于动态条件。即脚本运行时才可以确定的条件：
probe kernel.function("some_func") if ( someval > 10) { ... }
```

-   还可以在函数末尾加上“.call”或“.return”，分别表示函数被调用和返回时probe：

```
# 在调用build_open_flags时probe，handle是：打印rbp寄存器的值
probe kernel.function("build_open_flags").call {
        printf("rbp=%p\n", register("rbp"));
}
```

【8】通过命令行传递参数

和shell等脚本类似，可以在脚本里引用命令行传递的参数。不过stp脚本需要预先知道参数的类型，因为引用不同类型参数方式不同。

-   对于整数类型参数，通过“$N”引用，N是第几个参数(base 1)；
-   对于字符串参数，通过“@N”引用，N是第几个参数(base 1)，如果字符串中间有空格，需要在字符串两边加上双引号(不加就是两个变量)；



举例：

```
命令行：
    stap script.stp sometext 42
引用：
    printf(“arg1: %s, arg2: %d\n”, @1, $2)

命令行：
    stap script.stp "sometext nexttxt" 42
引用：
    printf(“arg1: %s, arg2: %d\n”, @1, $2)
```



## 可探测事件

前面章节介绍的"probe kernel.function()"等，只是诸多可探测事件中的一种。我们知道Kprobes允许你为任何内核指令以及函数入口和函数返回处理程序安装预处理程序和后处理程序，因此systemtap也支持在函数任意有效行(可能存在编译优化)进行probe。

Systemtap支持许多内置探测事件，这些事件是systemtap官方预先写好的脚本，被称为tapset。可以参考官方的tapsets手册使用这些库函数，在安装完成后，一般在本地位置是/usr/share/systemtap/tapset，如果想引用其他路径下的stap脚本，需要添加参数“-I”。

常用的可探测事件有：

```
begin,       systemtap 会话开始
end,         systemtap 会话结束
kernel.function("sys_xxx").call,     系统调用xx的入口
kernel.function("sys_xxx").return,   系统调用xx的返回
kernel.statement("func@sourcefile.c:100")        kernel文件sourcefile.c 100行处的状态
timer.ms(300),      每300毫秒的定时器
timer.profile,      每个CPU上周期触发的定时器
process("a.out").function("foo*"),               a.out 中函数名前缀为foo的函数信息
process("a.out").statement("*@main.c:200"),      a.out中文件main.c 200行处的状态
```

另外还封装了一些常用的可打印值，例如：

```
tid(),      当前线程id
pid(),      当前进程id
uid(),      当前用户id
execname(),    当前进程名称
cpu(),         当前cpu编号
gettimeofday_s(),      秒时间戳
get_cycles(),     硬件周期计数器快照
pp(),             探测点事件名称
ppfunc(),         探测点触发的函数名称
$$var, 上下文中存在 $var， 可以使用该变量
print_backtrace(),         打印内核栈
print_ubacktrace(),        打印用户空间栈
thread_indent()，打印N个空格，常用于打印多级函数时的缩进(配合target()选择目标进程)
```

