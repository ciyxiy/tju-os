#include "myslub.h"
#include <pmm.h>
#include <list.h>
#include <string.h>
#include <memlayout.h>
#include <assert.h>
#include <default_pmm.h>
#include <defs.h>
#include<stdio.h>


#define le2slab(le, member) \
    ((struct slab_t *)((char *)(le) - offsetof(struct slab_t, member)))
static inline void *page2kva(struct Page *page) {
    // 使用物理页面号计算对应的虚拟地址
    return (void *)((uintptr_t)page << PGSHIFT) + KERNBASE;
}
// 初始化缓存链表头
inline size_t length(list_entry_t *head) {//size_t 是一个无符号整型
    size_t length = 0;
    list_entry_t *current = head->next; // 从头节点的下一个节点开始

    // 遍历链表，直到到达链表的末尾
    while (current !=head) {
        length++; // 计数
        current = current->next; // 移动到下一个节点
    }

    return length; // 返回链表的长度
}

// 从 slab 中分配空闲对象
static void *slab_alloc(struct slab_t *slab, struct my_kmem_cache_t *cachep) {
    if (slab->free_count == 0) {
        return NULL;
    }
    void *objp = slab->free_list; // 取出空闲对象
    slab->free_list = *(void **)slab->free_list; // 更新空闲列表
    slab->free_count--;

    if (slab->free_count == 0) {
        list_del(&(slab->link));
        list_add(cachep->slabs_full.prev, &(slab->link)); // 移动到 slabs_full
    }

    return objp; // 返回请求到的对象指针
}

// 创建并初始化一个新的 slab_t 结构体
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

struct my_kmem_cache_t *my_kmem_cache_create(size_t size) {
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

void my_kmem_cache_destroy(struct my_kmem_cache_t *cachep) {
    // 释放掉全空闲的 slab
    list_entry_t *le = list_next(&(cachep->slabs_free));
    while (le != &(cachep->slabs_free)) {
        struct slab_t *slab = le2slab(le, link);
        list_entry_t *next = list_next(le);
        int size=cachep->objsize;
        my_kfree(slab,cachep); // 释放 slab
        le = next;
    }
}

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

void *my_kmalloc(int size) {
    if (size >= PAGE_SIZE) {
    // 计算需要的页框数，避免溢出
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // 直接请求页框
    struct Page *page = alloc_pages(num_pages);
    if (!page) {
        // 打印错误信息，帮助调试
        cprintf("Memory allocation failed for %zu pages (size: %zu)\n", num_pages, size);
        return NULL; // 分配失败，返回 NULL
    }

    // 返回虚拟地址
    return page2kva(page);
}


    // 调整大小以适应缓存对齐
    size_t aligned_size = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);

    // 查找合适的内存缓存
    for (int i = 0; i < MAX_CACHES; i++) {
        if (cache_pool[i].objsize >= aligned_size) {
            // 在缓存中分配内存
            return my_kmem_cache_alloc(&cache_pool[i]);
        }
    }

    // 如果没有合适的缓存，返回 NULL
    return NULL;
}


void my_kfree(void *objp,struct my_kmem_cache_t* p) {
    if (objp == NULL) {
        return; // 如果传入的指针是 NULL，直接返回
    }
    // 将对象返回给相应的内存缓存
    my_kmem_cache_free(p, objp);
}
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
void slub_init()
{
    size_t sizes[MAX_CACHES] = {8, 16, 32, 64, 96, 128, 192, 256, 512, 1024, 2048};
    
    for (size_t i = 0; i < MAX_CACHES; i++) {
        cache_pool[i].objsize = sizes[i];
        /*cache_pool[i].num_objs = (PAGE_SIZE - sizeof(struct slab_t)) / sizes[i];
        list_init(&cache_pool[i].slabs_full);
        list_init(&cache_pool[i].slabs_partial);
        list_init(&cache_pool[i].slabs_free);*/
	}
    check_slub();
}

