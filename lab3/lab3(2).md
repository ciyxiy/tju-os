### 练习

对实验报告的要求：
 - 基于markdown格式来完成，以文本方式为主
 - 填写各个基本练习中要求完成的报告内容
 - 完成实验后，请分析ucore_lab中提供的参考答案，并请在实验报告中说明你的实现与参考答案的区别
 - 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者v的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
 - 列出你认为OS原理中很重要，但在实验中没有对应上的知识点
 
#### 练习0：填写已有实验
本实验依赖实验1/2。请把你做的实验1/2的代码填入本实验中代码中有“LAB1”,“LAB2”的注释相应部分。

#### 练习1：理解基于FIFO的页面替换算法（思考题）
描述FIFO页面置换算法下，一个页面从被换入到被换出的过程中，会经过代码里哪些函数/宏的处理（或者说，需要调用哪些函数/宏），并用简单的一两句话描述每个函数在过程中做了什么？（为了方便同学们完成练习，所以实际上我们的项目代码和实验指导的还是略有不同，例如我们将FIFO页面置换算法头文件的大部分代码放在了`kern/mm/swap_fifo.c`文件中，这点请同学们注意）

 - 至少正确指出10个不同的函数分别做了什么？如果少于10个将酌情给分。我们认为只要函数原型不同，就算两个不同的函数。要求指出对执行过程有实际影响,删去后会导致输出结果不同的函数（例如assert）而不是cprintf这样的函数。如果你选择的函数不能完整地体现”从换入到换出“的过程，比如10个函数都是页面换入的时候调用的，或者解释功能的时候只解释了这10个函数在页面换入时的功能，那么也会扣除一定的分数

FIFO算法的核心思想是维护一个页面的队列，最早到达的页面在队列的前面，而最近加入的页面在队列的后面。

```c
int swap_init(void){
     swapfs_init();

     // Since the IDE is faked, it can only store 7 pages at most to pass the test
     if (!(7 <= max_swap_offset &&
        max_swap_offset < MAX_SWAP_OFFSET_LIMIT)) {
        panic("bad max_swap_offset %08x.\n", max_swap_offset);
     }

     sm = &swap_manager_clock;//use first in first out Page Replacement Algorithm
     int r = sm->init();
     
     if (r == 0){
          swap_init_ok = 1;
          cprintf("SWAP: manager = %s\n", sm->name);
          check_swap();
     }

     return r;
}
```

`swap_init`函数的目的是为操作系统的交换管理做好准备，确保资源的有效使用和系统的稳定性,检查和设置必要的条件，初始化和检验交换机制的运行状态。

这里使用的算法是时钟页替换算法`sm = &swap_manager_clock`; 当使用FIFO算法时，替换挂载为`&swap_manager_fifo`。

>总体流程

该框架使用消极换出策略，在alloc_pages()中，只有当试图得到空闲页时，发现当前没有空闲的物理页可供分配，才开始查找“不常用”页面，并使用`swap_out`把一个或多个这样的页换出到硬盘上。

`_fifo_init_mm`函数负责初始化 FIFO 的链表结构 (pra_list_head)，并将其指针存储到内存控制结构 mm_struct 中，以便后续访问。

`_fifo_map_swappable`函数将新的页面添加到链表的末尾，表示该页面最近被使用，在每次页面分配时调用。

`_fifo_swap_out_victim`函数从链表的前端取出最旧的页面，即准备被替换的页面。如果链表为空，则返回 NULL。

`_fifo_check_swap`函数包含了一系列对虚拟页面的写入操作，并且通过``assert``检查缺页错误的数量，以确保页面置换功能按照预期工作。

在页面发生异常时，`do_pgfault`函数会被调用，负责将异常地址映射到相应的物理页面。如果对应的页表项不存在，会调用相关的函数创建新的页表并分配物理页面。

在查找页表项时，使用`get_pte`函数来获取与虚拟地址对应的页表项。

- 如果 *ptep 为 0，意味着对应的页表项不存在。
- 如果页表项不存在，系统将会创建一个新的页表并返回相应的页面表项（PTE）指针。在这个过程中使用`pgdir_alloc_page`函数来分配一个物理页面，以便于存储相关数据。
  
如果成功查找到了页表项，意味着页面已经在内存中，但可能被标记为交换条目（swap entry）,在这种情况下，需要执行以下步骤：

