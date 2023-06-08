#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// a scratch area per CPU for machine-mode timer interrupts.
uint64 timer_scratch[NCPU][5];

// assembly code in kernelvec.S for machine-mode timer interrupt.
extern void timervec();

// entry.S jumps here in machine mode on stack0.
// 每个CPU核心会从entry.S跳转到这里开始执行
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  // 这里先设置mstatus状态寄存器，执行mret后，才会真正切换到Supervisor模式
  // mstatus 的 MPP 域代表了CPU的权限级别：机器模式、Supervisor模式、用户模式，不同权限级别对 可使用的寄存器、可执行的指令 有限制
  // 机器模式：权限最高，一般裸机嵌入式程序就直接运行在这个模式下
  // Supervisor模式：权限低于机器模式，现代操作系统内核主要运行在这个模式下，当它想获取一些机器模式的功能时，会通过SBI来trap进机器模式
  // SBI可以认为是底层机器模式提供给上层Supervisor模式的一些标准接口，这个就像现代软件行业的分层架构一样，上层想使用下层的能力，就是通过下层提供的服务、接口
  // 用户模式：权限最低，用户的应用程序运行在这个模式下，当它想使用Supervisor模式的功能时，就是通过系统调用来trap进内核
  // 系统调用是操作系统内核提供给应用程序的一些接口，也可以认为是Supervisor模式提供给用户模式的一些标准接口
  // 见图docs/xv6/riscv特权架构.png
  // 如果开启hypervisor（虚拟化），CPU的权限级别会发生变化，这部分riscv还正在设计，还没有形成最终标准（没有量产做芯片），但因为设计了很多年，也比较稳定了，所以像QEMU就实现了riscv的虚拟化仿真，虚拟化内容很多，这里不展开
  // trap实际上是陷阱门，上层想使用下层提供的功能时，需要通过陷阱门提升权限级别，我们前面说通过接口，实际上接口里面就是用的trap，至于如何trap，就是通过CPU的一些指令来实现的，比如ecall
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  // 把mepc寄存器设置为main函数，main函数在main.c中，执行mret后会跳转过去
  // mret 将 PC 设置为 mepc，通过将 mstatus 的 MPIE 域复制到MIE 来恢复之前的中断使能设置，并将权限模式设置为 mstatus 的 MPP 域中的值。
  // 
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}

// arrange to receive timer interrupts.
// they will arrive in machine mode at
// at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  int id = r_mhartid();

  // ask the CLINT for a timer interrupt.
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..2] : space for timervec to save registers.
  // scratch[3] : address of CLINT MTIMECMP register.
  // scratch[4] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &timer_scratch[id][0];
  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = interval;
  w_mscratch((uint64)scratch);

  // set the machine-mode trap handler.
  w_mtvec((uint64)timervec);

  // enable machine-mode interrupts.
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  w_mie(r_mie() | MIE_MTIE);
}
