### 练习

对实验报告的要求：

- 基于markdown格式来完成，以文本方式为主
- 填写各个基本练习中要求完成的报告内容
- 完成实验后，请分析ucore_lab中提供的参考答案，并请在实验报告中说明你的实现与参考答案的区别
- 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
- 列出你认为OS原理中很重要，但在实验中没有对应上的知识点




#### 练习0：填写已有实验

本实验依赖实验1。请把你做的实验1的代码填入本实验中代码中有“LAB1”的注释相应部分并按照实验手册进行进一步的修改。具体来说，就是跟着实验手册的教程一步步做，然后完成教程后继续完成完成exercise部分的剩余练习。

#### 练习1：理解first-fit 连续物理内存分配算法（思考题）

first-fit 连续物理内存分配算法作为物理内存分配一个很基础的方法，需要同学们理解它的实现过程。请大家仔细阅读实验手册的教程并结合 `kern/mm/default_pmm.c`中的相关代码，认真分析default_init，default_init_memmap，default_alloc_pages， default_free_pages等相关函数，并描述程序在进行物理内存分配的过程以及各个函数的作用。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 你的first fit算法是否有进一步的改进空间？

`default_init`:初始化空闲块列表 `free_list`，将可用的物理页面数设为0

`default_init_memmap`:初始化从base开始的n个空闲页面，将空闲页面加到free_list中

* `base`指向要初始化的页面的指针，`n`是要初始化的数量，遍历从 `base`开始的 `n`个页面，设置初始化状态。
* 遍历free_list，如果 `base` 页面地址小于当前页面 `page` 的地址，说明应该将 `base` 页面插入到当前页面之前。如果当前页面是空闲列表中的最后一个页面，在空闲列表的末尾添加 `base` 页面。


`default_alloc_pages`:从空闲页面列表中分配 `n` 个页面。从列表头部遍历空闲页面列表，若找到第一个property大于n的空闲页面，则分割该页面，将剩余部分重新加入到空闲列表中。

* 检查当前可用的空闲页面数量 `nr_free` 是否足够。如果可用页面少于 `n`，则返回 `NULL`，表示无法满足请求。
* 定义一个指针 `page`，用于存储找到的足够大的空闲页面。定义一个指针 `le`，初始化为空闲页面列表的头部。遍历链表，当前页面 `p` 的 `property` 是否大于或等于 `n`。如果满足条件，说明找到了足够大的空闲页面，将其指针赋值给 `page` 并退出循环。

* 获取当前页面 `page` 在空闲列表中的前一个节点 `prev`。从空闲列表中删除当前页面 `page`。
* 检查 `page` 的 `property` 是否大于 `n`，即是否可以分割页面。定义一个新的页面指针 `p`，指向分配后剩余的页面，再将剩余页面 `p` 添加回空闲列表，保持列表的结构。

`default_free_pages`:将释放的内存块按照顺序插入free_list中，并与相邻的页面进行合并以减少碎片化。

* 若空闲链表 `free_list` 为空，直接将当前页面链入空闲链表；若不为空，遍历空闲链表，找找到第一个页面 `page`，其地址大于 `base`，将 `base` 插入在 `page` 之前。如果到达链表末尾，则将 `base` 插入到最后。

插入后尝试与前一个页面合并：

* 找到 `base` 前面的页面 `p`，如果 `p` 的结尾刚好和 `base` 相接（即 `p + p->property == base`），则将 `p` 和 `base` 合并，更新 `p->property`。
* 清除 `base` 的属性，并从空闲链表中删除 `base`，将 `base` 更新为合并后的起始页面 `p`。

尝试与后一个页面合并：

*  找到 `base` 后面的页面 `p`，如果 `base` 的结尾刚好和 `p` 相接（即 `base + base->property == p`），则将 `base` 和 `p` 合并，更新 `base->property`。

* 清除 `p` 的属性，并将 `p` 从空闲链表中删除。

#### 改进空间

1、开销太大：

从低地址开始搜索的时间复杂度为O(n),可以考虑使用AVL树等数据结构维护空闲块，其中按照中序遍历得到的空闲块序列的物理地址恰好按照从小到大排序，使用二分查找查找到物理地址最小的能够满足条件的空闲地址块。

2、延迟合并：

针对产生碎片的情况，设置一定的延迟合并策略，即积累到一定数量的小块页面后，再统一合并，减少合并操作的频率。

3、内存碎片：

长期运行中，可能会出现大量小的空闲页面块，导致无法为大块内存请求提供足够的连续空间。

可以将空闲页面分成多个大小类，使用不同的链表或其他数据结构分别管理各类页面。当需要分配内存时，首先从对应大小的页面链表中查找，减少遍历其他不匹配大小的页面的时间。

#### 练习2：实现 Best-Fit 连续物理内存分配算法（需要编程）

在完成练习一后，参考kern/mm/default_pmm.c对First Fit算法的实现，编程实现Best Fit页面分配算法，算法的时空复杂度不做要求，能通过测试即可。
请在实验报告中简要说明你的设计实现过程，阐述代码是如何对物理内存进行分配和释放，并回答如下问题：

- 你的 Best-Fit 算法是否有进一步的改进空间？

#### 核心代码实现

思路：找到大于等于n的最小空闲块

```
size_t min_size = nr_free + 1;
while ((le = list_next(le)) != &free_list) {
      struct Page *p = le2page(le, page_link);
      if (p->property >= n && p->property<min_size) {
          page = p;
          min_size = p->property;

    }
  }
```

  其他代码部分与first fit相同。