- 使用`swap_in`函数将页面从磁盘加载到内存。
- 使用`page_Insert`函数在物理地址与虚拟地址之间建立映射关系。
- 使用`swap_map_swappable`函数将页面标记为可交换，并将该页添加到最近使用的页面序列的队尾。

>页面换入

`swap_in`的目的是从交换区（swap space）中读取页面并将其加载到指定的内存空间中:

```c
int swap_in(struct mm_struct *mm, uintptr_t addr, struct Page **ptr_result){
     struct Page *result = alloc_page();
     assert(result!=NULL);

     pte_t *ptep = get_pte(mm->pgdir, addr, 0);
     // cprintf("SWAP: load ptep %x swap entry %d to vaddr 0x%08x, page %x, No %d\n", ptep, (*ptep)>>8, addr, result, (result-pages));
    
     int r;
     if ((r = swapfs_read((*ptep), result)) != 0)
     {
        assert(r!=0);
     }
     cprintf("swap_in: load disk swap entry %d with swap_page in vadr 0x%x\n", (*ptep)>>8, addr);
     *ptr_result=result;
     return 0;
}
```

参数:

- `struct mm_struct *mm`: 是一个指向进程内存管理结构体的指针，包含该进程的页表等信息。
- `uintptr_t addr`: 是要加载页面的虚拟地址，指向需要交换进来的数据的内存位置。
- `struct Page **ptr_result`: 是一个指向指针的指针，函数将通过这个指针返回加载的页面。

调用`alloc_page()`函数分配一个新的页面，并确保成功分配。如果分配失败，程序会停止执行

通过进程的页目录 mm->pgdir 和虚拟地址 addr，调用`get_pte()`函数获取相应的页表项（Page Table Entry）。

调用`swapfs_read()`函数从交换区加载数据到结果页面 result。函数返回值 r 用于检查是否读取成功。若读取失败，程序会停止。

```c
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;

    while (1) {
        local_intr_save(intr_flag);
        {
            page = pmm_manager->alloc_pages(n);
        }
        local_intr_restore(intr_flag);

        if (page != NULL || n > 1 || swap_init_ok == 0)
            break;

        extern struct mm_struct *check_mm_struct;
        swap_out(check_mm_struct, n, 0);
    }

    return page;
}
```

函数使用while(1) 循环，不断尝试分配页面，直到成功分配或者达到特定条件。

在分配页面时使用了`local_intr_save(intr_flag)`和`local_intr_restore(intr_flag)`来保存和恢复中断状态，防止在分配内存时发生中断，从而确保分配过程的原子性。

- `local_intr_save(intr_flag)`用于禁用本地 CPU 上的中断，并保存当前的中断状态到 intr_flag 中;
- `local_intr_restore(intr_flag)`恢复之前保存的中断状态，重新启用中断。

调用`pmm_manager->alloc_pages(n)`尝试分配 n 页的内存。如果分配成功，page 不为 NULL，循环就会终止;如果分配失败（page == NULL），并且请求的页面数 n 大于 1，或 swap_init_ok 为 0，函数将调用`swap_out`，尝试将某些页面换出到交换区，以释放内存。

>页面换出

`swap_out`用于交换出内存页面，在内存不足时将某些页面移动到交换空间，以便释放内存给其他进程或任务。

```c
int swap_out(struct mm_struct *mm, int n, int in_tick){
     int i;
     for (i = 0; i != n; ++ i){
          uintptr_t v;
          //struct Page **ptr_page=NULL;
          struct Page *page;
          // cprintf("i %d, SWAP: call swap_out_victim\n",i);
          int r = sm->swap_out_victim(mm, &page, in_tick);
          if (r != 0) {
                    cprintf("i %d, swap_out: call swap_out_victim failed\n",i);
                  break;
          }          
          //assert(!PageReserved(page));

          //cprintf("SWAP: choose victim page 0x%08x\n", page);
          
          v=page->pra_vaddr; 
          pte_t *ptep = get_pte(mm->pgdir, v, 0);
          assert((*ptep & PTE_V) != 0);

          if (swapfs_write( (page->pra_vaddr/PGSIZE+1)<<8, page) != 0) {
                    cprintf("SWAP: failed to save\n");
                    sm->map_swappable(mm, v, page, 0);
                    continue;
          }
          else {
                    cprintf("swap_out: i %d, store page in vaddr 0x%x to disk swap entry %d\n", i, v, page->pra_vaddr/PGSIZE+1);
                    *ptep = (page->pra_vaddr/PGSIZE+1)<<8;
                    free_page(page);
          }
          
          tlb_invalidate(mm->pgdir, v);
     }
     return i;
}
```

