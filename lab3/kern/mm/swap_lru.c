#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_lru.h>
#include <list.h>



list_entry_t pra_list_head;

list_entry_t *curr_ptr;


static int
_lru_init_mm(struct mm_struct* mm)//mm 是指向 mm_struct 类型的指针，代表一个进程的内存管理结构体（memory management structure）
{
    list_init(&pra_list_head);
    mm->sm_priv = &pra_list_head;
    //链表的头部地址存储在 mm 结构体中的 sm_priv 字段里。这样，在内存管理结构中可以访问到这条链表

    return 0;
}

static int
_lru_map_swappable(struct mm_struct* mm, uintptr_t addr, struct Page* page, int swap_in)
{
    list_entry_t* head = (list_entry_t*)mm->sm_priv;
    list_entry_t* entry = &(page->pra_page_link);//需要处理的页面的链表节点

    assert(entry != NULL && head != NULL);
    
    list_add(head, entry);//将 entry（当前页面的链表节点）添加到链表的头部
    return 0;
}

_lru_swap_out_victim(struct mm_struct* mm, struct Page** ptr_page, int in_tick)
{
    list_entry_t* head = (list_entry_t*)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);
    
    list_entry_t* entry = list_prev(head);//entry获取链表尾部
    if (entry != head) {
        list_del(entry);//删除最后一个，被交换出去了
        *ptr_page = le2page(entry, pra_page_link);//将链表节点 entry 转换为页面结构体指针，并赋值给 *ptr_page
    }
    else {
        *ptr_page = NULL;
    }
    return 0;
}

static void
print_mm_list() {
    cprintf("--------begin----------\n");
    list_entry_t* head = &pra_list_head, * le = head;
   

    while ((le = list_next(le)) != head)//遍历链表
    {
        struct Page* page = le2page(le, pra_page_link);//将当前链表节点 le 转换为页面结构体 page
        cprintf("vaddr: %x\n", page->pra_vaddr);//打印当前页面的虚拟地址 page->pra_vaddr
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

static int
_lru_init(void)
{
    return 0;
}

static int
_lru_set_unswappable(struct mm_struct* mm, uintptr_t addr)
{
    return 0;
}

static int
_lru_tick_event(struct mm_struct* mm)
{
    return 0;
}

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



struct swap_manager swap_manager_lru =
{
     .name = "lru swap manager",
     .init = &_lru_init,
     .init_mm = &_lru_init_mm,
     .tick_event = &_lru_tick_event,
     .map_swappable = &_lru_map_swappable,
     .set_unswappable = &_lru_set_unswappable,
     .swap_out_victim = &_lru_swap_out_victim,
     .check_swap = &_lru_check_swap,
};
