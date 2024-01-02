## 1. 前言

我们在第 1 篇文章中介绍了 eBPF 虚拟机，包括其有意的设计限制以及如何从用户空间进程中进行交互。如果你还没有读过这篇文章，建议你在继续之前读一下，因为没有适当的介绍，直接开始接触机器和字节码的细节是比较困难的。如果有疑问，请看第 1 部分开头的流程图。

本系列的第 2 部分对第 1 部分中研究的 eBPF 虚拟机和程序进行了更深入的探讨。掌握这些低层次的知识并不是强制性的，但可以为本系列的其他部分打下非常有用的基础，我们将在这些机制的基础上研究更高层次的工具。



## 2. 虚拟机

eBPF 是一个 RISC 寄存器机，共有 11 个 64 位寄存器，一个程序计数器和 512 字节的固定大小的栈。9 个寄存器是通用读写的，1 个是只读栈指针，程序计数器是隐式的，也就是说，我们只能跳转到它的某个偏移量。VM 寄存器总是 64 位宽（即使在 32 位 ARM 处理器内核中运行！），如果最重要的 32 位被清零，则支持 32 位子寄存器寻址 - 这在第 4 部分交叉编译和在嵌入式设备上运行 eBPF 程序时非常有用。

这些寄存器是：

| r0:    | 存储返回值，包括函数调用和当前程序退出代码                   |
| ------ | ------------------------------------------------------------ |
| r1-r5: | 作为函数调用参数使用，在程序启动时，r1 包含 "上下文" 参数指针 |
| r6-r9: | 这些在内核函数调用之间被保留下来                             |
| r10:   | 每个 eBPF 程序 512 字节栈的只读指针                          |

在加载时提供的 eBPF 程序类型决定了哪些内核函数的子集可以被调用，以及在程序启动时通过 r1 提供的"上下文"参数。存储在 r0 中的程序退出值的含义也由程序类型决定。

每个函数调用在寄存器 r1-r5 中最多可以有 5 个参数；这适用于 ebpf 到 ebpf 的调用和内核函数调用。寄存器 r1-r5 只能存储数字或指向栈的指针（作为函数的参数），不能直接指向任意的内存。所有的内存访问必须在 eBPF 程序中使用之前首先将数据加载到 eBPF 栈。这一限制有助于 eBPF 验证器，它简化了内存模型，使其更容易进行内核检查。

BPF 可访问的内核 “辅助”（helper） 函数是由内核通过类似于定义 syscalls 的 API 定义的（不能通过模块扩展），定义使用 BPF_CALL_* 宏。bpf.h 试图为所有 BPF 可访问的内核辅助函数提供参考。例如，bpf_trace_printk 的定义使用了 BPF_CALL_5 和 5 对类型 / 参数名称。定义参数数据类型是非常重要的，因为在每次 eBPF 程序加载时，eBPF 验证器会确保寄存器的数据类型与被调用者的参数类型相符。

在本系列第 1 部分研究的例子中，我们使用了部分有用的内核宏，使用以下结构创建了一个 eBPF 字节码指令数组（所有指令都是这样编码的）：

```
struct bpf_insn {
 __u8 code;  /* opcode */
 __u8 dst_reg:4; /* dest register */
 __u8 src_reg:4; /* source register */
 __s16 off;  /* signed offset */
 __s32 imm;  /* signed immediate constant */
};

/*
msb                                                        lsb
+------------------------+----------------+----+----+--------+
|immediate               |offset          |src |dst |opcode  |
+------------------------+----------------+----+----+--------+
*/
```

让我们看看 BPF_JMP_IMM 指令，它编码了一个针对立即值的条件跳转。下面的宏注释对指令的逻辑应该是不言自明的。操作码编码了指令类别 BPF_JMP，操作（通过 BPF_OP 位域以确保正确）和一个标志 BPF_K，表示它是对直接/常量值的操作。

```
#define BPF_OP(code)    ((code) & 0xf0)
#define BPF_K  0x00

/* Conditional jumps against immediates, if (dst_reg 'op' imm32) goto pc + off16 */

#define BPF_JMP_IMM(OP, DST, IMM, OFF)    \
 ((struct bpf_insn) {     \
  .code  = BPF_JMP | BPF_OP(OP) | BPF_K,  \
  .dst_reg = DST,     \
  .src_reg = 0,     \
  .off   = OFF,     \
  .imm   = IMM })
```