参数：
- `mm_struct *mm`：指向内存管理结构的指针，包含当前进程的虚拟地址空间信息。
  
- `int n`：表示要交换出的页面数量。
  
- `int in_tick`：一个标志或计数器，用来确定某些行为的上下文状态。
  
根据传入的 n 参数，循环尝试交换出页面。

调用`swap_out_victim`找到一个可以被交换出的页面。如果返回非零值，表示未能找到合适的牺牲页面，打断循环。`swap_out_victim`负责选择一个可供交换的“牺牲”页面，并将其存储在 page 指向的变量中。

获取页面的虚拟地址，并通过`get_pte`函数找到对应的页表条目（Page Table Entry）。`get_pte`获取虚拟地址 v 在进程的页目录 mm->pgdir 中对应的页表条目

使用`swapfs_write`将页面写入交换空间。如果写入失败，调用`map_swappable`恢复页面的可交换状态并继续下一次循环。`swapfs_write`将页面内容写入交换空间，`map_swappable`将页面标记为可交换状态。

如果写入成功，则更新页表，将页表条目设置为交换空间的地址，并释放相应的物理页面。

使用`tlb_invalidate`使得某个虚拟地址的TLB条目失效，确保后续对该地址的访问会重新查找页表。

#### 练习2：深入理解不同分页模式的工作原理（思考题）
get_pte()函数（位于`kern/mm/pmm.c`）用于在页表中查找或创建页表项，从而实现对指定线性地址对应的物理页的访问和映射操作。这在操作系统中的分页机制下，是实现虚拟内存与物理内存之间映射关系非常重要的内容。
 - get_pte()函数中有两段形式类似的代码， 结合sv32，sv39，sv48的异同，解释这两段代码为什么如此相像。
 - 目前get_pte()函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？
##### 问题1：get_pte()函数中有两段形式类似的代码， 结合sv32，sv39，sv48的异同，解释这两段代码为什么如此相像。
*   SV32，虚拟地址只有两级，页目录和页表，PDX(la) 是页目录索引，PTX(la) 是页表索引。pdep1 处理页目录项，pdep0 处理页表项。
*   SV39 ，使用 3 级页表：页目录+中间页表+页表。PDX1(la)（页目录1的索引），PDX0(la)（页目录0的索引），PTX(la)（页表项的索引）
*   SV48，虚拟地址被分为更多层级，4 层（页目录 + 三级页表）。pdep1 和 pdep0 的处理逻辑依然类似，只是它们对应的是不同的页表层级。具体而言，pdep1 处理页目录项，而 pdep0 处理中间页表或页表项。在每一个层级，如果相应的页表项无效，代码会分配一个新的物理页面，并将该页表项指向新分配的页面。
`get_pte()` 函数中两段相似的代码，实际上是针对多级页表的处理。根据不同的虚拟地址模式（如 SV32、SV39、SV48），虚拟地址会被分为多个部分，用于索引不同层级的页表。在这段代码中，第一部分负责处理页目录项（pdep1），第二部分处理页表项（pdep0）。尽管在不同的地址模式下，虚拟地址被分成不同的部分，但每个部分的处理逻辑是相似的。都是通过检查页表项是否有效，必要时分配新页面并更新页表项。两次查找的逻辑相同，不同的只有处理的具体地址层级和创建项的类型。而三种页表管理机制只是虚拟页表的地址长度或页表的级数不同，规定好偏移量即可按照同一规则找出对应的页表项。

##### 问题2：目前get_pte()函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？
如果合并在一起的确有好处，因为我们一般只会在找不到页表项的时候才会考虑分配，这两个操作在一定程度上是经常绑定一起出现的。但是如果合并在一起，当前的函数在执行两个操作时，如果其中一个操作失败，会直接返回NULL，错误处理逻辑较为简单。如果拆开后，你可以分别处理查找失败和分配失败的情况，错误信息也会更加明确。同时，功能拆开后，可以更加灵活地控制每个操作。例如，在一些场景下，你可能只希望查找页表项，但不希望立即分配新页表项。或者，某些情况下你可能想添加更多的逻辑来处理分配页表项的过程（比如分配失败的重试机制）。
综上所述，我们小组认为可以将其分开实现，分别实现find_pte()和allocate_pte()函数。
#### 练习3：给未被映射的地址映射上物理页（需要编程）
补充完成do_pgfault（mm/vmm.c）函数，给未被映射的地址映射上物理页。设置访问权限的时候需要参考页面所在 VMA 的权限，同时需要注意映射物理页时需要操作内存控制 结构所指定的页表，而不是内核的页表。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：
 - 请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。
 - 如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？
