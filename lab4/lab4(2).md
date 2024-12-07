### 练习

对实验报告的要求：
 - 基于markdown格式来完成，以文本方式为主
 - 填写各个基本练习中要求完成的报告内容
 - 完成实验后，请分析ucore_lab中提供的参考答案，并请在实验报告中说明你的实现与参考答案的区别
 - 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
 - 列出你认为OS原理中很重要，但在实验中没有对应上的知识点

#### 练习0：填写已有实验
本实验依赖实验2/3。请把你做的实验2/3的代码填入本实验中代码中有“LAB2”,“LAB3”的注释相应部分。
#### 练习1：分配并初始化一个进程控制块（需要编码）
alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。

>【提示】在alloc_proc函数的实现中，需要初始化的proc_struct结构中的成员变量至少包括：state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。

实现代码如下：
```c
        proc->state = PROC_UNINIT;
	proc->pid = -1；
	proc->runs = 0;
	proc->kstack = 0;
	proc->need_resched = 0;
	proc->parent = NULL;//内核创建的idle进程没有父进程
	proc->mm = NULL;
	memset(&(proc->context), 0, sizeof(struct context));
	proc->tf = NULL;
    proc->cr3 = boot_cr3;
	proc->flags = 0;
	memset(proc->name, 0, PROC_NAME_LEN + 1);
```
根据指导书的要求，我们需要对proc进行初始化，将proc_struc中的各个成员变量都清零，其中有一些设为特殊值：
- state设置为未初始化状态；
- pid设置为-1；
- 进程运行时间初始化为0；
- 内核栈地址kstack默认从0开始；
- need_resched是一个用于判断是否需要进程的调度替换，为1则需要进行调度，这里我们将其初始化为0；
- 父进程parent设置为空；
- 内存空间初始化为空；
- 上下文context初始化为0；
- 中断帧指针tf设置为NULL；
- 页目录cr3设置为为内核页目录表的基址boot_cr3；
- 标志位flags设置为0；
- 进程名name初始化为0；

##### 问题1：请说明proc_struct中struct context context和struct trapframe *tf成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）

- 于保存进程的CPU上下文（如寄存器状态），以便在上下文切换时恢复进程的执行状态。上下文切换发生在操作系统需要从一个进程切换到另一个进程时，必须保存当前进程的寄存器状态，并恢复下一个进程的寄存器状态，以继续其执行。这通常涉及寄存器的保存和恢复，以及程序计数器（PC）的设置。
在通过proc_run切换到CPU上运行时，需要调用switch_to将原进程的寄存器保存，以便下次切换回去时读出，保持之前的状态。
- struct trapframe 的作用是保存当进程处于内核模式时的寄存器值（包括程序计数器、栈指针、标志寄存器等），以便异常或系统调用结束后，可以返回到用户模式并恢复进程的执行状态。在处理系统调用或中断时，内核会修改 tf 中的寄存器值，而当系统调用或中断处理完成后，内核会恢复 tf 中的值，使进程能够继续执行。

#### 练习2：为新创建的内核线程分配资源（需要编码）
创建一个内核线程需要分配和设置好很多资源。kernel_thread函数通过调用do_fork函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们实际需要"fork"的东西就是stack和trapframe。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：

- 调用alloc_proc，首先获得一块用户信息块。
- 为进程分配一个内核栈。
- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
- 复制原进程上下文到新进程
- 将新进程添加到进程列表
- 唤醒新进程
- 返回新进程号

请在实验报告中简要说明你的设计实现过程。

实现代码如下：
```c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    // 进程数目超过了最大值，返回错误
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    //1. call alloc_proc to allocate a proc_struct
    if((proc = alloc_proc()) == NULL){
        goto fork_out;
    }

    //2. call setup_kstack to allocate a kernel stack for child process
    if(setup_kstack(proc) != 0){
        goto bad_fork_cleanup_proc;  // 释放刚刚alloc的proc_struct
    }

    //3. call copy_mm to dup OR share mm according clone_flag
    if(copy_mm(clone_flags, proc) != 0){
        goto bad_fork_cleanup_kstack;  // 释放刚刚setup的kstack
    }

    //4. call copy_thread to setup tf & context in proc_struct
    copy_thread(proc, stack, tf);  // 复制trapframe，设置context

    //5. insert proc_struct into hash_list && proc_list
    proc->pid = get_pid();
    hash_proc(proc);  // 插入hash_list
    list_add(&proc_list, &(proc->list_link));  // 插入proc_list
    nr_process ++;

    //6. call wakeup_proc to make the new child process RUNNABLE
    wakeup_proc(proc);  // 设置为RUNNABLE

    // 7. set ret vaule using child proc's pid
    ret=proc->pid;

fork_out:
    return ret;
bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```
首先使用`alloc_proc`函数分配一个`proc_struct`结构体，然后使用`setup_kstack`函数为新进程分配内核栈，接着使用`copy_mm`函数复制父进程的内存管理信息，然后使用`copy_thread`函数复制父进程的`trapframe`，并设置`context`。最后使用`get_pid`函数为新进程分配一个唯一的进程号，将新进程插入到进程列表和哈希表中，并设置为`RUNNABLE`状态，返回新进程的进程号。


