#ifndef __KERN_MM_MYSLUB_H__
#define __KERN_MM_MYSLUB_H__

#include "myslub.h"
#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>
#include <defs.h>
#include<stdio.h>
#define PAGE_SIZE 4096        // 每页大小
#define MAX_CACHES 11        // 设定最大缓存数量

struct slab_t {
    list_entry_t link;         // 用于链接到 slab 链表
   void *free_list;           // 指向空闲对象列表
    uint16_t free_count;       // 当前 slab 中空闲对象数
    uint16_t total_count;      // slab 中对象总数
};

// 定义 slab cache 结构
struct my_kmem_cache_t {
    list_entry_t slabs_full;       // 全满 slab 链表
    list_entry_t slabs_partial;    // 部分空闲 slab 链表
    list_entry_t slabs_free;       // 全空闲 slab 链表
    uint16_t objsize;              // 单个对象的大小
    uint16_t num_objs;             // 每个 slab 能容纳的对象数量
    //list_entry_t cache_link;       // 用于缓存链表的链表链接
};
static struct my_kmem_cache_t cache_pool[MAX_CACHES]; // 静态数组


static struct list_entry cache_list_head = { .prev = &cache_list_head, .next = &cache_list_head };
// 定义 slab 结构


// 创建 kmem cache
struct my_kmem_cache_t *my_kmem_cache_create(size_t size);

// 销毁 kmem cache
void my_kmem_cache_destroy(struct my_kmem_cache_t *cachep);

// 从 cache 中分配对象
void *my_kmem_cache_alloc(struct my_kmem_cache_t *cachep);

// 释放对象
void my_kmem_cache_free(struct my_kmem_cache_t *cachep, void *objp);

// kmalloc 实现，支持任意大小内存分配
void *my_kmalloc(int size);

// kfree 实现
void my_kfree(void *objp,struct my_kmem_cache_t* );
size_t length(list_entry_t *head); 
void slub_init();

#endif /* ! __KERN_MM_MYSLUB_H__ */