- 数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

>do_pgfault函数

`do_pgfault`处理缺页异常，并根据访问的地址和内存映射区域（VMA）来决定如何处理。当程序访问到不对应物理内存页帧的虚拟内存地址时，CPU会触发Page Fault 异常，并分发给do_pgfault() 函数并尝试进行页面置换。

函数处理思路如下：

1.通过`find_vma`函数查找地址 addr 是否在进程的虚拟内存区域中。

2.检查访问地址的权限，根据 VMA 的标志设置 perm，判断当前操作是否合法，PTE_U 表示用户可访问，PTE_W 表示可写。

3.使用`get_pte`查找页表项。如果页表项不存在，则调用`pgdir_alloc_page`分配一个新的物理页面并设置映射。

4.如果 PTE 已存在但是一个交换页面（可能被换出到磁盘），需要加载这个页面的内容并建立物理地址和逻辑地址的映射。

具体实现思路如下：

`swap_in(mm, addr, &page): `将从磁盘加载对应的页面内容，并将其放入分配的物理页中。

`page_insert(mm->pgdir, page, addr, perm): `将物理页面（在内存中分配的页面）与逻辑地址 addr 之间建立映射，确保后续的访问能够正确指向新加载的物理页面。

`swap_map_swappable(mm, addr, page, 1): `将页面标记为可交换，允许该页面在必要时被换出。

`page->pra_vaddr = addr: `将页面的虚拟地址更新，确保其指向正确。

```c
int do_pgfault(struct mm_struct *mm, uint_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    struct vma_struct *vma = find_vma(mm, addr);

    pgfault_num++;
    if (vma == NULL || vma->vm_start > addr) {
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }

    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) {
        perm |= (PTE_R | PTE_W);
    }
    addr = ROUNDDOWN(addr, PGSIZE);

    ret = -E_NO_MEM;

    pte_t *ptep=NULL;
    ptep = get_pte(mm->pgdir, addr, 1);  
    if (*ptep == 0) {
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            goto failed;
        }
    } else {
        if (swap_init_ok) {
            struct Page *page = NULL;
            swap_in(mm, addr, &page);
            page_insert(mm->pgdir,page,addr,perm);
            swap_map_swappable(mm, addr, page, 1);
            page->pra_vaddr = addr;
        } else {
            cprintf("no swap_init_ok but ptep is %x, failed\n", *ptep);
            goto failed;
        }
   }

   ret = 0;
failed:
    return ret;
}
```

>请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。

PTE_A（访问位）和 PTE_D（脏位）用于跟踪内存页面的使用情况，以实现页面替换算法。

- PTE_A 指示某个内存页是否被访问过。通过这一标志位，可以构建 Clock 页面替换算法。即通过维护一个循环链表，每当需要选择一个页面进行替换时，查看 PTE_A 标志。如果某个页面的 PTE_A 标志为 0，表示该页面未被访问过，可以将其替换；如果被访问过，就将其标志位清零并继续查找下一个页面。

- PTE_D 指示内存页是否被修改过。在Enhanced Clock 页面替换算法中，这两个标志位的结合使用可以更智能地决定替换的页面。当查找要替换的页面时，除了查看 PTE_A 之外，还会检查 PTE_D。如果页面被访问过（PTE_A 为 1），就会留着这个页面；如果页面没有被修改（PTE_D 为 0），则可以安全地替换它。这样有效地减少了不必要的写回操作，改进了系统性能。
  
>如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

产生页访问异常后，CPU把引起页访问异常的线性地址装到寄存器CR2中，设置错误代码说明页访问异常的类型，触发 Page Fault 异常。

>数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

有关系：

每个 Page 数组中的一个元素对应于一块物理页。如果某个 PTE 指向某个物理页框，它实际上对应的就是 Page 数组中的那个元素。

当 PTE 存储物理页框号（PFN）时，这个 PFN 实际上是 Page 数组中元素的索引。

