        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
        # 这里是整个内核的入口，多核处理器的每个CPU核心都会从这里开始执行指令
.section .text
.global _entry
_entry:
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)
        # 机器模式下，每个CPU核心有自己的栈空间，下面6行汇编代码，就是sp = stack0 + (hartid * 4096)这个公式
        # 最终就是为每个CPU核心都设置了自己的栈空间
        la sp, stack0
        li a0, 1024*4
        # csr指riscv中的控制和状态寄存器，而csr指令用来对其进行操作。csrr a1 mhartid是伪指令，对应csrrs a1, mhartid, x0，用于读取CSR。
        # 伪指令主要是给汇编程序员和编译器开发人员用的，让汇编程序更加简洁、美观、可读性更好
        # 其中mhartid（Hart ID Register）：多核CPU当前硬件线程(hart)的ID
        # x0是riscv指令集特有的寄存器，是一个值恒为 0 的寄存器， ARM-32 和 x86-32 这两个指令集中没有零寄存器
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        # jump to start() in start.c
        # 使用call指令调用start.c中的start函数
        call start
spin:
        j spin
