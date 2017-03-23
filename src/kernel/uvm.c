#include <type.h>
#include <idt.h>
#include <pmm.h>
#include <string.h>
#include <uvm.h>
#include <proc.h>

/* 将 0 - USER_BASE(0xc0000000) 的所有内存映射到页表中 */
void kvm_init(pde_t *pgdir) {
    uint32_t addr;

    for (addr = 0; addr < pmm_size(); addr += PAGE_SIZE) {
        vmm_map(pgdir, addr, addr, PTE_P | PTE_R | PTE_K);
    }
}

/* 映射空间 */
void uvm_init(pde_t *pgdir, char *init, uint32_t size) {
    char *room;
    room = (char *)pmm_alloc(); // 申请页框
    memcpy(room, init, size); // 拷贝初值
    vmm_map(pgdir, USER_BASE, (uint32_t)room, PTE_U | PTE_P | PTE_R); // 用户空间基址存放初值
}

// 切换至内核态
void uvm_switch(struct proc *pp) {
    tss_set(SEL_KDATA << 3, (uint32_t)pp->stack + PAGE_SIZE); // 内核态
    vmm_switch((uint32_t)pp->pgdir);
}

/* 释放空间 */
void uvm_free(pte_t *pgdir) {
    uint32_t i;

    for (i = PDE_INDEX(USER_BASE); i < PTE_COUNT; i++) {
        if (pgdir[i] & PTE_P) { // 有效
            pmm_free(pgdir[i] & PAGE_MASK); // 释放物理页
        }
    }

    pmm_free((uint32_t)pgdir); // 释放页表
}

// 拷贝空间
pde_t *uvm_copy(pte_t *pgdir, uint32_t size) {
    pde_t *pgd;
    uint32_t i, pa, mem;

    pgd = (pde_t *)pmm_alloc(); // 申请物理页

    kvm_init(pgd); // 映射空间到物理页

    for (i = 0; i < size; i += PAGE_SIZE) {
        if (vmm_ismap(pgdir, USER_BASE + i, &pa)) {
            mem = pmm_alloc();
            memcpy((void *)mem, (void *)pa, PAGE_SIZE);
            vmm_map(pgd, USER_BASE + i, mem, PTE_R | PTE_U | PTE_P);
        }
    }

    return pgd;
}

// 申请空间
int uvm_alloc(pte_t *pgdir, uint32_t old_sz, uint32_t new_sz) {
    uint32_t mem;
    uint32_t start;

    if (new_sz < old_sz) { // 新空间更小，不用申请
        return old_sz;
    }

    // 对多余的空间进行映射
    for (start = PAGE_ALIGN_UP(old_sz); start < new_sz; start += PAGE_SIZE) {
        mem = pmm_alloc();
        vmm_map(pgdir, start, mem, PTE_P | PTE_R | PTE_U);
    }

    return new_sz;
}