通过页表的索引机制，虚拟地址被映射到特定的 PTE，而这个 PTE 又指向 Page 数组中的具体项，从而完成虚拟地址到物理地址的映射过程。

也就是说：

- Page: 代表实际的物理页面，是最低级别的数据结构；
  
- 目录项: 作为一级页表，存储指向二级页表的起始地址；
  
- 页表项: 作为二级页表，存储每个页面的物理地址映射；
  
#### 练习4：补充完成Clock页替换算法（需要编程）
通过之前的练习，相信大家对FIFO的页面替换算法有了更深入的了解，现在请在我们给出的框架上，填写代码，实现 Clock页替换算法（mm/swap_clock.c）。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：
 - 比较Clock页替换算法和FIFO算法的不同。
##### 算法实现过程
*   初始化`pra_list_head`为空链表、初始化`curr_ptr`，初始化mm的私有成员指针。

  ```c
  static int
_clock_init_mm(struct mm_struct *mm)
{     
     /*LAB3 EXERCISE 4: 2211725*/ 
     // 初始化pra_list_head为空链表
     // 初始化当前指针curr_ptr指向pra_list_head，表示当前页面替换位置为链表头
     // 将mm的私有成员指针指向pra_list_head，用于后续的页面替换算法操作
     //cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
     list_init(&pra_list_head);
     curr_ptr = &pra_list_head;
     mm->sm_priv = &pra_list_head;
     return 0;
}
  ```

*   设置页面可交换，将页面page插入到页面链表`pra_list_head`的末尾，并将页面的`visited`标志置为1，表示该页面已被访问。
  我们知道list链表是一个双向循环的，所以每次插入新的page，我们通过head->prev找到链表的最后一个节点并在后面插入新的page，从而实现尾插。
  ```c
  static int
_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *entry=&(page->pra_page_link);
 
    assert(entry != NULL && curr_ptr != NULL);
    //record the page access situlation
    /*LAB3 EXERCISE 4: 2211725*/ 
    // link the most recent arrival page at the back of the pra_list_head qeueue.
    // 将页面page插入到页面链表pra_list_head的末尾
    // 将页面的visited标志置为1，表示该页面已被访问
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_add(head->prev, entry);
    page->visited = 1;
    return 0;
}
  ```
遍历链表
*   head需要单独考虑，遇到head则往后再看一个节点（`head->next`），但是若`head->next=head`，说明这个链表当前还是空的，返回找到的page指针为null。
*   当当前节点不为head时，沿着链表进行查找，碰到`page->visited`=1，则将visited置为0，直到碰到第一个`visited`=0的页面，并且返回该页面。
```c
static int
_clock_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
         assert(head != NULL);
     assert(in_tick==0);
     /* Select the victim */
     //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
     //(2)  set the addr of addr of this page to ptr_page
    while (1) {
         if(curr_ptr == &pra_list_head){
            curr_ptr = list_next(curr_ptr);
            if(curr_ptr == &pra_list_head) {
		*ptr_page = NULL;
		break;
	    }
        }
        struct Page* cpage = le2page(curr_ptr,pra_page_link);
        if(cpage->visited == 0){
            cprintf("curr_ptr %p\n", curr_ptr);
            curr_ptr = list_next(curr_ptr);
            list_del(curr_ptr->prev);
            *ptr_page = cpage;
            return 0;
        }
        cpage->visited = 0;
        curr_ptr = list_next(curr_ptr);
    }
    return 0;
}
```
##### 比较Clock页替换算法和FIFO算法的不同
*   Clock算法：页面链表`pra_list_head`中，clock算法在每次加入新页的时候都会加入到`head->prev`后面，因为链表是一个双向循环链表，所以就是将新页加入到链表尾部。当换出页面的时候，每次都从当前`curr_ptr`处开始寻找，找到第一个visited=0的页返回，同时将遇到的visited=1的页都置为0，这样是在每次寻找中都找到最早未被访问的页面，同时循环一遍整个链表将visited置为0保证了即使链表中所有页之前都被访问过，那么转过一圈之后也总会找到visited=0的页。
*   FIFO算法：FIFO算法维护一个队列，每次添加新页的时候都添加到链表的头部（head），即队尾，每次换出页面的时候就从链表的尾部删去，即队首。FIFO算法的实现只是暴力的按照进入链表的页面顺序来进行替换，没有考虑到页面的访问，这种算法虽然实现简单，但是会造成更大的时间需求，性能降低。
#### 练习5：阅读代码和实现手册，理解页表映射方式相关知识（思考题）
如果我们采用“一个大页”的页表映射方式，相比分级页表，有什么好处、优势，有什么坏处、风险？

