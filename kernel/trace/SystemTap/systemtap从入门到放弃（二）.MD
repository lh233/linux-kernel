## 前言

上一篇《systemtap从入门到放弃（一）》我们知道了什么是systemtap，以及如何书写简单的systemtap脚本。本篇承接上文，介绍systemtap的安装和简易场景应用，通过几个小例子掌握systemtap在内核开发调试中的简单使用。

## 安装systemtap

在linux发行版上安装systemtap，相对而言是一件比较简单得事情，可以选择命令行直接安装，也可以选择源码安装。下面介绍下安装步骤：

1、安装前先确认是否是否配置下面的CONFIG选项，因为systemtap依赖kprobe和relay文件系统：

```
# 可以通过cat /boot/config-xxx | grep xxx  检查下面的配置项是否配置
CONFIG_DEBUG_INFO
CONFIG_KPROBES
CONFIG_DEBUG_FS
CONFIG_RELAY
# 如果没有，需要配置并重新编译安装内核
```

2、类似kdump等，调试内核必然需要安装内核debuginfo包，一般安装位置是/usr/lib/debug，检查没有的话可选择用下面的方法安装：

```
# 查看内核版本
    uname -r 
# centos：
    在http://debuginfo.centos.org/8/x86_64/网址中找到对应版本，下载kernel-debug-debuginfo 以及kernel-debuginfo-common
	
# ubuntu：
	在http://ddebs.ubuntu.com/pool/main/l/linux/ 中下载对应版本（例如linux-image-unsigned-5.4.0-61-generic-dbgsym_5.4.0-61.69_amd64.ddeb）
    dpkg   -i xxx.ddeb 安装
```

3、命令行安装(ubuntu)

```
apt-get install systemtap
apt-get install systemtap-sdt-dev
```

其他安装方式可以参考[systemtap官方wiki](https://sourceware.org/systemtap/wiki)。systemtap脚本解析命令是"stap"，可以用来查看是否安装成功。

stap常用的参数和用法如下：

```
Usage: stap [options] FILE                    Run script in file.
   or: stap [options] -e SCRIPT               Run given script.
   or: stap [options] -l PROBE                List matching probes.
   or: stap [options] -L PROBE                List matching probes and local variables.
[options]
                      -T TIME    terminate the script after TIME seconds
```

前两个是基本的执行脚本命令，后面“-l -L”选项可以列出程序中的探测点。下面以一个实际的例子看下"-L "参数的使用。

-   C源码：

```
main.c：
     1	#include <stdlib.h>
     2	#include <stdio.h> 
     3	extern int sum(int value);
     4	 
     5	struct inout {
     6	    int value;
     7	    int result;
     8	};
     9	
    10	int main(int argc, char * argv[])
    11	{
    12	    struct inout * io = (struct inout * ) malloc(sizeof(struct inout));
    13	    if (NULL == io) {
    14	        printf("Malloc failed.\n");
    15	        return -1;
    16	    }
    17	
    18	    if (argc != 2) {
    19	        printf("Wrong para!\n");
    20	        return -1;
    21	    }
    22	
    23	    io -> value = *argv[1] - '0';
    24	    io -> result = sum(io -> value);
    25	    printf("Your enter: %d, result:%d\n", *argv[1] - '0', io -> result);
    26	    return 0;
    27	}
sum.c：
     1	
     2	int sum(int value) {
     3	    int result = 0;
     4	    int i = 0;
     5	    for (i = 0; i < value; i++)
     6	        result += (i + 1);
     7	    return result;
     8	}
```

加"-g"选项编译生成test可执行文件，使用stap 查看该程序探测点命令如下：

【2】打印结构体变量

```
struct value ｛ 
	int member;
	struct in_value in_v;
	struct in_value *in_v2;
}
```

-   打印结构体变量：可以直接使用$value->member，多级就是$value->in_v->member。注意，无论结构体变量是否是指针，这里都用"->"而不用"."；
-   打印整个结构体：打印整个结构体，只需要在结构体后面加个$:“$value$”，如果打印两层，就加两个:“$value$$”。

【3】修改函数变量

在指定位置probe后，直接给变量赋予新值即可，只是需要注意的是stap要加-g参数在guru模式下才能修改变量的值。还是使用文章刚开始时的main.c 和sum.c 程序，调试步骤如下：

```
root@ubuntu2004:# stap -L 'process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:*")'
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:11") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:12") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:13") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:14") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:15") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:18") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:19") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:20") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:23") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:24") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:25") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:26") $argc:int $argv:char** $io:struct inout*
process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:27") $argc:int $argv:char** $io:struct inout*
```

我们要修改main函数中io -> value的值，该变量在23行被初始化，我们probe 24行，并修改变量值：

```
    10	int main(int argc, char * argv[])
    11	{
            ... ...
    23	    io -> value = *argv[1] - '0';
    24	    io -> result = sum(io -> value);
            ... ...
        }
```

脚本change_value.stp：

```
#!/usr/bin/stap
probe begin {
    printf("=== begin ===\n");
}

probe process("/workspace/test/gdb/test").statement("main@/workspace/test/gdb/main.c:24") {
	printf("input val = %d\n", $io->value);
	$io->value = 9;
	printf("after val = %d\n", $io->value);
}
probe end {
    printf("=== end ===\n");
}
```

结果：

```
root@ubuntu2004:# stap change_value.stp -c '../gdb/test 8' -g
    	Your enter: 8, result:45
	=== begin ===
	input val = 8 after val = 9
        === end ===
```

变量被初始化位8，被我们手动改位9，因此累加结果变成了49！

## 总结
本文通过几个简单的例子介绍了几个简单调试场景下systemtap的时候，更加复杂的调试，比如page fault、tcp等可以参考systemtap的tapset介绍。另外systemtap在性能测试方面也有很多用处，有需要的朋友可以自行去systemtap官网上查看。

