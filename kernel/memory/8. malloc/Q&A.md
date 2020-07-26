malloc()函数返回的内存是否马上会分配物理内存？testA和testB分别在何时分配物理内存？



假设不考虑libc的因素，malloc分配了100Byte，那么实际内核也是为其分配了100Byte吗？



假设使用printf打印指针bufA和bufB指向的地址是一样的，那么在内核中这两块虚拟内存是否“打架”了呢？



vm_normal_page()函数返回的什么样的页面struct page数据结构？为什么内存管理代码需要这个函数？



请简述get_user_page()函数的作用和实现流程。



请简述follow_page()函数的作用的实现过程。