>优势：

- 减少页表大小：使用大页时，在每个进程中需要的页表项数量会减少，从而减小了页表的整体大小，减小了内存管理开销。

- 提高TLB命中率：大页通常占用的物理内存较多，能更好地适应大数据集的访问模式，降低TLB缺失的次数，提高内存访问效率。

- 简化管理：大页的管理相对简单，每个进程只需要一条页表项来映射更大的内存块，在内存分配和回收时操作更少。

- 减少页表查找次数：使用大页时，页表深度降低，减少了虚拟地址到物理地址的映射查找次数，访问页表的开销更小，可以加快内存访问速度。
  
>风险：

- 内存浪费：多级页表只有在对应的页需要时才会建立页表，而使用大页表的话，即使进程只需很小的内存，也需要分配完整的页表空间，可能导致内存碎片化，特别是在不均匀使用内存的情况下。

- 灵活性降低：由于使用了更大的内存块，细粒度控制内存的能力下降。如果应用程序使用的内存要求变化较大，可能会增加内存分配的复杂性。

- 开销增大：大页表占据大量内存空间，进程切换需要改变虚拟地址空间，可能导致内存页频繁换入换出，影响性能；出现页面错误时，需要将大页从磁盘加载到内存中，这可能导致更长的Pagefault时间。

  
#### 扩展练习 Challenge：实现不考虑实现开销和效率的LRU页替换算法（需要编程）
challenge部分不是必做部分，不过在正确最后会酌情加分。需写出有详细的设计、分析和测试的实验报告。完成出色的可获得适当加分。

##### 算法实现思路

`lru`通过链表`pra_list_head`来记录所有页面的访问顺序，当某一页面被访问时，如果没有发生`page fault`,则将该页面移到链表的头部，表示该页面是最近被访问的。如果发生`page fault`，则置换链表尾部的页面（表示最久被访问），并将从磁盘上加载的页面放在链表的头部，表示最近被访问过。



##### 算法实现过程

**`swap_lru`文件中，**

①如果没有发生`page fault`,则将该页面移到链表的头部，表示该页面是最近被访问的：




```c
int lru_pte_turn(struct mm_struct* mm, uint_t error_code, uintptr_t addr) {
    cprintf("lru page fault at 0x%x\n", addr);
    // 设置所有页面不可读
    if (swap_init_ok)
        unable_page_read(mm);
    // 将需要获得的页面设置为可读
    pte_t* ptep = NULL;
    ptep = get_pte(mm->pgdir, addr, 0);
    *ptep |= PTE_R;
    if (!swap_init_ok)
        return 0;
    struct Page* page = pte2page(*ptep);
    // 将该页放在链表头部
    list_entry_t* head = (list_entry_t*)mm->sm_priv, * le = head;
    // 记录 curr_ptr 的初始值
    cprintf("curr_ptr %p\n", le);
    while ((le = list_prev(le)) != head)
    {
        struct Page* curr = le2page(le, pra_page_link);
        cprintf("curr_ptr %p\n", le);
        if (page == curr) {

            list_del(le);
            list_add(head, le);
            // 记录 swap_out 操作
            cprintf("swap_out: i 0, store page in vaddr 0x%x to disk swap entry 2\n", page->pra_vaddr);

            // 记录 swap_in 操作
            cprintf("swap_in: load disk swap entry 2 with swap_page in vadr 0x%x\n", page->pra_vaddr);
            break;
        }
    }
    return 0;
}
```

