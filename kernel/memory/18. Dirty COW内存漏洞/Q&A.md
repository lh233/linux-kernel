在阅读本节前请思考如下小问题口

-   为什么 Dirty COW小程序可以修改一个只读文件的内容?
-   在 Dirty COW内存漏洞中如果 Dirty COW程序没有 madvise Thread线程,即只有procselfmem Thread线程,能否修改foo文件的内容呢?
-   假设在内核空间获取了某个文件对应的 page cache页面的 Struct page数据结杓,而对应的VMA属性是只读,那么内核空间是否可以成功修改该文件呢?
-   如果用户进程使用只读属性( PROT_READ)来mmap映射一个文件到用户空间,然后使用 memcpy来写这段内存空间,会是什么样的情况?

