#include <pmm.h>
#include <list.h>
#include <string.h>
#include <stdio.h>
#include <buddy_pmm.h>
#include <sbi.h>

// 计算偏移量公式：offset=(index+1)*node_size – size。
// 其中索引的下标均从0开始，size为内存总大小，node_size为内存块对应大小。
#define LEFT_LEAF(index) ((index) * 2 + 1) // 左子节点索引
#define RIGHT_LEAF(index) ((index) * 2 + 2) // 右子节点索引
#define PARENT(index) ( ((index) + 1) / 2 - 1) // 父节点索引

#define IS_POWER_OF_2(x) (!((x)&((x)-1))) // 判断x是否为2的幂
#define MAX(a, b) ((a) > (b) ? (a) : (b)) // 获取最大值
#define UINT32_SHR_OR(a,n) ((a)|((a)>>(n))) // 右移n位并与原数或运算

#define UINT32_MASK(a) (UINT32_SHR_OR(UINT32_SHR_OR(UINT32_SHR_OR(UINT32_SHR_OR(UINT32_SHR_OR(a,1),2),4),8),16)) 
// 获取大于a的一个最小的2^k
#define UINT32_REMAINDER(a) ((a)&(UINT32_MASK(a)>>1)) // 计算a的余数
#define UINT32_ROUND_DOWN(a) (UINT32_REMAINDER(a)?((a)-UINT32_REMAINDER(a)):(a)) // 小于a的最大2^k

// 修复内存块大小，使其为2的幂
static unsigned fixsize(unsigned size) 
{
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    return size + 1;
}

// 伙伴系统的结构体定义
struct buddy2 {
    unsigned size; // 表明管理的内存大小
    unsigned len; // 当前节点的长度
};
struct buddy2 root[40000]; // 存放二叉树的数组，用于内存分配

int nr_block; // 已分配的块数
free_area_t free_area; // 待分配区域
struct Page* page_base; // 页面基址

#define free_list (free_area.free_list) // 空闲链表
#define nr_free (free_area.nr_free) // 可用内存数量

// 初始化伙伴系统
static void buddy_init(void) {
    list_init(&free_list); // 初始化空闲列表
    nr_free = 0; // 重置可用内存计数
}

// 初始化二叉树上的节点
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

// 初始化内存映射
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

// 内存分配
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


// 分配n个页面
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

// 检查页面p是否在空闲列表中
int in_freelist(struct Page* p)
{
    if (list_prev(&p->page_link) == NULL) // 不在空闲列表中
        return 0;

    return list_next(list_prev(&p->page_link)) == &p->page_link; // 检查是否为第一个节点
}

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

// 获取当前空闲页面的数量
static size_t
buddy_nr_free_pages(void) {
    return nr_free; // 返回剩余空闲页面数
}

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

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