如何对物理内存进行分配和释放：

首先初始化物理内存管理器，设置 free_list 和 nr_free，在系统启动时，通过调用 best_fit_init_memmap 函数初始化可用的物理页面；在进程或内核代码中，使用 best_fit_alloc_pages 函数来分配物理页面，并使用 best_fit_free_pages 函数来释放页面。

分配：遍历 free_list 列表，查找满足需求的空闲页框。如果找到满足需求的页面，记录该页面以及当前找到的最小连续空闲页框数量。最终获得满足需求且连续空闲页数量最少的块，分配其中的页面，并将剩余的页面添加回 free_list 列表。如果没有找到满足条件的块，返回NULL

释放：将这些页面添加回 free_list 列表，并尝试合并相邻的空闲块，以最大程度地减少碎片化；更新 nr_free 变量以反映可用页面的数量。



#### 改进空间

1、开销太大：

best fit 比 first fit 开销更大，也可以考虑使用AVL树或跳表等数据结构维护空闲块，使查找效率从 O(n) 降低到 O(log n)。

2、不能很好适应内存多样化请求：

Best Fit 的适应性对于请求大小不均匀的场景表现较好，但在长期运行后，特别是大块和小块内存请求频繁交替出现时，Best Fit 可能会使得部分大块空闲空间无法有效利用。

结合分区分配策略，针对不同大小的内存请求使用不同的空闲块管理策略。例如，大块内存分配使用 First Fit，小块内存分配使用 Best Fit。这样可以在不同的场景下发挥各自优势。

3、内存碎片：

长期运行中，可能会出现大量更小的内存碎片，导致大量内存虽然空闲，但无法满足任何实用的内存请求，从而降低内存的整体利用率。

可以考虑使用buddy system






#### 扩展练习Challenge：buddy system（伙伴系统）分配算法（需要编程）

Buddy System算法把系统中的可用存储空间划分为存储块(Block)来进行管理, 每个存储块的大小必须是2的n次幂(Pow(2, n)), 即1, 2, 4, 8, 16, 32, 64, 128...

