内存管理大多是以页为中心展开的，struct page数据结构显得非常重要，在阅读本节前请思考如下小问题

1. struct page 数据结构中的\_count和\_mapcount有什么区别？



2. 匿名页面和page cache页面有什么区别？
3. struct page 数据结构中有一个锁，请问trylock_page()和lock_page()有什么区别？