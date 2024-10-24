#include <pmm.h>
#include <list.h>
#include <string.h>
#include <stdio.h>
#include <buddy_pmm.h>
#include <sbi.h>

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

    for (i = 0; i < 2 * size - 1; ++i){
        if (IS_POWER_OF_2(i + 1))
            node_size /= 2; // 更新节点大小
        root[i].len = node_size; // 设置当前节点长度
    }
}

// 初始化内存映射
static void buddy_init_memmap(struct Page* base, size_t n) {
    assert(n > 0); // 确保n大于0
    n = UINT32_ROUND_DOWN(n); // 向下取整为2的幂
    // 检查页面的使用情况
    struct Page* p = page_base = base;
    for (; p != base + n; p++){
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
    for (node_size = root->size; node_size != size; node_size /= 2){
        int page_num = (index + 1) * node_size - root->size; // 计算页偏移
        struct Page* left_page = page_base + page_num; // 左子页
        struct Page* right_page = left_page + node_size / 2; // 右子页
        // 分裂节点
        if (left_page->property == node_size && PageProperty(left_page)) // 只有当整块大页都是空闲页时，才进行分裂
        {
            left_page->property /= 2; // 更新左子页的属性
            right_page->property = left_page->property; // 更新右子页的属性

            SetPageProperty(right_page); // 设置右子页的属性
            list_add(&(left_page->page_link), &(right_page->page_link)); // 将子页加入到空闲列表
        }

        // 选择下一个子节点
        if (root[LEFT_LEAF(index)].len >= size && root[RIGHT_LEAF(index)].len >= size)
            index = root[LEFT_LEAF(index)].len <= root[RIGHT_LEAF(index)].len ? LEFT_LEAF(index) : RIGHT_LEAF(index); // 选择更小子节点
        
        else
            index = root[LEFT_LEAF(index)].len < root[RIGHT_LEAF(index)].len ? RIGHT_LEAF(index) : LEFT_LEAF(index); // 选择更小子节点
        
    }

    root[index].len = 0; // 标记节点为已使用
    // 页上的偏移，表示第几个页
    *page_num = (index + 1) * node_size - root->size; // 当前页的编号
    *parent_page_num = (PARENT(index) + 1) * node_size * 2 - root->size; // 父页的编号

    // 向上刷新，修改祖先节点的值
    while (index){
        index = PARENT(index); // 逐级向上遍历
        root[index].len = MAX(root[LEFT_LEAF(index)].len, root[RIGHT_LEAF(index)].len); // 更新节点长度
    }
}


// 分配n个页面
static struct Page*
buddy_alloc_pages(size_t n){
    // 确保n大于0
    assert(n > 0);
    // 确保n小于剩余可用块数
    if (n > nr_free)
        return NULL;
    // 确保n是2的整数次幂
    if (!IS_POWER_OF_2(n))
        n = fixsize(n);

    // 要分配的页面以及父页面
    struct Page* page, * parent_page;
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
            struct Page* right_page = page + n; // 右子页面
            right_page->property = n; // 设置右子页面属性
            list_entry_t* prev = list_prev(&(page->page_link)); // 获取前一个节点
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

// 检查页面p是否在空闲列表中
int in_freelist(struct Page* p)
{
    if (list_prev(&p->page_link) == NULL) // 不在空闲列表中
        return 0;

    return list_next(list_prev(&p->page_link)) == &p->page_link; // 检查是否为第一个节点
}

// 释放base开始的n个页面
static void
buddy_free_pages(struct Page* base, size_t n) {
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
    struct Page* p = base;
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
    }
    else {
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
                struct Page* left_page = page_base + left_page_num; // 左子页面
                struct Page* right_page = page_base + right_page_num; // 右子页面

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
            }
            else { // 如果没有合并，更新节点长度
                root[buddy_index].len = MAX(left_longest, right_longest);
            }
        }
    }
   
}

// 获取当前空闲页面的数量
static size_t
buddy_nr_free_pages(void) {
    return nr_free; // 返回剩余空闲页面数
}

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

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
