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