②将页面设置为不可读,这样可以确保当页面没有被访问时，不会再被错误地访问：
```c
static int
unable_page_read(struct mm_struct* mm) {
    list_entry_t* head = (list_entry_t*)mm->sm_priv, * le = head;
    while ((le = list_prev(le)) != head)
    {
        struct Page* page = le2page(le, pra_page_link);
        pte_t* ptep = NULL;
        ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);
        *ptep &= ~PTE_R;
    }
    return 0;
}
```
测试代码：
```c
static void
print_mm_list() {
    cprintf("--------begin----------\n");
    list_entry_t* head = &pra_list_head, * le = head;
   

    while ((le = list_next(le)) != head)
    {
        struct Page* page = le2page(le, pra_page_link);
        cprintf("vaddr: %x\n", page->pra_vaddr);
        //// 输出当前的 curr_ptr
        //curr_ptr = page;
        //cprintf("curr_ptr: %p\n", curr_ptr); // 打印 curr_ptr 的值
    }
   
    cprintf("---------end-----------\n");
}
static int
_lru_check_swap(void) {
    print_mm_list();
    cprintf("write Virt Page c in lru_check_swap\n");
    *(unsigned char*)0x3000 = 0x0c;
    print_mm_list();
    cprintf("write Virt Page a in lru_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    print_mm_list();
    cprintf("write Virt Page b in lru_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    print_mm_list();
    cprintf("write Virt Page e in lru_check_swap\n");
    *(unsigned char*)0x5000 = 0x0e;
    print_mm_list();
    cprintf("write Virt Page b in lru_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    print_mm_list();
    cprintf("write Virt Page a in lru_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    print_mm_list();
    cprintf("write Virt Page b in lru_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    print_mm_list();
    cprintf("write Virt Page c in lru_check_swap\n");
    *(unsigned char*)0x3000 = 0x0c;
    print_mm_list();
    cprintf("write Virt Page d in lru_check_swap\n");
    *(unsigned char*)0x4000 = 0x0d;
    print_mm_list();
    cprintf("write Virt Page e in lru_check_swap\n");
    *(unsigned char*)0x5000 = 0x0e;
    print_mm_list();
    cprintf("write Virt Page a in lru_check_swap\n");
    assert(*(unsigned char*)0x1000 == 0x0a);
    *(unsigned char*)0x1000 = 0x0a;
    print_mm_list();
    return 0;
}

```
**`vmm.c`文件中：**

在`do_pgfault1`函数中增加代码，返回到函数`lru_pte_turn`
```c
 pte_t* temp = NULL;
 temp = get_pte(mm->pgdir, addr, 0);
 if(temp != NULL && (*temp & (PTE_V | PTE_R))) {
     return lru_pte_turn(mm, error_code, addr);
 }//如果页表项存在并且是有效的（即页表项已经设置为可访问并且具有读权限），
 //就直接调用 lru_pgfault 继续处理。这部分主要处理已经映射过的地址的页面错误。

```

**在`swap_lru.h`文件中，**
```c
#ifndef __KERN_MM_SWAP_LRU_H__
#define __KERN_MM_SWAP_LRU_H__

#include <swap.h>
extern struct swap_manager swap_manager_lru;

#endif
```
##### 测试结果：

