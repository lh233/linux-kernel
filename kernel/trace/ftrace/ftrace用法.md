Ftrace设计作为一个内部的tracer提供给系统的开发者和设计者，帮助他们弄清kernel正在发生的行为，它能够调式分析延迟和性能问题。对于前一章节，我们学习了Ftrace发展到现在已经不仅仅是作为一个function tracer了，它实际上成为了一个通用的trace工具的框架



- 一方面已经从function tracer扩展到irqsoff tracer、preemptoff tracer

- 另一方面静态的trace event也成为trace的一个重要组成部分



通过前面两节的学习，我们知道了什么是ftrace，能够解决什么问题，从这章开始我们主要是学习，怎么去使用ftreace解决问题。


