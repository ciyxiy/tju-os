# lab 0.5
## 练习1：使用GDB验证启动流程
为了熟悉使用qemu和gdb进行调试工作,使用gdb调试QEMU模拟的RISC-V计算机加电开始运行到执行应用程序的第一条指令（即跳转到0x80200000）这个阶段的执行过程，说明RISC-V硬件加电后的几条指令在哪里？完成了哪些功能？要求在报告中简要写出练习过程和回答。

### 执行过程
一、加电

（一）打开两个终端输入命令调试ucore：
``` 
make debug
make gdb
``` 


（二）使用make gdb调试，输入指令x/10i $pc 查看即将执行的10条汇编指令：
```  
0x1000:	auipc	t0,0x0      #将当前PC加上立即数0，存入寄存器t0
0x1004:	addi	a1,t0,32    #t0的值加上立即数32，存入寄存器a1
0x1008:	csrr	a0,mhartid  #将特殊寄存器mhartid（Machine Hart ID，即机器硬件线程ID）的值复制到寄存器a0
0x100c:	ld	    t0,24(t0)   #从寄存器 t0 偏移 24 个字节处加载一个双字（64位）到寄存器 t0
0x1010: jr	    t0          #跳转到寄存器 t0 指示的地址，此时t0寄存器指示的地址就是0x80000000
0x1014:	unimp  
0x1016:	unimp
0x1018:	unimp
0x101a:	0x8000
0x101c:	unimp
```
这段RISC-V汇编代码的目的是为启动程序做好准备，接下来我们通过si命令逐行调试，并查看寄存器结果：
```
(gdb) si
0x0000000000001004 in ?? ()
(gdb) info r t0
t0             0x0000000000001000	4096
(gdb) si
0x0000000000001008 in ?? ()
(gdb) info r t0
t0             0x0000000000001000	4096
(gdb) si
0x000000000000100c in ?? ()
(gdb) info r t0
t0             0x0000000000001000	4096
(gdb) si
0x0000000000001010 in ?? ()
(gdb) info r t0
t0             0x0000000080000000	2147483648  #t0 = [t0 + 24] = 0x80000000
(gdb) si
0x0000000080000000 in ?? ()                     #跳转到地址0x80000000
```

最后通过 jr t0 跳转到物理地址 0x80000000 处，作为 bootloader 的 OpenSBI.bin 被加载到物理内存以这个物理地址 （0x80000000）开头的区域上。

二、OpenSBI启动

输入x/10i $pc，显示0x80000000处的10条数据。该地址处加载的是作为bootloader的OpenSBI.bin，该处的作用为加载操作系统内核并启动操作系统的执行。代码如下：
```
(gdb) x/10i $pc
=> 0x80000000:	csrr	a6,mhartid     #从 mhartid CSR中读取硬件线程 ID，并将结果存储在寄存器 a6 中
   0x80000004:	bgtz	a6,0x80000108  #如果 a6 寄存器的值大于零，则跳转到地址 0x80000108 处执行。
   0x80000008:	auipc	t0,0x0         #将当前指令的地址的高 20 位加上偏移量（0x0），并将结果存储在 t0 寄存器中。
   0x8000000c:	addi	t0,t0,1032     #将 t0 寄存器的值加上 1032，并将结果存储在 t0 寄存器中。
   0x80000010:	auipc	t1,0x0         #将当前指令的地址的高 20 位加上偏移量（0x0），并将结果存储在 t1 寄存器中。
   0x80000014:	addi	t1,t1,-16      #将 t1 寄存器的值减去 16，并将结果存储在 t1 寄存器中。
   0x80000018:	sd	    t1,0(t0)       #将 t1 寄存器的值存储到 t0 寄存器指定地址的内存位置。
   0x8000001c:	auipc	t0,0x0         #将当前指令的地址的高 20 位加上偏移量（0x0），并将结果存储在 t0 寄存器中。
   0x80000020:	addi	t0,t0,1020     #将 t0 寄存器的值加上 1020，并将结果存储在 t0 寄存器中。
   0x80000024:	ld	    t0,0(t0)       #将 t0 寄存器指定地址的内存内容加载到 t0 寄存器。

```