如果我们去计算该指令的值，或者拆解一个包含 BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 2) 的 eBPF 字节码，我们会发现它是 0x020015。这个特定的字节码非常频繁地被用来测试存储在 r0 中的函数调用的返回值；如果 r0 == 0，它就会跳过接下来的 2 条指令。



## 3. 重新认识字节码

现在我们已经有了必要的知识来完全理解本系列第 1 部分中 eBPF 例子中使用的字节码，现在我们将一步一步地进行详解。记住，sock_example.c 是一个简单的用户空间程序，使用 eBPF 来统计回环接口上收到多少个 TCP、UDP 和 ICMP 协议包。

在更高层次上，代码所做的是从接收到的数据包中读取协议号，然后把它推到 eBPF 栈中，作为 map_lookup_elem 调用的索引，从而得到各自协议的数据包计数。map_lookup_elem 函数在 r0 接收一个索引（或键）指针，在 r1 接收一个 map 文件描述符。如果查找调用成功，r0 将包含一个指向存储在协议索引的 map 值的指针。然后我们原子式地增加 map 值并退出。

```
/* eBPF example program:
 * - creates arraymap in kernel with key 4 bytes and value 8 bytes
 *
 * - loads eBPF program:
 *   r0 = skb->data[ETH_HLEN + offsetof(struct iphdr, protocol)];
 *   *(u32*)(fp - 4) = r0;
 *   // assuming packet is IPv4, lookup ip->proto in a map
 *   value = bpf_map_lookup_elem(map_fd, fp - 4);
 *   if (value)
 *        (*(u64*)value) += 1;
 *
 * - attaches this program to loopback interface "lo" raw socket
 *
 * - every second user space reads map[tcp], map[udp], map[icmp] to see
 *   how many packets of given protocol were seen on "lo"
 */
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <linux/bpf.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <stddef.h>
#include <bpf/bpf.h>
#include "bpf_insn.h"
#include "sock_example.h"
#include "bpf_util.h"

char bpf_log_buf[BPF_LOG_BUF_SIZE];

static int test_sock(void)
{
	int sock = -1, map_fd, prog_fd, i, key;
	long long value = 0, tcp_cnt, udp_cnt, icmp_cnt;

	map_fd = bpf_map_create(BPF_MAP_TYPE_ARRAY, NULL, sizeof(key), sizeof(value),
				256, NULL);
	if (map_fd < 0) {
		printf("failed to create map '%s'\n", strerror(errno));
		goto cleanup;
	}

	struct bpf_insn prog[] = {
		BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
		BPF_LD_ABS(BPF_B, ETH_HLEN + offsetof(struct iphdr, protocol) /* R0 = ip->proto */),
		BPF_STX_MEM(BPF_W, BPF_REG_10, BPF_REG_0, -4), /* *(u32 *)(fp - 4) = r0 */
		BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
		BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -4), /* r2 = fp - 4 */
		BPF_LD_MAP_FD(BPF_REG_1, map_fd),
		BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
		BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 2),
		BPF_MOV64_IMM(BPF_REG_1, 1), /* r1 = 1 */
		BPF_ATOMIC_OP(BPF_DW, BPF_ADD, BPF_REG_0, BPF_REG_1, 0),
		BPF_MOV64_IMM(BPF_REG_0, 0), /* r0 = 0 */
		BPF_EXIT_INSN(),
	};
	size_t insns_cnt = ARRAY_SIZE(prog);
	LIBBPF_OPTS(bpf_prog_load_opts, opts,
		.log_buf = bpf_log_buf,
		.log_size = BPF_LOG_BUF_SIZE,
	);

	prog_fd = bpf_prog_load(BPF_PROG_TYPE_SOCKET_FILTER, NULL, "GPL",
				prog, insns_cnt, &opts);
	if (prog_fd < 0) {
		printf("failed to load prog '%s'\n", strerror(errno));
		goto cleanup;
	}

	sock = open_raw_sock("lo");

	if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_BPF, &prog_fd,
		       sizeof(prog_fd)) < 0) {
		printf("setsockopt %s\n", strerror(errno));
		goto cleanup;
	}

	for (i = 0; i < 10; i++) {
		key = IPPROTO_TCP;
		assert(bpf_map_lookup_elem(map_fd, &key, &tcp_cnt) == 0);

		key = IPPROTO_UDP;
		assert(bpf_map_lookup_elem(map_fd, &key, &udp_cnt) == 0);

		key = IPPROTO_ICMP;
		assert(bpf_map_lookup_elem(map_fd, &key, &icmp_cnt) == 0);

		printf("TCP %lld UDP %lld ICMP %lld packets\n",
		       tcp_cnt, udp_cnt, icmp_cnt);
		sleep(1);
	}

cleanup:
	/* maps, programs, raw sockets will auto cleanup on process exit */
	return 0;
}

int main(void)
{
	FILE *f;

	f = popen("ping -4 -c5 localhost", "r");
	(void)f;

	return test_sock();
}
```