##### 问题2：请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。
ucore使用`get_pid`函数为新进程分配一个唯一的进程号。在其中使用了两个静态变量`last_pid`和`next_safe`。
`last_pid`用于记录上一个进程的进程号，`next_safe`用于维护最小的一个不可用进程号。
每一次进入`get_pid`后，可以直接从(`last_pid`,`next_safe`)这个开区间中直接获得一个可用的进程号，也就是`last_pid+1`，直到这个区间中不存在进程号，也就是`last_pid+1==next_safe`。此时，通过遍历进程链表，在整个进程号空间中寻找最小可用的进程号，将`last_pid`更新为该进程号，在循环进程链表的同时将`next_safe`更新为大于`last_pid`的最小不可用进程号，从而方便下一次获取进程号，最后返回`last_pid`。

#### 练习3：编写proc_run 函数（需要编码）
proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括：

检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。

禁用中断。你可以使用/kern/sync/sync.h中定义好的宏local_intr_save(x)和local_intr_restore(x)来实现关、开中断。

切换当前进程为要运行的进程。

切换页表，以便使用新进程的地址空间。/libs/riscv.h中提供了lcr3(unsigned int cr3)函数，可实现修改CR3寄存器值的功能。

实现上下文切换。/kern/process中已经预先编写好了switch.S，其中定义了switch_to()函数。可实现两个进程的context切换。

允许中断。

实现代码如下：
```c
bool intr_flag;//保存当前中断状态
struct proc_struct* prev = current, * next = proc;
//prev 指向当前正在运行的进程 current，而 next 指向目标进程 proc
local_intr_save(intr_flag);
//调用 local_intr_save() 宏，保存当前的中断状态，并禁用中断
{
    current = proc;
    lcr3(next->cr3);//设置 CR3 寄存器为目标进程 next 的 cr3 值，即切换到目标进程的页表
    switch_to(&(prev->context), &(next->context));
    //调用 switch_to() 函数，进行上下文切换
}
local_intr_restore(intr_flag);//恢复中断状态，调用 local_intr_restore() 函数
```

##### 问题：在本实验的执行过程中，创建且运行了几个内核线程？

在本实验中创建并运行了两个内核线程：

0号线程`idleproc`：完成内核中各个子系统的初始化，之后立即调度，执行其他进程。

和1号线程`initproc`：用于完成实验的功能而调度的内核进程。


`make qemu`后得到：
```

(THU.CST) os is loading ...

Special kernel symbols:
  entry  0xc0200032 (virtual)
  etext  0xc0204e76 (virtual)
  edata  0xc020a060 (virtual)
  end    0xc02155cc (virtual)
...
++ setup timer interrupts
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
kernel panic at kern/process/proc.c:370:
    process exit!!.

Welcome to the kernel debug monitor!!
Type 'help' for a list of commands.
```

#### 扩展练习
##### 说明语句 local_intr_save(intr_flag);....local_intr_restore(intr_flag); 是如何 实现开关中断的？
```c
#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>

static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */
```
我们可以找到对应定义的代码，根据代码可知，当调用`local_intr_save`的时候，检查当前中断状态，如果中断已启用，则禁用中断，并返回 1。如果中断已经禁用，则不做任何操作，直接返回0。

- read_csr(sstatus)：读取当前的 sstatus 寄存器的值。SSTATUS_SIE 是 sstatus 中的一个标志位，表示外部中断是否被使能。
- intr_disable()：禁用中断的操作。这通常通过修改 sstatus 寄存器中的 SSTATUS_SIE 位来完成。
- return 1 表示中断原本是启用的，禁用了中断。return 0 表示中断原本是禁用的，不做任何操作。


当需要恢复中断时，调用local_intr_restore，需要判断intr_flag的值，如果其值为1，则需要调用intr_enable将sstatus寄存器的SIE位置1，否则该位依然保持0。以此来恢复调用local_intr_save之前的SIE的值。

