#pragma once
#include <stdint.h>

#define PTE_P 0x001
#define PTE_W 0x002
#define PTE_U 0x004

// 4KB map + user flag range + phys lookup + range free
void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_set_user(uint64_t virt, uint64_t len);
uint64_t vmm_phys(uint64_t virt);
void vmm_unmap_range(uint64_t start, uint64_t end);
