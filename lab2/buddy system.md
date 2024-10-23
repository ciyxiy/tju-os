## 扩展练习Challenge：buddy system（伙伴系统）分配算法（需要编程）
Buddy System算法把系统中的可用存储空间划分为存储块(Block)来进行管理, 每个存储块的大小必须是2的n次幂(Pow(2, n)), 即1, 2, 4, 8, 16, 32, 64, 128...

参考伙伴分配器的一个极简实现， 在ucore中实现buddy system分配算法，要求有比较充分的测试用例说明实现的正确性，需要有设计文档。

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
            if (right_page - page_base == 1)
                cprintf("在分配时:--------------------设置--------------------\n");
            SetPageProperty(right_page); // 设置右子页的属性
            // cprintf("在分配时: node_size:%d, page_num:%d, left_page->property:%d, right_page->property:%d\n",node_size, page_num, left_page->property, right_page->property);
            list_add(&(left_page->page_link), &(right_page->page_link)); // 将子页加入到空闲列表
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
    cprintf("在分配页面: page_num:%d, parent_page_num:%d\n", page_num, parent_page_num);

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
            if (right_page - page_base == 1)
                cprintf("在分配页面:--------------------设置--------------------\n");
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

    if (page - page_base == 1)
        cprintf("在分配页面:--------------------清除--------------------\n");
    ClearPageProperty(page); // 清除页面属性
    cprintf("在分配: page_num:%d , property:%d \n", page - page_base, PageProperty(page));

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
    cprintf("释放内存开始，释放页数: %zu\n", n);
    if (!IS_POWER_OF_2(n)) { // 确保n是2的幂
        n = fixsize(n);
        cprintf("调整释放页数为2的幂次方: %zu\n", n);
    }
    
    assert(base >= page_base && base < page_base + root->size); // 确保base在有效范围内
    cprintf("开始释放内存块，起始地址: %p\n", base);

    // 计算分段数量以及页号
    int div_seg_num = root->size / n; // 被分割的段数量
    int page_num = base - page_base; // 当前页的页号
    assert(page_num % n == 0); // 确保在分位点上
    cprintf("页号: %d, 段数量: %d\n", page_num, div_seg_num);

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
    cprintf("内存块释放，增加空闲页数: %zu\n", n);

    // 如果空闲列表为空，直接加入
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link)); // 将基页面加入空闲列表
        cprintf("空闲列表为空，直接加入基页面\n");
    } else {
        // 计算对应的树节点索引
        int buddy_index = root->size - 1 + page_num; // 该页所对应的树节点
        int node_size = n; // 当前节点大小
        root[buddy_index].len = n;
        cprintf("开始合并内存块，树节点索引: %d\n", buddy_index);

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
                cprintf("合并内存块成功，左子页号: %d, 右子页号: %d\n", left_page_num, right_page_num);
            } else { // 如果没有合并，更新节点长度
                root[buddy_index].len = MAX(left_longest, right_longest);
                cprintf("无法合并，更新节点长度，节点索引: %d\n", buddy_index);
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
static void
buddy_check(void) {
    cprintf("\n伙伴系统内存管理器测试开始！\n\n");
    
    cprintf("所有可用页面数: %d\n", nr_free);
    
    struct Page *p0, *p1, *p2, *p3, *p4, *p5;
    p0 = p1 = p2 = p3 = p4 = p5 = NULL;

    // 第1步：为p0, p1, p2, 和 p3分配页面
    cprintf("分配页面...\n");
    p0 = alloc_pages(70);
    assert(p0 != NULL);
    cprintf("p0: 分配了 %d 个页面，地址: %p\n", 70, p0);
    
    p1 = alloc_pages(35);
    assert(p1 != NULL);
    cprintf("p1: 分配了 %d 个页面，地址: %p\n", 35, p1);
    
    p2 = alloc_pages(257);
    assert(p2 != NULL);
    cprintf("p2: 分配了 %d 个页面，地址: %p\n", 257, p2);
    
    p3 = alloc_pages(63);
    assert(p3 != NULL);
    cprintf("p3: 分配了 %d 个页面，地址: %p\n", 63, p3);

    // 打印分配后的内存状态
    cprintf("分配后的内存状态:\n");
    for (int i = 0; i < 30; i++) {
        cprintf("页 %d: %s\n", i, (PageProperty(page_base + i) ? "已分配" : "空闲"));
    }
    
    // 第2步：释放p1和p3的页面
    cprintf("释放页面...\n");
    free_pages(p1, 35);
    cprintf("释放了 p1 (%d 个页面)\n", 35);
    
    free_pages(p3, 63);
    cprintf("释放了 p3 (%d 个页面)\n", 63);

    // 打印释放后的内存状态
    cprintf("释放p1和p3后的内存状态:\n");
    for (int i = 0; i < 30; i++) {
        cprintf("页 %d: %s\n", i, (PageProperty(page_base + i) ? "已分配" : "空闲"));
    }

    // 第3步：释放p0的页面
    cprintf("释放了 p0 (%d 个页面)\n", 70);
    free_pages(p0, 70);

    // 打印释放后的内存状态
    cprintf("释放p0后的内存状态:\n");
    for (int i = 0; i < 30; i++) {
        cprintf("页 %d: %s\n", i, (PageProperty(page_base + i) ? "已分配" : "空闲"));
    }

    // 第4步：为p4和p5分配页面
    cprintf("再次分配页面...\n");
    p4 = alloc_pages(255);
    assert(p4 != NULL);
    cprintf("p4: 分配了 %d 个页面，地址: %p\n", 255, p4);
    
    p5 = alloc_pages(255);
    assert(p5 != NULL);
    cprintf("p5: 分配了 %d 个页面，地址: %p\n", 255, p5);

    // 打印分配后的内存状态
    cprintf("分配p4和p5后的内存状态:\n");
    for (int i = 0; i < 30; i++) {
        cprintf("页 %d: %s\n", i, (PageProperty(page_base + i) ? "已分配" : "空闲"));
    }

    // 第5步：释放p2, p4, 和 p5的页面
    cprintf("最终释放页面...\n");
    free_pages(p2, 257);
    cprintf("释放了 p2 (%d 个页面)\n", 257);
    
    free_pages(p4, 255);
    cprintf("释放了 p4 (%d 个页面)\n", 255);
    
    free_pages(p5, 255);
    cprintf("释放了 p5 (%d 个页面)\n", 255);

    // 打印最终释放后的内存状态
    cprintf("最终释放后的内存状态:\n");
    for (int i = 0; i < 30; i++) {
        cprintf("页 %d: %s\n", i, (PageProperty(page_base + i) ? "已分配" : "空闲"));
    }

    cprintf("测试完成！\n");
    cprintf("-----------------------------------------------------\n");
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
伙伴系统内存管理器测试开始！

所有可用页面数: 16384
分配页面...
在分配页面: page_num:0, parent_page_num:0
在分配: page_num:0 , property:0 
p0: 分配了 70 个页面，地址: 0xffffffffc025df48
在分配页面: page_num:128, parent_page_num:128
在分配: page_num:128 , property:0 
p1: 分配了 35 个页面，地址: 0xffffffffc025f348
在分配页面: page_num:512, parent_page_num:0
在分配: page_num:512 , property:0 
p2: 分配了 257 个页面，地址: 0xffffffffc0262f48
在分配页面: page_num:192, parent_page_num:128
在分配: page_num:192 , property:0 
p3: 分配了 63 个页面，地址: 0xffffffffc025fd48
分配后的内存状态:
页 0: 空闲
页 1: 空闲
页 2: 空闲
页 3: 空闲
页 4: 空闲
页 5: 空闲
页 6: 空闲
页 7: 空闲
页 8: 空闲
页 9: 空闲
页 10: 空闲
页 11: 空闲
页 12: 空闲
页 13: 空闲
页 14: 空闲
页 15: 空闲
页 16: 空闲
页 17: 空闲
页 18: 空闲
页 19: 空闲
页 20: 空闲
页 21: 空闲
页 22: 空闲
页 23: 空闲
页 24: 空闲
页 25: 空闲
页 26: 空闲
页 27: 空闲
页 28: 空闲
页 29: 空闲
释放页面...
释放内存开始，释放页数: %zu
调整释放页数为2的幂次方: %zu
开始释放内存块，起始地址: 0xffffffffc025f348
页号: 128, 段数量: 256
内存块释放，增加空闲页数: %zu
开始合并内存块，树节点索引: 16511
无法合并，更新节点长度，节点索引: 8255
无法合并，更新节点长度，节点索引: 4127
无法合并，更新节点长度，节点索引: 2063
无法合并，更新节点长度，节点索引: 1031
无法合并，更新节点长度，节点索引: 515
无法合并，更新节点长度，节点索引: 257
无法合并，更新节点长度，节点索引: 128
无法合并，更新节点长度，节点索引: 63
无法合并，更新节点长度，节点索引: 31
无法合并，更新节点长度，节点索引: 15
无法合并，更新节点长度，节点索引: 7
无法合并，更新节点长度，节点索引: 3
无法合并，更新节点长度，节点索引: 1
无法合并，更新节点长度，节点索引: 0
内存释放完成
释放了 p1 (35 个页面)
释放内存开始，释放页数: %zu
调整释放页数为2的幂次方: %zu
开始释放内存块，起始地址: 0xffffffffc025fd48
页号: 192, 段数量: 256
内存块释放，增加空闲页数: %zu
开始合并内存块，树节点索引: 16575
无法合并，更新节点长度，节点索引: 8287
无法合并，更新节点长度，节点索引: 4143
无法合并，更新节点长度，节点索引: 2071
无法合并，更新节点长度，节点索引: 1035
无法合并，更新节点长度，节点索引: 517
无法合并，更新节点长度，节点索引: 258
无法合并，更新节点长度，节点索引: 128
无法合并，更新节点长度，节点索引: 63
无法合并，更新节点长度，节点索引: 31
无法合并，更新节点长度，节点索引: 15
无法合并，更新节点长度，节点索引: 7
无法合并，更新节点长度，节点索引: 3
无法合并，更新节点长度，节点索引: 1
无法合并，更新节点长度，节点索引: 0
内存释放完成
释放了 p3 (63 个页面)
释放p1和p3后的内存状态:
页 0: 空闲
页 1: 空闲
页 2: 空闲
页 3: 空闲
页 4: 空闲
页 5: 空闲
页 6: 空闲
页 7: 空闲
页 8: 空闲
页 9: 空闲
页 10: 空闲
页 11: 空闲
页 12: 空闲
页 13: 空闲
页 14: 空闲
页 15: 空闲
页 16: 空闲
页 17: 空闲
页 18: 空闲
页 19: 空闲
页 20: 空闲
页 21: 空闲
页 22: 空闲
页 23: 空闲
页 24: 空闲
页 25: 空闲
页 26: 空闲
页 27: 空闲
页 28: 空闲
页 29: 空闲
释放了 p0 (70 个页面)
释放内存开始，释放页数: %zu
调整释放页数为2的幂次方: %zu
开始释放内存块，起始地址: 0xffffffffc025df48
页号: 0, 段数量: 128
内存块释放，增加空闲页数: %zu
开始合并内存块，树节点索引: 16383
无法合并，更新节点长度，节点索引: 8191
无法合并，更新节点长度，节点索引: 4095
无法合并，更新节点长度，节点索引: 2047
无法合并，更新节点长度，节点索引: 1023
无法合并，更新节点长度，节点索引: 511
无法合并，更新节点长度，节点索引: 255
无法合并，更新节点长度，节点索引: 127
无法合并，更新节点长度，节点索引: 63
无法合并，更新节点长度，节点索引: 31
无法合并，更新节点长度，节点索引: 15
无法合并，更新节点长度，节点索引: 7
无法合并，更新节点长度，节点索引: 3
无法合并，更新节点长度，节点索引: 1
无法合并，更新节点长度，节点索引: 0
内存释放完成
释放p0后的内存状态:
页 0: 已分配
页 1: 空闲
页 2: 空闲
页 3: 空闲
页 4: 空闲
页 5: 空闲
页 6: 空闲
页 7: 空闲
页 8: 空闲
页 9: 空闲
页 10: 空闲
页 11: 空闲
页 12: 空闲
页 13: 空闲
页 14: 空闲
页 15: 空闲
页 16: 空闲
页 17: 空闲
页 18: 空闲
页 19: 空闲
页 20: 空闲
页 21: 空闲
页 22: 空闲
页 23: 空闲
页 24: 空闲
页 25: 空闲
页 26: 空闲
页 27: 空闲
页 28: 空闲
页 29: 空闲
再次分配页面...
在分配页面: page_num:256, parent_page_num:0
在分配: page_num:256 , property:0 
p4: 分配了 255 个页面，地址: 0xffffffffc0260748
在分配页面: page_num:1024, parent_page_num:1024
在分配: page_num:1024 , property:0 
p5: 分配了 255 个页面，地址: 0xffffffffc0267f48
分配p4和p5后的内存状态:
页 0: 已分配
页 1: 空闲
页 2: 空闲
页 3: 空闲
页 4: 空闲
页 5: 空闲
页 6: 空闲
页 7: 空闲
页 8: 空闲
页 9: 空闲
页 10: 空闲
页 11: 空闲
页 12: 空闲
页 13: 空闲
页 14: 空闲
页 15: 空闲
页 16: 空闲
页 17: 空闲
页 18: 空闲
页 19: 空闲
页 20: 空闲
页 21: 空闲
页 22: 空闲
页 23: 空闲
页 24: 空闲
页 25: 空闲
页 26: 空闲
页 27: 空闲
页 28: 空闲
页 29: 空闲
最终释放页面...
释放内存开始，释放页数: %zu
调整释放页数为2的幂次方: %zu
开始释放内存块，起始地址: 0xffffffffc0262f48
页号: 512, 段数量: 32
内存块释放，增加空闲页数: %zu
开始合并内存块，树节点索引: 16895
无法合并，更新节点长度，节点索引: 8447
无法合并，更新节点长度，节点索引: 4223
无法合并，更新节点长度，节点索引: 2111
无法合并，更新节点长度，节点索引: 1055
无法合并，更新节点长度，节点索引: 527
无法合并，更新节点长度，节点索引: 263
无法合并，更新节点长度，节点索引: 131
无法合并，更新节点长度，节点索引: 65
无法合并，更新节点长度，节点索引: 32
无法合并，更新节点长度，节点索引: 15
无法合并，更新节点长度，节点索引: 7
无法合并，更新节点长度，节点索引: 3
无法合并，更新节点长度，节点索引: 1
无法合并，更新节点长度，节点索引: 0
内存释放完成
释放了 p2 (257 个页面)
释放内存开始，释放页数: %zu
调整释放页数为2的幂次方: %zu
开始释放内存块，起始地址: 0xffffffffc0260748
页号: 256, 段数量: 64
内存块释放，增加空闲页数: %zu
开始合并内存块，树节点索引: 16639
无法合并，更新节点长度，节点索引: 8319
无法合并，更新节点长度，节点索引: 4159
无法合并，更新节点长度，节点索引: 2079
无法合并，更新节点长度，节点索引: 1039
无法合并，更新节点长度，节点索引: 519
无法合并，更新节点长度，节点索引: 259
无法合并，更新节点长度，节点索引: 129
无法合并，更新节点长度，节点索引: 64
无法合并，更新节点长度，节点索引: 31
无法合并，更新节点长度，节点索引: 15
无法合并，更新节点长度，节点索引: 7
无法合并，更新节点长度，节点索引: 3
无法合并，更新节点长度，节点索引: 1
无法合并，更新节点长度，节点索引: 0
内存释放完成
释放了 p4 (255 个页面)
释放内存开始，释放页数: %zu
调整释放页数为2的幂次方: %zu
开始释放内存块，起始地址: 0xffffffffc0267f48
页号: 1024, 段数量: 64
内存块释放，增加空闲页数: %zu
开始合并内存块，树节点索引: 17407
无法合并，更新节点长度，节点索引: 8703
无法合并，更新节点长度，节点索引: 4351
无法合并，更新节点长度，节点索引: 2175
无法合并，更新节点长度，节点索引: 1087
无法合并，更新节点长度，节点索引: 543
无法合并，更新节点长度，节点索引: 271
无法合并，更新节点长度，节点索引: 135
无法合并，更新节点长度，节点索引: 67
无法合并，更新节点长度，节点索引: 33
无法合并，更新节点长度，节点索引: 16
无法合并，更新节点长度，节点索引: 7
无法合并，更新节点长度，节点索引: 3
无法合并，更新节点长度，节点索引: 1
无法合并，更新节点长度，节点索引: 0
内存释放完成
释放了 p5 (255 个页面)
最终释放后的内存状态:
页 0: 已分配
页 1: 空闲
页 2: 空闲
页 3: 空闲
页 4: 空闲
页 5: 空闲
页 6: 空闲
页 7: 空闲
页 8: 空闲
页 9: 空闲
页 10: 空闲
页 11: 空闲
页 12: 空闲
页 13: 空闲
页 14: 空闲
页 15: 空闲
页 16: 空闲
页 17: 空闲
页 18: 空闲
页 19: 空闲
页 20: 空闲
页 21: 空闲
页 22: 空闲
页 23: 空闲
页 24: 空闲
页 25: 空闲
页 26: 空闲
页 27: 空闲
页 28: 空闲
页 29: 空闲
测试完成！
```