在更高层次上，代码所做的是从接收到的数据包中读取协议号，然后把它推到 eBPF 栈中，作为 map_lookup_elem 调用的索引，从而得到各自协议的数据包计数。map_lookup_elem 函数在 r0 接收一个索引（或键）指针，在 r1 接收一个 map 文件描述符。如果查找调用成功，r0 将包含一个指向存储在协议索引的 map 值的指针。然后我们原子式地增加 map 值并退出。

```
BPF_MOV64_REG(BPF_REG_6, BPF_REG_1),
```

当一个 eBPF 程序启动时，r1 中的地址指向 context 上下文（当前情况下为数据包缓冲区）。r1 将在函数调用时用于参数，所以我们也将其存储在 r6 中作为备份。

```
BPF_LD_ABS(BPF_B, ETH_HLEN + offsetof(struct iphdr, protocol) /* R0 = ip->proto */),
```

这条指令从 context 上下文缓冲区的偏移量向 r0 加载一个字节（BPF_B），当前情况下是网络数据包缓冲区，所以我们从一个 iphdr 结构 中提供协议字节的偏移量，以加载到 r0。

```
BPF_STX_MEM(BPF_W, BPF_REG_10, BPF_REG_0, -4), /* *(u32 *)(fp - 4) = r0 */
```

将包含先前读取的协议的字（BPF_W）加载到栈上（由 r10 指出，从偏移量 -4 字节开始）。

```
BPF_MOV64_REG(BPF_REG_2, BPF_REG_10),
BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, -4), /* r2 = fp - 4 */
```

将栈地址指针移至 r2 并减去 4，所以现在 r2 指向协议值，作为下一个 map 键查找的参数。

```
BPF_LD_MAP_FD(BPF_REG_1, map_fd),
```

将本地进程中的文件描述符引用包含协议包计数的 map 加载到 r1。

```
BPF_RAW_INSN(BPF_JMP | BPF_CALL, 0, 0, 0, BPF_FUNC_map_lookup_elem),
```

执行 map 查找调用，将栈中由 r2 指向的协议值作为 key。结果存储在 r0 中：一个指向由 key 索引的值的指针地址。

```
BPF_JMP_IMM(BPF_JEQ, BPF_REG_0, 0, 2),
```

还记得 0x020015 吗？这和第一节的字节码是一样的。如果 map 查找没有成功，r0 == 0，所以我们跳过下面两条指令。

```
BPF_MOV64_IMM(BPF_REG_1, 1), /* r1 = 1 */
BPF_RAW_INSN(BPF_STX | BPF_XADD | BPF_DW, BPF_REG_0, BPF_REG_1, 0, 0), /* xadd r0 += r1 */
```

递增 r0 所指向的地址的 map 值。

```
BPF_MOV64_IMM(BPF_REG_0, 0), /* r0 = 0 */
BPF_EXIT_INSN(),
```

将 eBPF 的 retcode 设置为 0 并退出。

尽管这个 sock_example 逻辑是非常简单（它只是在一个映射中增加一些数字），但在原始字节码中实现或理解它也是很难做到的。更加复杂的任务在像这样的汇编程序中完成会变得非常困难。展望未来，我们将准备使用更高级别的语言和工具来实现更强大的 eBPF 用例，而不费吹灰之力。



## 4. 总结

在这一部分中，我们仔细观察了 eBPF 虚拟机的寄存器和指令集，了解了 eBPF 可访问的内核函数是如何从字节码中调用的，以及它们是如何被核心内核通过类似 syscall 的特殊目的 API 定义的。我们也完全理解了第 1 部分例子中使用的字节码。还有一些未探索的领域，如创建多个 eBPF 程序函数或链式 eBPF 程序以绕过 Linux 发行版的 4096 条指令限制。也许我们会在以后的文章中探讨这些。

现在，主要的问题是编写原始字节码是很困难的，这非常像编写汇编代码，而且编写效果不高。在第 3 部分中，我们将开始研究使用高级语言编译成 eBPF 字节码，到此为止我们已经了解了虚拟机工作的底层基础知识。