- 参考[伙伴分配器的一个极简实现](http://coolshell.cn/articles/10427.html)， 在ucore中实现buddy system分配算法，要求有比较充分的测试用例说明实现的正确性，需要有设计文档。

### 算法原理：

Buddy System算法，也称为伙伴系统算法，是一种用于内存分配和管理的算法。它的核心思想是将内存分割成2的幂次大小的块，并通过合并和分割这些块来满足内存分配和释放的需求。

- 内存分割：内存被分割成多个大小为2的幂次方的块，每个大小的块都维护在一个空闲链表中。

- 内存分配：当需要分配内存时，算法会找到能够满足需求的最小空闲块。如果找到的块比需求大，它会将这个块分割成两个大小相等的“伙伴”块，然后继续这个过程直到找到合适大小的块或者到达最小块大小。

- 内存释放：当释放内存时，系统会检查释放的块的伙伴是否也是空闲的，如果是，则将它们合并成更大的块，这个过程会递归进行，直到无法合并为止。

### 初始化

该板块包含的函数负责在系统启动时初始化内存管理器所需的数据结构和内存映射。

#### 初始化Buddy System (buddy_init)

设置空闲内存列表和空闲内存数量，是内存管理系统启动时的第一个调用点：

```c
static void buddy_init(void) {
    list_init(&free_list); // 初始化空闲列表
    nr_free = 0; // 重置可用内存计数
}

```

#### 初始化二叉树节点 (buddy2_new)

根据提供的总内存大小，构建一个二叉树结构，每个节点代表一个可能的内存块。节点的大小是2的幂次方：

```c
void buddy2_new(int size) {
    unsigned node_size;
    int i;
    nr_block = 0; // 重置已分配块数
    if (size < 1 || !IS_POWER_OF_2(size)) // 检查规格是否正确
        return;

    root[0].size = size; // 设置根节点大小
    node_size = size * 2; // 计算节点的大小

    for (i = 0; i < 2 * size - 1; ++i) 
    {
        if (IS_POWER_OF_2(i + 1))
            node_size /= 2; // 更新节点大小
        root[i].len = node_size; // 设置当前节点长度
    }
}

```

#### 初始化内存映射 (buddy_init_memmap)

将物理内存页映射到操作系统的内存管理结构中，初始化每个页面的状态，并将其加入到空闲列表中。

```c
static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0); // 确保n大于0
    n = UINT32_ROUND_DOWN(n); // 向下取整为2的幂
    // 检查页面的使用情况
    struct Page *p = page_base = base;
    for (; p != base + n; p++) 
    {
        assert(PageReserved(p)); // 确保页面被保留
        p->flags = p->property = 0; // 清空标志和属性
        set_page_ref(p, 0); // 设定页面引用为0
    }
    base->property = n; // 设置基页的属性
    SetPageProperty(base); // 设定基页的属性状态
    nr_free += n; // 更新空闲页面计数

    list_add(&free_list, &(base->page_link)); // 将基页添加到空闲列表
    buddy2_new(n); // 初始化新的伙伴系统
}

```

### 内存分配

这个板块包含的函数负责处理内存分配请求，处理来自操作系统或其他模块的内存分配请求。

#### 内存分配 (buddy2_alloc)

遍历二叉树，寻找足够大的空闲内存块来满足请求。如果需要，会分裂较大的内存块以匹配请求的大小，并更新树结构。

```c
void buddy2_alloc(struct buddy2* root, int size, int* page_num, int* parent_page_num) {
    unsigned index = 0; // 节点的标号
    unsigned node_size;

    if (root == NULL) // 无法进行分配
        return;

    if (size <= 0) // 分配请求不合理
        return;
    if (!IS_POWER_OF_2(size)) // 如果不是2的幂，取比size更大的2的n次幂
        size = fixsize(size);

    if (root[index].len < size) // 可分配内存不足
        return;

    // 逐步寻找合适的节点进行分配
    for (node_size = root->size; node_size != size; node_size /= 2) 
    {
        int page_num = (index + 1) * node_size - root->size; // 计算页偏移
        struct Page *left_page = page_base + page_num; // 左子页
        struct Page *right_page = left_page + node_size / 2; // 右子页
        // 分裂节点
        if (left_page->property == node_size && PageProperty(left_page)) // 只有当整块大页都是空闲页时，才进行分裂
        {
            left_page->property /= 2; // 更新左子页的属性
            right_page->property = left_page->property; // 更新右子页的属性
            SetPageProperty(right_page); // 设置右子页的属性
        }

        // 选择下一个子节点
        if (root[LEFT_LEAF(index)].len >= size && root[RIGHT_LEAF(index)].len >= size)
        {
            index = root[LEFT_LEAF(index)].len <= root[RIGHT_LEAF(index)].len ? LEFT_LEAF(index) : RIGHT_LEAF(index); // 选择更小子节点
        }
        else
        {
            index = root[LEFT_LEAF(index)].len < root[RIGHT_LEAF(index)].len ? RIGHT_LEAF(index) : LEFT_LEAF(index); // 选择更小子节点
        }
    }

    root[index].len = 0; // 标记节点为已使用
    // 页上的偏移，表示第几个页
    *page_num = (index + 1) * node_size - root->size; // 当前页的编号
    *parent_page_num = (PARENT(index) + 1) * node_size * 2 - root->size; // 父页的编号

    // 向上刷新，修改先祖节点的值
    while (index) 
    {
        index = PARENT(index); // 逐级向上遍历
        root[index].len = MAX(root[LEFT_LEAF(index)].len, root[RIGHT_LEAF(index)].len); // 更新节点长度
    }
}
```

#### 分配页面 (buddy_alloc_pages)

处理具体的内存分配请求，调用buddy2_alloc来找到和分配内存。

同时，。更新操作系统的内存管理数据结构，如空闲列表和空闲内存计数器，确保内存状态的准确性
```c
static struct Page *
buddy_alloc_pages(size_t n) 
{
    // 确保n大于0
    assert(n > 0);
    // 确保n小于剩余可用块数
    if (n > nr_free)
        return NULL;
    // 确保n是2的整数次幂
    if (!IS_POWER_OF_2(n))
        n = fixsize(n);
    
    // 要分配的页面以及父页面
    struct Page *page, *parent_page;
    // 页的序号
    int page_num, parent_page_num;

    // 分配页面
    buddy2_alloc(root, n, &page_num, &parent_page_num);

    // 从计算的页开始分配
    page = page_base + page_num;
    parent_page = page_base + parent_page_num;

    // 记录已分配块数
    nr_block++;

    // 检查是否还有剩余
    if (page->property != n) // 还有剩余
    {
        if (page == parent_page) // 若page是parent_page的左孩子
        {
            // 将右节点连入链表
            struct Page *right_page = page + n; // 右子页面
            right_page->property = n; // 设置右子页面属性
            list_entry_t *prev = list_prev(&(page->page_link)); // 获取前一个节点
            list_del(&page->page_link); // 从链表中删除当前页面
            list_add(prev, &(right_page->page_link)); // 将右子页面加入链表
            SetPageProperty(right_page); // 设置右子页面属性
        }
        else // 若page是parent_page的右孩子
        {
            // 更改左节点的property
            parent_page->property /= 2; // 将父页面属性减半
        }
    }
    else // 表示已全部分配
    {
        list_del(&page->page_link); // 从链表中删除已分配页面
    }

    ClearPageProperty(page); // 清除页面属性

    nr_free -= n; // 减去已分配的页数
    return page; // 返回分配的页面
}
```

### 内存释放

处理内存释放请求，将内存返回给系统，并尝试优化内存布局。

#### 释放页面 (buddy_free_pages)

将不再需要的内存块返回给系统，并通过伙伴系统树与相邻的空闲内存块合并，以减少内存碎片。
```c
// 释放base开始的n个页面
static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0); // 确保n大于0
    if (!IS_POWER_OF_2(n)) { // 确保n是2的幂
        n = fixsize(n);
    }
    
    assert(base >= page_base && base < page_base + root->size); // 确保base在有效范围内

    // 计算分段数量以及页号
    int div_seg_num = root->size / n; // 被分割的段数量
    int page_num = base - page_base; // 当前页的页号
    assert(page_num % n == 0); // 确保在分位点上

    // 检查页面的状态并重置
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p)); // 检查页面是否被保留且未占用
        p->flags = 0; // 清空页面标志
        set_page_ref(p, 0); // 设置页面引用为0
    }
    base->property = n; // 设置基页面的属性
    SetPageProperty(base); // 设置基页面属性
    nr_free += n; // 增加空闲页面计数

    // 如果空闲列表为空，直接加入
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link)); // 将基页面加入空闲列表
    } else {
        // 计算对应的树节点索引
        int buddy_index = root->size - 1 + page_num; // 该页所对应的树节点
        int node_size = n; // 当前节点大小
        root[buddy_index].len = n;
        // 向上合并
        while (buddy_index) {
            buddy_index = PARENT(buddy_index); // 向上获取父节点
            node_size *= 2; // 更新节点大小
            int left_longest = root[LEFT_LEAF(buddy_index)].len; // 左子节点的长度
            int right_longest = root[RIGHT_LEAF(buddy_index)].len; // 右子节点的长度

            // 如果左右节点可以合并
            if (left_longest + right_longest == node_size) { // 进行合并
                root[buddy_index].len = node_size; // 更新父节点长度
                int left_page_num = (LEFT_LEAF(buddy_index) + 1) * node_size / 2 - root->size; // 左边的页号
                int right_page_num = (RIGHT_LEAF(buddy_index) + 1) * node_size / 2 - root->size; // 右边的页号
                struct Page *left_page = page_base + left_page_num; // 左子页面
                struct Page *right_page = page_base + right_page_num; // 右子页面

                // 检查左子页面是否在空闲列表中
                if (!in_freelist(left_page)) {
                    list_add_before(&(right_page->page_link), &(left_page->page_link)); // 将右子页面插入左子页面之前
                }
                // 检查右子页面是否在空闲列表中
                if (!in_freelist(right_page)) {
                    list_add(&(left_page->page_link), &(right_page->page_link)); // 将左子页面添加到空闲列表
                }
                // 合并两个节点
                left_page->property += right_page->property; // 更新左子页面属性
                right_page->property = 0; // 清空右子页面属性
                list_del(&right_page->page_link); // 从链表中删除右子页面
                ClearPageProperty(right_page); // 清空右子页面属性
            } else { // 如果没有合并，更新节点长度
                root[buddy_index].len = MAX(left_longest, right_longest);
            }
        }
    }
    cprintf("内存释放完成\n");
}

```

### 辅助函数

#### 检查页面是否在空闲列表中 (in_freelist)

```c
int in_freelist(struct Page* p)
{
    if (list_prev(&p->page_link) == NULL) // 不在空闲列表中
        return 0;

    return list_next(list_prev(&p->page_link)) == &p->page_link; // 检查是否为第一个节点
}
```

#### 获取当前空闲页面的数量 (buddy_nr_free_pages)

```c
static size_t
buddy_nr_free_pages(void) {
    return nr_free;// 返回剩余空闲页面数
}
```

### 验证输出

#### 测试样例

```c
static void buddy_check(void)
{
    cprintf("测试多次小规模分配与释放\n");
    struct Page *p0, *p1, *p2, *p3;
    p0 = p1 = p2 = p3 = NULL;

    assert((p0 = alloc_pages(3)) != NULL);
    assert((p1 = alloc_pages(2)) != NULL);
    assert((p2 = alloc_pages(1)) != NULL);
    assert((p3 = alloc_pages(5)) != NULL);

    free_pages(p0, 3);
    free_pages(p1, 2);
    free_pages(p2, 1);
    free_pages(p3, 5);

    assert((p0 = alloc_pages(3)) != NULL);
    assert((p1 = alloc_pages(2)) != NULL);
    assert((p2 = alloc_pages(1)) != NULL);
    assert((p3 = alloc_pages(5)) != NULL);

    free_pages(p0, 3);
    free_pages(p1, 2);
    free_pages(p2, 1);
    free_pages(p3, 5);
    cprintf("成功完成\n");
    
    cprintf("测试大规模块分配与释放\n");
    
    assert((p0 = alloc_pages(128)) != NULL);
    assert((p1 = alloc_pages(64)) != NULL);
    assert((p2 = alloc_pages(256)) != NULL);

    free_pages(p0, 128);
    free_pages(p1, 64);
    free_pages(p2, 256);

    assert((p0 = alloc_pages(128)) != NULL);
    assert((p1 = alloc_pages(64)) != NULL);
    assert((p2 = alloc_pages(256)) != NULL);

    free_pages(p0, 128);
    free_pages(p1, 64);
    free_pages(p2, 256);
    cprintf("成功完成\n");
    
    cprintf("测试不同大小块的交替分配与释放\n");
  
    assert((p0 = alloc_pages(10)) != NULL);
    assert((p1 = alloc_pages(20)) != NULL);
    assert((p2 = alloc_pages(5)) != NULL);
    assert((p3 = alloc_pages(8)) != NULL);

    free_pages(p1, 20);
    free_pages(p3, 8);

    assert((p1 = alloc_pages(15)) != NULL);
    free_pages(p0, 10);
    free_pages(p1, 15);
    free_pages(p2, 5);
    cprintf("成功完成\n");
}
```

#### 测试结果

将 pmm_manager 改为 buddy_pmm_manager，并 make grade 进行测试，试验验证成功。

输出如下：

```c
ys@ys-virtual-machine:~/Desktop/riscv64-ucore-labcodes/lab2$ make grade
>>>>>>>>>> here_make>>>>>>>>>>>
gmake[1]: Entering directory '/home/ys/Desktop/riscv64-ucore-labcodes/lab2' + cc kern/init/entry.S + cc kern/init/init.c + cc kern/libs/stdio.c + cc kern/debug/kdebug.c + cc kern/debug/kmonitor.c + cc kern/debug/panic.c + cc kern/driver/clock.c + cc kern/driver/console.c + cc kern/driver/intr.c + cc kern/trap/trap.c + cc kern/trap/trapentry.S + cc kern/mm/best_fit_pmm.c + cc kern/mm/buddy_pmm.c + cc kern/mm/default_pmm.c + cc kern/mm/pmm.c + cc libs/printfmt.c + cc libs/readline.c + cc libs/sbi.c + cc libs/string.c + ld bin/kernel riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img gmake[1]: Leaving directory '/home/ys/Desktop/riscv64-ucore-labcodes/lab2'
>>>>>>>>>> here_make>>>>>>>>>>>
<<<<<<<<<<<<<<< here_run_qemu <<<<<<<<<<<<<<<<<<
try to run qemu
qemu pid=56922
<<<<<<<<<<<<<<< here_run_check <<<<<<<<<<<<<<<<<<
  -check physical_memory_map_information:    OK
  -check_best_fit:                           OK
  -check ticks:                              OK
Total Score: 30/30
```

make qemu输出如下：

```c
make qemu
+ cc kern/mm/buddy_pmm.c
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img

OpenSBI v0.4 (Jul  2 2019 11:53:53)
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |_
  \____/| .__/ \___|_| |_|_____/|____/_____|
        | |
        |_|

Platform Name          : QEMU Virt Machine
Platform HART Features : RV64ACDFIMSU
Platform Max HARTs     : 8
Current Hart           : 0
Firmware Base          : 0x80000000
Firmware Size          : 112 KB
Runtime SBI Version    : 0.1

PMP0: 0x0000000080000000-0x000000008001ffff (A)
PMP1: 0x0000000000000000-0xffffffffffffffff (A,R,W,X)
(THU.CST) os is loading ...
Special kernel symbols:
  entry  0xffffffffc0200032 (virtual)
  etext  0xffffffffc0201a34 (virtual)
  edata  0xffffffffc0206010 (virtual)
  end    0xffffffffc0254680 (virtual)
Kernel executable memory footprint: 338KB
memory management: buddy_pmm_manager
physcial memory map:
  memory: 0x0000000007e00000, [0x0000000080200000, 0x0000000087ffffff].
测试多次小规模分配与释放
成功完成
测试大规模块分配与释放
成功完成
测试不同大小块的交替分配与释放
成功完成
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0205000
satp physical address: 0x0000000080205000
++ setup timer interrupts
100 ticks
100 ticks
...
```

#### 扩展练习Challenge：任意大小的内存单元slub分配算法（需要编程）

slub算法，实现两层架构的高效内存单元分配，第一层是基于页大小的内存分配，第二层是在第一层基础上实现基于任意大小的内存分配。可简化实现，能够体现其主体思想即可。

- 参考[linux的slub分配算法/](http://www.ibm.com/developerworks/cn/linux/l-cn-slub/)，在ucore中实现slub分配算法。要求有比较充分的测试用例说明实现的正确性，需要有设计文档。
- &emsp;当请求内存分配时，对于较大的块直接按照页的大小分配，可以直接使用原本的页面分配算法。但大多数情况下，程序需要的并不是一整页，而是几个、几十个字节的小内存。于是需要另外一套系统来完成对小内存的管理，这就是slub系统。
###### 在linux中的slub算法思想
&emsp;linux中的slub算法小内存的管理思想是把内存分组管理，每个组分别包含2^3、2^4、...2^11个字节。会有一个`kmalloc_caches[12]`数组，该数组的定义如下：
```
struct kmem_cache kmalloc_caches[PAGE_SHIFT] __cacheline_aligned;
```

&emsp;每个数组元素对应一种大小的内存，对于每一个元素，其工作原理如下：
*   在slub分配器中，首先从 uCore 的页分配器获取内存块（以页为单位）。我们的slub分配器将这些页视为管理小块内存的`slab`。每次请求的小内存块我们将其成为`object`，相同大小的`object`会放到同一个`slab`中。
*   每个`kmem_cache`有两个“部门”。一个是“仓库”：`kmem_cache_node`，一个“营业厅”：`kmem_cache_cpu`。“营业厅”里只保留一个`slab`，只有在营业厅`kmem_cache_cpu`中没有空闲内存的情况下才会从仓库中换出其他的`slab`。
*   `kmem_cache_cpu`的`freelist`变量中保存着下一个空闲`object的`地址。
*   若`kmem_cache_cpu`中已经没有空闲的`object`了，会把当前的这个`slab`放到`kmem_cache_node`中。`kmem_cache_node`中有两个双链表，`partial`和`full`，分别盛放不满的slab(slab中有空闲的object)和全满的slab(slab中没有空闲的object)。此时分配小内存会从`kmem_cache_node`的`partial`中挑出一个不满的`slab`放到`kmem_cache_cpu`中并把一个空闲的`object`返回给用户。
*   若`kmem_cache_cpu`中已经没有空闲的`object`且`kmem_cache_node`没有空闲的`slab`，就需要借助页分配算法申请页`slab`，并把`slab`初始化，放到`kmem_cache_cpu`中，返回第一个空闲的`object`。
*   再回收内存的时候同理，要注意`kmem_cache_node`中两个链表之间`slab`的转换，以及`slab`全部`object`空闲时的释放。
###### 设计slub算法
&emsp;基于以上linux中的slub的思想，我设计出一个一下的ucore中的slub算法：
对于大于等于PAGESIZE的内存块我们沿用分页式的内存管理，对于小于PAGESIZE的内存块，我们利用slub的算法思想实现。
###### 关键代码解释：
*   2个结构体：
```
struct slab_t {
    list_entry_t link;         // 每个slab中object构成的链表
   void *free_list;           // 指向该slab当前即将被分配的空闲object
    uint16_t free_count;       // 当前 slab 中空闲object数
    uint16_t total_count;      // slab 中object总数
};

// 定义 slab cache 结构
struct my_kmem_cache_t {
    list_entry_t slabs_full;       // 全满 slab 链表
    list_entry_t slabs_partial;    // 部分空闲 slab 链表
    list_entry_t slabs_free;       // 全空闲 slab 链表
    uint16_t objsize;              // 单个object的大小
    uint16_t num_objs;             // 每个 slab 能容纳的对象数量      
};
```
我们定义11种object大小，集中放到一个cache数组中：
```
static struct my_kmem_cache_t cache_pool[MAX_CACHES]; 
```
*   定义2个简单的函数，le2slab是根据link推测出slab的地址。page2kva是根据物理页号算出虚拟地址进行内存管理。
```#define le2slab(le, member) \
    ((struct slab_t *)((char *)(le) - offsetof(struct slab_t, member)))
static inline void *page2kva(struct Page *page) {
    // 使用物理页面号计算对应的虚拟地址
    return (void *)((uintptr_t)page << PGSHIFT) + KERNBASE;
}
```
###### 分配内存(my_kmalloc)
*   根据请求的内存大小决定是直接引用分页管理的alloc_pages分配还是利用slub，详细解释在注释中：
```
void *my_kmalloc(int size) {
    if (size >= PAGE_SIZE) {
    // 计算需要的页框数
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // 直接请求页框
    struct Page *page = alloc_pages(num_pages);
    if (!page) {
        return NULL; // 分配失败，返回 NULL
    }

    // 返回虚拟地址
    return page2kva(page);
}
    // 调整大小对齐
    size_t aligned_size = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
   
    // 查找合适的cache
    for (int i = 0; i < MAX_CACHES; i++) {
        if (cache_pool[i].objsize >= aligned_size) {

            // 在对应的cache种分配object
            return my_kmem_cache_alloc(&cache_pool[i]);
        }
    }

    // 如果没有合适的缓存，返回 NULL
    return NULL;
}
```
分配内存时一些被调用的函数如下：
`my_kmem_cache_create`：
*   这个函数是对cache进行初始化，我们定义了11中cache，只有在初始化之后才可以请求分配内存。初始化主要是对cache的各个属性赋值，如objsize，num_objs,以及对3个链表进行初始化。
```struct my_kmem_cache_t *my_kmem_cache_create(size_t size) {
    struct my_kmem_cache_t *cachep = NULL;  // 在循环外部声明并初始化

    for(int i = 0; i < MAX_CACHES; i++) {
        if(cache_pool[i].objsize >= size) {
            cachep = &cache_pool[i]; // 使用静态数组
            break; // 找到合适的缓存，跳出循环
        }
    }

    // 检查是否找到了合适的缓存
    if (cachep == NULL) {
        return NULL; // 或者根据需要返回错误代码
    }

    // 初始化链表
    list_init(&(cachep->slabs_full));
    list_init(&(cachep->slabs_partial));
    list_init(&(cachep->slabs_free));
    
    // 计算每个 slab 能容纳的对象数量
    cachep->num_objs = (PAGE_SIZE - sizeof(struct slab_t)) / size;

    return cachep; // 返回找到的缓存指针
}
```
`my_kmem_cache_alloc`：
*   这个代码就是从特定的cache中分配内存。由于my_kmem_cache_t中存放了3个链表（slabs_partial，slabs_free，slabs_full）的头节点，再分配内存的时候slabs的状态会一直变化，所以分配内存时也需要一直维护这3个链表，及时将无空闲object的slab放入slabs_full链表中，将有空闲object的slab放入slabs_partial链表中，将全都是空闲object的slab放入slabs_free链表中。
```
void *my_kmem_cache_alloc(struct my_kmem_cache_t *cachep) {
    void *objp = NULL;
    if (!list_empty(&(cachep->slabs_partial))) {
        list_entry_t *le = list_next(&(cachep->slabs_partial));
        struct slab_t *slab = le2slab(le, link);
        objp = slab_alloc(slab, cachep);
    } else if (!list_empty(&(cachep->slabs_free))) {
        list_entry_t *le = list_next(&(cachep->slabs_free));
        struct slab_t *slab = le2slab(le, link);
        objp = slab_alloc(slab, cachep);
    } else {
        struct slab_t *new_slab = slab_create(cachep);
        //cprintf("在这里在这里\n"); 
        if (!new_slab) {
            return NULL;
        }
        
        objp = slab_alloc(new_slab, cachep);
    }

    return objp;
}
```
`slab_alloc`:
*   从 slab 中分配空闲的object，slab结构体中free_count存放空闲object的个数，这也是其状态的标志。free_list是一个指针，指向下一个将被分配的空闲的object。
```static void *slab_alloc(struct slab_t *slab, struct my_kmem_cache_t *cachep) {
    if (slab->free_count == 0) {
        return NULL;
    }
    void *objp = slab->free_list; // 取出空闲object
    slab->free_list = *(void **)slab->free_list; // 更新空闲列表
    slab->free_count--;

    if (slab->free_count == 0) {
        list_del(&(slab->link));  //若free_count变为0，则将这个slab一道链表slabs_full中。
        list_add(cachep->slabs_full.prev, &(slab->link)); // 移动到 slabs_full
    }

    return objp; // 返回请求到的object指针
}
```
`slab_create`:
*   创建并初始化一个新的 slab_t 结构体，这个主要是当一个cache中没有partial也没有free的slab了，就重新请求一个页作为新的slab，对这个slab初始化，包括将内部的object链表建立起来。
```
static struct slab_t *slab_create(struct my_kmem_cache_t *cachep) {
     
     struct slab_t *slab = (struct slab_t *)alloc_page();  
     
    if (!slab) {
        return NULL;  // 分配失败
    }
  
    slab->free_list = (void *)((char *)slab + sizeof(struct slab_t));  // 空闲对象起始位置
    slab->free_count = cachep->num_objs; // 初始化空闲对象数量
    slab->total_count = cachep->num_objs; // 初始化总对象数量

    // 初始化 slab 中的对象链表
    void *first = slab->free_list;
    for (int i = 0; i < cachep->num_objs - 1; i++) {
        void *next = (char *)first + cachep->objsize;
        *(void **)first = next;
        first = next;
    }
    *(void **)first = NULL; // 最后一个空闲对象的下一项为空

    list_add(cachep->slabs_partial.prev, &(slab->link)); // 新 slab 加入 slabs_partial
    return slab;
}
```
###### 释放内存

  
`my_kfree`:
*   通过调用这个函数进行释放内存，传入内存虚拟地址以及所在的cache，核心在`my_kmem_cache_free`函数中体现。
```
void my_kfree(void *objp,struct my_kmem_cache_t* p) {
    if (objp == NULL) {
        return; // 如果传入的指针是 NULL，直接返回
    }
    // 将对象返回给相应的内存缓存
    my_kmem_cache_free(p, objp);
}
```
`my_kmem_cache_free`：
*   释放内存主要是注意每个slab释放object后3个状态链表的改变。
```
void my_kmem_cache_free(struct my_kmem_cache_t *cachep, void *objp) {
    if (objp == NULL) {
        return; // 如果传入的指针是 NULL，直接返回
    }
    struct slab_t *slab = le2slab(cachep->slabs_partial.next,link);
    if(slab==NULL)
    slab = le2slab(cachep->slabs_full.next,link);
    //struct slab_t *slab = (struct slab_t *)((char *)objp - ((uintptr_t)objp % PAGE_SIZE));
    *(void **)objp = slab->free_list;
    slab->free_list = objp; // 前插法
    slab->free_count++;

    if (slab->free_count == slab->total_count) {
        list_del(&(slab->link));
        list_add(cachep->slabs_free.prev, &(slab->link));
    } else {
        list_del(&(slab->link));
        list_add(cachep->slabs_partial.prev, &(slab->link));
    }
}
```
###### 测试文本
在kern_init中我们加入一个slub_init函数对我定义的11个cache先初始化一下，只是确定其object的大小：
```
void slub_init()
{
    size_t sizes[MAX_CACHES] = {8, 16, 32, 64, 96, 128, 192, 256, 512, 1024, 2048};
    
    for (size_t i = 0; i < MAX_CACHES; i++) {
        cache_pool[i].objsize = sizes[i];
	}
    check_slub();
}
```
`check_slub`:测试样本
```
static void check_slub()
{  
  for (int i = 0; i < MAX_CACHES; i++) {
        cprintf("cache_array[%d] = %d\n", i, cache_pool[i].objsize);  // 输出数组元素
    }
   struct my_kmem_cache_t * cp1=my_kmem_cache_create(1024);
   assert(cp1 != NULL); 
   cprintf("cp1->objsize=%d\n",cp1->objsize); 
   assert(cp1->objsize==1024);
    cprintf("cp1->num_objs=%d\n",cp1->num_objs); 
   assert(cp1->num_objs==3);
   void * obj1=my_kmalloc(1024);
   assert(obj1 != NULL); 
   if (obj1 == NULL) {
    cprintf("Memory allocation failed\n");
    return; // 或其他适当的错误处理
}
   struct slab_t* partialslab1=le2slab(cp1->slabs_partial.next,link);
   cprintf("partialslab1->free_count = %d\n", partialslab1->free_count);
   cprintf("partialslab1->total_count = %d\n", partialslab1->total_count);
   struct my_kmem_cache_t * cp2=my_kmem_cache_create(8);
   struct my_kmem_cache_t * cp3=my_kmem_cache_create(16);
   struct my_kmem_cache_t * cp4=my_kmem_cache_create(32);
   struct my_kmem_cache_t * cp5=my_kmem_cache_create(64);
   struct my_kmem_cache_t * cp6=my_kmem_cache_create(128);
   struct my_kmem_cache_t * cp7=my_kmem_cache_create(256);
   struct my_kmem_cache_t * cp8=my_kmem_cache_create(512);//把这些的cache都激活,全部激活了才能有后续的分配操作
     assert(cp2 != NULL); 
     assert(cp3 != NULL); 
     assert(cp4 != NULL); 
     assert(cp5 != NULL); 
     assert(cp6 != NULL); 
     assert(cp7 != NULL); 
     assert(cp8 != NULL); 
     
     void *obj7,*obj8,*obj9,*obj10;
     obj7=my_kmalloc(1024);
      obj8=my_kmalloc(1023);
      obj9=my_kmalloc(1011);
      cprintf("len1024full=%d\n", length(&(cp1->slabs_full)));  
      cprintf("len1024part=%d\n", length(&(cp1->slabs_partial)));  
      cprintf("len1024free=%d\n", length(&(cp1->slabs_free))); 
  
      obj10=my_kmalloc(5000);
      assert(obj10!=NULL);
      
  cprintf("len3full=%d\n", length(&(cp3->slabs_full)));  
  cprintf("len3part=%d\n", length(&(cp3->slabs_partial)));  
  cprintf("len3free=%d\n", length(&(cp3->slabs_free)));  
  
   cprintf("\n\n");
   void *obj2,*obj3,*obj4,*obj5,*obj6;
     obj2=my_kmalloc(8);
     obj3=my_kmalloc(16);
     cprintf("len3full=%d\n", length(&(cp3->slabs_full)));  
     cprintf("len3part=%d\n", length(&(cp3->slabs_partial)));  
     cprintf("len3free=%d\n", length(&(cp3->slabs_free)));  
     cprintf("\n\n");
     
     struct slab_t* slab2=le2slab(cp2->slabs_partial.next,link);     
     cprintf("cp2->objsize=%d\n",cp2->objsize); 
     cprintf("cp2>num_objs=%d\n",cp2->num_objs); 
     cprintf("slab2->free_count = %d\n",slab2->free_count);
     cprintf("slab2->total_count = %d\n", slab2->total_count);
     cprintf("\n\n");
     
     struct slab_t* slab3=le2slab(cp3->slabs_partial.next,link);
     cprintf("slab3->free_count = %d\n",slab3->free_count);
     cprintf("slab3->total_count = %d\n", slab3->total_count);
     obj4=my_kmalloc(16);
     obj5=my_kmalloc(16);
     obj6=my_kmalloc(16);
     cprintf("slab3->free_count = %d\n",slab3->free_count);
     cprintf("slab3->total_count = %d\n", slab3->total_count);
     cprintf("\n\n");
     
     my_kfree(obj3,cp3);
     cprintf("slab3->free_count = %d\n",slab3->free_count);
     cprintf("slab3->total_count = %d\n", slab3->total_count);
     
      cprintf("\n\n");
     /*my_kfree(obj8,cp1);
     cprintf("len1024full=%d\n", length(&(cp1->slabs_full)));  
     cprintf("len1024part=%d\n", length(&(cp1->slabs_partial)));  
     cprintf("len1024free=%d\n", length(&(cp1->slabs_free))); */
     
}
```
*   输出的测试结果、
  
```
cache_array[0] = 8
cache_array[1] = 16
cache_array[2] = 32
cache_array[3] = 64
cache_array[4] = 96
cache_array[5] = 128
cache_array[6] = 192
cache_array[7] = 256
cache_array[8] = 512
cache_array[9] = 1024
cache_array[10] = 2048  //我们create 几个cache进行初始化，并输出他们的objsize
cp1->objsize=1024
cp1->num_objs=3
partialslab1->free_count = 2
partialslab1->total_count = 3    //对于cp1（object大小为1024，在一个4096大小的页中只能分3个object，因为每个页最面要放32Byte的slab_t的结构体。
len1024full=1
len1024part=1
len1024free=0           //这里是已经申请了4次1024大小的object，所以其对应的cache中full链表中已经有一个slab，同时又重新creat了一个slab为第4次申请分配内存。
len3full=0
len3part=0
len3free=0             //刚刚初始好大小为16的cache，所以各个链表大小均为0


len3full=0            //申请了一次16的object后cache16的变化
len3part=1
len3free=0


cp2->objsize=8
cp2>num_objs=508
slab2->free_count = 507
slab2->total_count = 508


slab3->free_count = 253
slab3->total_count = 254
slab3->free_count = 250
slab3->total_count = 254     //这一段slab3的变化可以看到分配object后其free_count的减少，当释放时free_count增大。
slab3->free_count = 251
slab3->total_count = 254


++ setup timer interrupts
100 ticks
100 ticks
......
//同时在样本中我们还请求了大于PAGESIZE的内存，设置断言，发现成功分配到。
```

#### 扩展练习Challenge：硬件的可用物理内存范围的获取方法（思考题）

- 如果 OS 无法提前知道当前硬件的可用物理内存范围，请问你有何办法让 OS 获取可用物理内存范围？

> Challenges是选做，完成Challenge的同学可单独提交Challenge。完成得好的同学可获得最终考试成绩的加分。

我们查阅资料，得到以下方法：

**1、发出中断调用** ：通过  **BIOS 中断 0x15, 功能号 E820h** ，并在寄存器中设置适当的参数，BIOS 返回一个包含系统中各内存段的类型及大小的内存映射表。

**2、****解析内存映射表** ：BIOS 返回的内存映射表列出了系统中所有的物理内存区域，包括可用的 RAM、保留的硬件设备地址空间等。操作系统根据这些信息划分出可以使用的物理内存。

**3、** **初始化物理内存管理器** ：ucore 使用这些信息初始化物理内存管理器，并将可用的物理内存区域加入内存分配器中，以便后续的页表管理和虚拟内存管理。

一个可能的代码：

```
struct e820map {
    uint32_t nr_map;  // 可用区域数量
    struct {
        uint64_t addr;  // 起始地址
        uint64_t size;  // 区域大小
        uint32_t type;  // 区域类型（1 表示可用内存）
    } map[E820MAX];
};

void get_memory_map() {
    struct e820map *memmap = (struct e820map*)0x8000;  // 内存映射表存放地址
    int success = 0;
    asm volatile (
        "int $0x15"  // 调用 BIOS 中断
        : "=a" (success)
        : "a" (0xE820), "b" (0), "c" (24), "d" (0x534D4150), "D" (memmap)
        : "memory"
    );
    if (success == 1) {
        // 成功获取到内存映射表，接下来可以使用 memmap 进行物理内存管理初始化
    }
}

```
