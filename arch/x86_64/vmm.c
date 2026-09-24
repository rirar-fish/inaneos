// page mapper
#include "vmm.h"
#include "pmm.h"

#define PT_ENTRIES 512

extern uint64_t pml4[];

static void flush_all(void) {
  uint64_t cr3;
  __asm__ volatile("mov %%cr3, %0; mov %0, %%cr3" : "=r"(cr3) :: "memory");
}

static uint64_t *table_alloc(void) {
  uint64_t p = pmm_alloc_frame();
  if (!p)
    return 0;
  uint64_t *t = (uint64_t *)(uintptr_t)p;
  for (int i = 0; i < PT_ENTRIES; i++)
    t[i] = 0;
  return t;
}

// walk or create tables down to PT
static uint64_t *walk(uint64_t virt, int create) {
  uint64_t *t = pml4;
  for (int level = 3; level > 0; level--) {
    uint64_t idx = (virt >> (12 + 9 * level)) & 0x1FF;
    if (!(t[idx] & PTE_P)) {
      if (!create)
        return 0;
      uint64_t *nt = table_alloc();
      if (!nt)
        return 0;
      t[idx] = (uint64_t)nt | PTE_P | PTE_W | PTE_U; // upper levels open
    }
    t = (uint64_t *)(uintptr_t)(t[idx] & ~0xFFFUL);
  }
  return t;
}

// split 2MB page at PD level
static int split_large(uint64_t virt) {
  uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
  uint64_t pd_idx = (virt >> 21) & 0x1FF;
  uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[0] & ~0xFFFUL);
  if (!(pdpt[pdpt_idx] & PTE_P))
    return -1;
  uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFUL);
  if (!(pd[pd_idx] & PTE_P) || !(pd[pd_idx] & 0x80))
    return 0; // already 4KB or missing
  uint64_t *pt = table_alloc();
  if (!pt)
    return -1;
  uint64_t base = pd[pd_idx] & ~0x1FFFFFUL;
  uint64_t flags = pd[pd_idx] & 0xFFF & ~0x80UL;
  for (int i = 0; i < PT_ENTRIES; i++)
    pt[i] = base + i * 0x1000 + flags;
  pd[pd_idx] = (uint64_t)pt | PTE_P | PTE_W | PTE_U; // leaf guards below
  return 0;
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
  split_large(virt);
  uint64_t *pt = walk(virt, 1);
  if (!pt)
    return;
  pt[(virt >> 12) & 0x1FF] = (phys & ~0xFFFUL) | (flags & 0xFFF) | PTE_P;
  flush_all();
}

void vmm_set_user(uint64_t virt, uint64_t len) {
  uint64_t start = virt & ~0xFFFUL;
  uint64_t end = (virt + len + 0xFFF) & ~0xFFFUL;
  for (uint64_t v = start; v < end; v += 0x1000) {
    split_large(v);
    uint64_t *pt = walk(v, 0);
    if (pt && (pt[(v >> 12) & 0x1FF] & PTE_P))
      pt[(v >> 12) & 0x1FF] |= PTE_U;
  }
  flush_all();
}

uint64_t vmm_phys(uint64_t virt) {
  uint64_t *t = pml4;
  for (int level = 3; level > 0; level--) {
    uint64_t e = t[(virt >> (12 + 9 * level)) & 0x1FF];
    if (!(e & PTE_P))
      return 0;
    if (level == 1 && (e & 0x80))
      return (e & ~0x1FFFFFUL) + (virt & 0x1FFFFF);
    t = (uint64_t *)(uintptr_t)(e & ~0xFFFUL);
  }
  uint64_t e = t[(virt >> 12) & 0x1FF];
  if (!(e & PTE_P))
    return 0;
  return (e & ~0xFFFUL) + (virt & 0xFFF);
}

void vmm_unmap_range(uint64_t start, uint64_t end) {
  for (uint64_t v = start & ~0xFFFUL; v < end; v += 0x1000) {
    uint64_t *t = pml4;
    int ok = 1;
    for (int level = 3; level > 0; level--) {
      uint64_t e = t[(v >> (12 + 9 * level)) & 0x1FF];
      if (!(e & PTE_P) || (level == 1 && (e & 0x80))) {
        ok = 0;
        break;
      }
      t = (uint64_t *)(uintptr_t)(e & ~0xFFFUL);
    }
    if (!ok)
      continue;
    uint64_t *e = &t[(v >> 12) & 0x1FF];
    if (*e & PTE_P) {
      pmm_free_frame(*e & ~0xFFFUL);
      *e = 0;
    }
  }
  flush_all();
}