三、 跳转到 0x80200000 (kern/init/entry.S）
```
break *0x80200000
```
由题目已知第一条指令位于0x80200000处，通过该命令在该处打上断点。

输入命令：
```
continue
```
运行到对应位置。之后查看此处的10条指令：
```
(gdb) x/10i $pc
=> 0x80200000 <kern_entry>:		auipc	sp,0x3  
   #将当前指令的地址的高 20 位加上一个偏移量（0x3），存储在栈指针寄存器 sp 中。
   0x80200004 <kern_entry+4>:	mv	sp,sp
   0x80200008 <kern_entry+8>:	j	0x8020000c <kern_init>  
   #跳转到地址 0x8020000c 处。
   0x8020000c <kern_init>:		auipc	a0,0x3
   0x80200010 <kern_init+4>:	addi	a0,a0,-4
   0x80200014 <kern_init+8>:	auipc	a2,0x3
   0x80200018 <kern_init+12>:	addi	a2,a2,-12
   0x8020001c <kern_init+16>:	addi	sp,sp,-16
   #将 sp 寄存器的值减去 16，存储在 sp 寄存器中，用于为新的栈帧分配空间。
   0x8020001e <kern_init+18>:	li	a1,0
   #将立即数 0 加载到 a1 寄存器
   0x80200020 <kern_init+20>:	sub	a2,a2,a0
```
这段代码是执行应用程序的第一条指令，即0x80200000（kern_entry）后的指令。

前三行代码等价于
```
la sp, bootstacktop
tail kern_init
```
对这两段代码的分析见lab1。

###  回答

1、RISCV加电后的指令在地址0x1000到地址0x1010。其中在地址为0x1010的指令处会跳转。

2、完成的功能如下：
```
0x1000:	auipc	t0,0x0          #将当前PC加上立即数0，存入寄存器t0
0x1004:	addi	a1,t0,32        #t0的值加上立即数32，存入寄存器a1
0x1008:	csrr	a0,mhartid      #将特殊寄存器（CSR）值复制到寄存器a0
0x100c:	ld	    t0,24(t0)       #从寄存器 t0 偏移 24 个字节处加载一个双字（64位）到寄存器 t0
0x1010:	jr	    t0              #跳转到寄存器 t0 指示的地址，此时t0寄存器指示的地址就是0x80000000
```
3、本实验中重要的知识点，以及与对应的OS原理中的知识点

（1）OpenSbi固件对应os原理中的bootloader

OpenSbi固件是QEMU自带的bootloader，在 Qemu 开始执行任何指令之前，首先两个文件将被加载到 Qemu 的物理内存中：即作为 bootloader 的 OpenSBI.bin 被加载到物理内存以物理地址 0x80000000 开头的区域上，同时内核镜像 os.bin 被加载到以物理地址 0x80200000 开头的区域上。

bootloader的作用是将操作系统加载在内存里。


（2）链接脚本

链接器的主要作用是将多个输入文件（通常是目标文件，如 .o 文件）结合在一起，生成一个单一的可执行文件（如 ELF 文件），链接过程是将多个独立编译的模块整合成一个完整的可执行程序的重要步骤，确保它们能够正确地协同工作。

实验中的tools/kernel.ld 定义了一个在RISC-V平台上生成内核可执行文件的内存布局。确保内核在加载时具有合适的内存布局和访问结构。






4、OS原理中很重要，但在实验中没有对应上的知识点

编译所有的源代码，把目标文件链接起来，生成elf文件，生成bin硬盘镜像，用qemu跑起来。

makefile 的规则：
```
target ... : prerequisites ...
    command
    ...
    ...
```
target也就是一个目标文件，可以是object file，也可以是执行文件。还可以是一个标签（label）。prerequisites就是，要生成那个target所需要的文件或是目标。command也就是make需要执行的命令（任意的shell命令）。 这是一个文件的依赖关系，也就是说，target这一个或多个的目标文件依赖于prerequisites中的文件，其生成规则定义在 command中。如果prerequisites中有一个以上的文件比target文件要新，那么command所定义的命令就会被执行。



在实验中直接使用make file



# lab1  
### 练习1：理解内核启动中的程序入口操作
阅读 kern/init/entry.S内容代码，结合操作系统内核启动流程，说明指令 la sp, bootstacktop 完成了什么操作，目的是什么？ tail kern_init 完成了什么操作，目的是什么？
```
la sp, bootstacktop  #操作：将栈指针（sp）设置为bootstacktop的地址；目的：实现内核栈的初始化，bootstacktop就是内核栈的顶部地址。
tail kern_init  #操作：通过尾调用的方式跳转到kern_init函数执行；目的：通知编译器此处的函数不再返回。这有助于优化编译器可以根据这一信息省略不必要的返回操作。
```

### 练习2：完善中断处理 

请编程完善trap.c中的中断处理函数trap，在对时钟中断进行处理的部分填写kern/trap/trap.c函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用print_ticks子程序，向屏幕上打印一行文字”100 ticks”，在打印完10行后调用sbi.h中的shut_down()函数关机。

要求完成问题1提出的相关函数实现，提交改进后的源代码包（可以编译执行），并在实验报告中简要说明实现过程和定时器中断中断处理的流程。实现要求的部分代码后，运行整个系统，大约每1秒会输出一次”100 ticks”，输出10行。

编写代码如下：

```
case IRQ_S_TIMER:
            // "All bits besides SSIP and USIP in the sip register are
            // read-only." -- privileged spec1.9.1, 4.1.4, p59
            // In fact, Call sbi_set_timer will clear STIP, or you can clear it
            // directly.
            // cprintf("Supervisor timer interrupt\n");
             /* LAB1 EXERCISE2   YOUR CODE : */
            /*(1)设置下次时钟中断- clock_set_next_event()
             *(2)计数器（ticks）加一
             *(3)当计数器加到100的时候，我们会输出一个`100ticks`表示我们触发了100次时钟中断，同时打印次数（num）加一
            * (4)判断打印次数，当打印次数为10时，调用<sbi.h>中的关机函数关机
            */
            clock_set_next_event();
            ticks++;
            if(ticks%TICK_NUM == 0){
            	print_ticks();
            	num++;
            }
            if (num==10) {  
                sbi_shutdown();
            }
            break;
```

输出如下：

```
Special kernel symbols:
  entry  0x000000008020000a (virtual)
  etext  0x00000000802009a4 (virtual)
  edata  0x0000000080204010 (virtual)
  end    0x0000000080204028 (virtual)
Kernel executable memory footprint: 17KB
++ setup timer interrupts
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
100 ticks
```

实现流程：
```
1.按设置下次时钟中断，clock_set_next_event()函数告诉定时器硬件在多少时间后再次触发中断；
2.计数器ticks加一；
3.判断ticks是否是TICK_NUM(100)的倍数,如果是，调用print_ticks()函数，且更新打印次数num；
4.判断是否已经输出了10次100ticks信息，如果是，调用sbi_shutdown()函数。
```
定时器中断中断处理的流程：
```
1.定时器达到预设的时间后，向CPU发送中断请求。
2.CPU保存当前任务的状态，包括程序计数器（PC）、寄存器、程序状态字（PSW）等。
3.CPU跳转到中断向量表中对应的中断处理程序地址，执行中断程序。
4.中断处理程序执行完毕后，恢复之前保存的任务状态。
5.中断处理程序通过特定的中断返回指令返回到被中断的任务。
6.CPU继续执行被中断的任务。
```


### 扩展练习 Challenge1：描述与理解中断流程
描述ucore中处理中断异常的流程（从异常的产生开始），其中mov a0，sp的目的是什么？SAVE_ALL中寄寄存器保存在栈中的位置是什么确定的？对于任何中断，__alltraps 中都需要保存所有寄存器吗？请说明理由。
#### 1.异常处理的步骤如下：
1.1  异常产生后，会跳转到异常处理程序入口，这个地址是通过一个CSR寄存器`stvec`找到的，`stvec`寄存器中就是“中断向量表基址”，中断向量表会将不同种类的中断映射到对应的中断处理程序，如果只有一个中断处理程序，`stvec`就直接指向的是中断处理程序的入口地址。我们的实验中是将对不同的中断类型放到了一个中断处理程序中，在内核初始化时将该寄存器设置为`__alltraps`，所以中断异常产生时会跳转到trapentry.S中的`__alltraps`标签处执行。

1.2 跳转到`__alltraps`标签处，首先要在中断之前保存当前CPU中的执行状态，即保存所有的寄存器`SAVE_ALL`，然后执行`mov a0,sp`将sp保存到a0中，之后跳转到trap函数继续执行。

1.3 再跳转到trap函数中，调用`trap_dispatch`函数，判断异常是中断还是异常，分别跳转到对应的处理函数`interrupt_handler`或`expection_handler`处根据`cause`的值执行相应的异常处理程序。
`cause` 是从 `struct trapframe` 中的 `cause`字段获取的，包含了发生的异常类型或中断类型的编码。该值由 RISC-V 处理器的 `scause`寄存器提供，`scause` 时用于指示具体中断或异常原因的寄存器。
```
static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        // interrupts
        interrupt_handler(tf);
    } else {
        // exceptions
        exception_handler(tf);
    }
}
```
上述的代码就是通过比较cause和0的大小来判断是中断还是异常。
```
 void interrupt_handler(struct trapframe *tf)中的switch语句判断：

 intptr_t cause = (tf->cause << 1) >> 1;
```
这是将cause的高位符号位去掉，看低位的编码来处理相应的中断类型。

#### 2.执行mov a0,sp的原因
该指令是把保存context之后的栈顶指针寄存器赋值给a0，我们知道a0寄存器是一个参数寄存器，接下来会调用trap函数，就会将中断帧作为参数传递给中断处理程序，从而进行中断处理。
####  3.寄存器保存的位置？
各个寄存器保存的位置当前栈指针 sp 作为基准，我们可以从SAVE_ALL的代码中发现，下面就是为上下文开辟一个存储空间，之后我们就可以通过栈顶指针sp来访问该段区域的不同位置，从而将寄存器进行保存。
```
csrw sscratch, sp
addi sp, sp, -36 * REGBYTES
```
#### 4.是否需要保存所有的寄存器？
不需要保存所有的寄存器，我们知道保存寄存器的目的就是避免对其原有的内容进行修改，但是这个是需要跟具体的中断处理的过程相结合的，对于一些中断程序只用到了几个寄存器，那我们只需要保存对应的寄存器的值如sepc、status、部分的通用寄存器等，如果一味的所有都保存会占用较多的资源。

### 扩展练习Challenge2：理解上下文切换机制
在trapentry.S中汇编代码 csrw sscratch, sp；csrrw s0, sscratch, x0实现了什么操作，目的是什么？save all里面保存了stval scause这些csr，而在restore all里面却不还原它们？那这样store的意义何在呢？
```
1.
csrw sscratch, sp；
操作：将当前的栈指针(sp)保存到sscratch这个特殊功能寄存器中。sscratch寄存器用于在特权模式之间进行栈指针的保存和恢复，以支持在不同特权级别之间进行切换时保持栈指针的连续性。
目的：为了在发生中断时，能够将用户栈指针保存下来，然后在内核栈上进行中断处理，从而保护了用户态的栈不被破坏。

csrrw s0, sscratch, x0；
操作：首先将sscratch寄存器的值读到s0寄存器中，然后将零寄存器x0赋值给sscratch寄存器。这实际上是将内核栈的栈顶指针值保存到了s0，并将sscratch寄存器重置为0。
目的：在中断处理完成后，能够恢复到用户栈，继续执行用户态的代码。同时将sscratch寄存器重置为0，这样如果产生了递归异常，异常向量就会知道它来自于内核而不是用户模式。

2.
stval 寄存器用于存储与某些异常相关的额外信息。它的内容取决于发生的异常类型。对于不同的异常，stval 可能包含以下信息
    对于加载/存储地址错误异常，stval 包含了引发异常的无效地址。
    对于整数除以零异常，stval 包含了被除数。
    对于系统调用异常，stval 包含了系统调用号。

scause 寄存器包含两部分信息：
    异常编码：指示具体的异常类型。
    中断标志：一个比特位，用来区分异常（该位为0）和中断（该位为1）。

stval寄存器保存了触发异常的存储器地址值，这个值在异常处理过程中可能会被用到，但在返回时不需要恢复，因为异常处理程序已经处理了这个值。
scause寄存器保存了异常的原因，这个值在异常处理过程中同样会被用到，但在返回时也不需要恢复，因为sret指令会根据当前的sstatus寄存器中的SPP字段来决定返回到哪个特权级别，并且 sepc 寄存器已经保存了异常发生时的程序计数器值，所以返回时可以直接跳转到正确的位置继续执行。

保存这些CSR的意义在于它们提供了中断发生时的上下文信息，这对于调试和异常处理是非常重要的。但在恢复时，只需要恢复那些对于返回到原来执行流程所必需的寄存器值，而stval和scause并不需要恢复到原来的状态。
```

### 扩展练习Challenge3：完善异常中断

编码完善的代码如下：

```
 case CAUSE_ILLEGAL_INSTRUCTION:
             // 非法指令异常处理
             /* LAB1 CHALLENGE3   YOUR CODE: */
            /*(1)输出指令异常类型（ Illegal instruction）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
            */
            cprintf("Exception type:Illegal instruction\n");
            cprintf("Illegal instruction caught at 0x%lx\n", tf->epc);
            tf->epc += 4; 
            break;
        case CAUSE_BREAKPOINT:
            //断点异常处理
            /* LAB1 CHALLLENGE3   YOUR CODE:*/
            /*(1)输出指令异常类型（ breakpoint）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
            */
            cprintf("Exception type: breakpoint \n");
            cprintf("ebreak caught at 0x%lx\n", tf->epc);
	         tf->epc += 4;
            break;
```

在kern_init函数中，intr_enable();之后写入两行

```
asm("mret");
asm("ebreak");
```

输出结果如下：
```
sbi_emulate_csr_read: hartid0: invalid csr_num=0x302
Exception type:Illegal instruction
Illegal instruction caught at 0x8020004e
Exception type: breakpoint 
ebreak caught at 0x80200052�
```

查阅相关资料后，发现是因为ebreak的指令只占2字节，+4后错误地指向下一条指令的中间，输出�，故CAUSE_BREAKPOINT中的"更新 tf->epc寄存器"代码应修改为tf->epc += 2;

输出结果如下：
```
sbi_emulate_csr_read: hartid0: invalid csr_num=0x302
Exception type:Illegal instruction
Illegal instruction caught at 0x8020004e
Exception type: breakpoint 
ebreak caught at 0x80200052
```

### 重要的知识点
#### 1.异常中断的执行流
中断/异常触发->保存上下文到栈中->进入中断处理代码->按照中断类型处理->结束处理，恢复上下文，返回触发中断的下一条指令继续执行
#### 2.CSR寄存器
CSR寄存器在处理中断异常的时候各自的作用，如何进行处理各类的中断异常。其中：
```
sepc/epc：指向发生异常的指令
stvec：保存发生异常时跳转到异常处理程序的地址
scause：指向发生异常的种类,最高位是1是中断；如果最高位是0是异常
```
#### 3.中断之后的恢复
我们可以看到并不需要将所有的csr寄存器恢复，有的csr已经在中断处理过程中使用过，就没有必要再恢复。