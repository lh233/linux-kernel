## Systrace介绍

Systrace 是Android平台提供的一款工具，用于记录短期内的设备活动。该工具会生成一份报告，其中汇总了Android内核中的数据，例如CPU调度程序、磁盘活动和应用线程。这份报告可帮助我们了解如何以最佳方式改善应用或游戏的性能。

Systrace 工具用于显示整个设备在做些什么，不过也可用于识别应用中的卡顿。Systrace 的系统开销非常小，因此你可以在插桩测试期间体验实际卡顿情况。

Systrace报告示例：

![](https://img-blog.csdnimg.cn/2020110419184963.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3UwMTE1Nzg3MzQ=,size_16,color_FFFFFF,t_70#pic_center)

这份报告提供了 Android 设备在给定时间段内的系统进程的总体情况，还会检查所捕获的跟踪信息，以突出显示它所观察到的问题（例如界面卡顿或耗电量高）。

## Systrace使用方法详解

要想使用Systrace分析性能，我们首先需要找到Systrace工具，以及使用该工具生成一份.html的报告文件。

### 使用命令行捕获Systreace报告文件

**systrace命令**
systrace命令会调用Systrace工具，以收集和检查设备上在系统一级运行的所有进程的时间信息。

systrace命令是一个Python脚本，所以需要进行如下准备：

Android Studio下载并安装最新的Android SDK Tools。
安装Python并将其添加到工作站的执行路径中（注意，这里要求Python2.7版本）。
使用USB调试连接将搭载Android 4.3（API 级别 18）或更高版本的设备连接到开发系统。
systrace 命令在 Android SDK Tools 工具包中提供，位于 android-sdk/platform-tools/systrace/。

例如，作者电脑上sstrace位置为：/Users/apple/Library/Android/sdk/platform-tools/systrace。



**命令语法**

要为应用生成HTML报告，我们需要使用以下语法从命令行运行systrace：

```
python systrace.py [options] [categories]
```



**命令和命令选项**

![](https://img-blog.csdnimg.cn/20201104191919334.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3UwMTE1Nzg3MzQ=,size_16,color_FFFFFF,t_70#pic_center)

示例1：

```
python ./systrace.py -t 5 -o mynewtrace.html
```