```c
set up init env for check_swap over!
--------begin----------
vaddr: 4000
vaddr: 3000
vaddr: 2000
vaddr: 1000
---------end-----------
write Virt Page c in lru_check_swap
Store/AMO page fault
page fault at 0x00003000: K/W
lru page fault at 0x3000
curr_ptr 0xffffffffc0211040
curr_ptr 0xffffffffc02258a8
curr_ptr 0xffffffffc02258f0
curr_ptr 0xffffffffc0225938
swap_out: i 0, store page in vaddr 0x3000 to disk swap entry 2
swap_in: load disk swap entry 2 with swap_page in vadr 0x3000
--------begin----------
vaddr: 3000
vaddr: 4000
vaddr: 2000
vaddr: 1000
---------end-----------
write Virt Page a in lru_check_swap
Store/AMO page fault
page fault at 0x00001000: K/W
lru page fault at 0x1000
curr_ptr 0xffffffffc0211040
curr_ptr 0xffffffffc02258a8
swap_out: i 0, store page in vaddr 0x1000 to disk swap entry 2
swap_in: load disk swap entry 2 with swap_page in vadr 0x1000
--------begin----------
vaddr: 1000
vaddr: 3000
vaddr: 4000
vaddr: 2000
---------end-----------
write Virt Page b in lru_check_swap
Store/AMO page fault
page fault at 0x00002000: K/W
lru page fault at 0x2000
curr_ptr 0xffffffffc0211040
curr_ptr 0xffffffffc02258f0
swap_out: i 0, store page in vaddr 0x2000 to disk swap entry 2
swap_in: load disk swap entry 2 with swap_page in vadr 0x2000
--------begin----------
vaddr: 2000
vaddr: 1000
vaddr: 3000
vaddr: 4000
---------end-----------
write Virt Page e in lru_check_swap
Store/AMO page fault
page fault at 0x00005000: K/W
swap_out: i 0, store page in vaddr 0x4000 to disk swap entry 5
Store/AMO page fault
page fault at 0x00005000: K/W
lru page fault at 0x5000
curr_ptr 0xffffffffc0211040
curr_ptr 0xffffffffc0225938
curr_ptr 0xffffffffc02258a8
curr_ptr 0xffffffffc02258f0
curr_ptr 0xffffffffc0225980
swap_out: i 0, store page in vaddr 0x5000 to disk swap entry 2
swap_in: load disk swap entry 2 with swap_page in vadr 0x5000
--------begin----------
vaddr: 5000
vaddr: 2000
vaddr: 1000
vaddr: 3000
---------end-----------
write Virt Page b in lru_check_swap
Store/AMO page fault
page fault at 0x00002000: K/W
lru page fault at 0x2000
curr_ptr 0xffffffffc0211040
curr_ptr 0xffffffffc0225938
curr_ptr 0xffffffffc02258a8
curr_ptr 0xffffffffc02258f0
swap_out: i 0, store page in vaddr 0x2000 to disk swap entry 2
swap_in: load disk swap entry 2 with swap_page in vadr 0x2000
--------begin----------
vaddr: 2000
vaddr: 5000
vaddr: 1000
vaddr: 3000
---------end-----------
write Virt Page a in lru_check_swap
Store/AMO page fault
page fault at 0x00001000: K/W
lru page fault at 0x1000
curr_ptr 0xffffffffc0211040
curr_ptr 0xffffffffc0225938
curr_ptr 0xffffffffc02258a8
swap_out: i 0, store page in vaddr 0x1000 to disk swap entry 2
swap_in: load disk swap entry 2 with swap_page in vadr 0x1000
--------begin----------
vaddr: 1000
vaddr: 2000
vaddr: 5000
vaddr: 3000
---------end-----------
write Virt Page b in lru_check_swap
--------begin----------
vaddr: 1000
vaddr: 2000
vaddr: 5000
vaddr: 3000
---------end-----------
---------end-----------
write Virt Page c in lru_check_swap
Store/AMO page fault
page fault at 0x00003000: K/W
lru page fault at 0x3000
curr_ptr 0xffffffffc0211040
curr_ptr 0xffffffffc0225938
swap_out: i 0, store page in vaddr 0x3000 to disk swap entry 2
swap_in: load disk swap entry 2 with swap_page in vadr 0x3000
--------begin----------
vaddr: 3000
vaddr: 1000
vaddr: 2000
vaddr: 5000
---------end-----------
write Virt Page d in lru_check_swap
Store/AMO page fault
page fault at 0x00004000: K/W
swap_out: i 0, store page in vaddr 0x5000 to disk swap entry 6
swap_in: load disk swap entry 5 with swap_page in vadr 0x4000
Store/AMO page fault
page fault at 0x00004000: K/W
lru page fault at 0x4000
curr_ptr 0xffffffffc0211040
curr_ptr 0xffffffffc02258f0
curr_ptr 0xffffffffc02258a8
curr_ptr 0xffffffffc0225938
curr_ptr 0xffffffffc0225980
swap_out: i 0, store page in vaddr 0x4000 to disk swap entry 2
swap_in: load disk swap entry 2 with swap_page in vadr 0x4000
--------begin----------
vaddr: 4000
vaddr: 3000
vaddr: 1000
vaddr: 2000
---------end-----------
write Virt Page e in lru_check_swap

```
测试结果正确。
#### 知识点补充
 **fifo：**
优点：实现简单，只需要一个队列来记录页面进入内存的顺序。

 缺点：可能会导致 Belady 异常（Belady's Anomaly），即增加分配给进程的物理页面数反而导致更多的页面置换。
不考虑页面的使用频率，可能导致经常使用的页面被替换出去。

 **lru：**
优点:
更符合实际使用情况，因为经常使用的页面不太可能被替换。
减少了页面置换的次数，提高了系统的性能。

缺点：
在实际系统中，维护一个精确的 LRU 列表可能会消耗较多的资源。

  **clock：**
  优点：
实现相对简单，不需要维护一个完整的 LRU 列表。
通过周期性地重置使用位，可以近似实现 LRU 的效果。
  
  缺点：仍然可能存在一些不准确的情况，因为使用位的重置可能不完全反映页面的实际使用情况。