malloc()函数是C语言中内存分配函数，学习C语言的初学者经常会有这样的困扰。

假设系统由进程A和进程B，分别使用testA和testB函数分配内存：

```
//进程A分配内存
void testA(void)
{
	char *bufA = malloc(100);
	....
	*buf = 100;
	....
}

//进程B分配内存
void testB(void)
{
	char *bufB = malloc(100);
	mlock(buf, 100);
	...
}
```

malloc()函数是c函数封装的一个核心函数，c函数库会做一些处理后调用linux内核系统调用brk，所以大家并不太熟悉brk的系统调用，原因在于很少人会直接调用系统brk向系统申请内存，而总是malloc()之类的c函数库的API函数。如果把malloc()想象成零售，那么brk就是代理商。malloc函数的实现为用户进程维护了一个本地的小仓库，当进程需要使用更多的内存时就向这个小仓库要货，小仓库存量不足时就通过代理商brk向内核批发。

