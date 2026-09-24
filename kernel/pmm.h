#pragma once
#include <stdint.h>

// frame allocator, frames <1GB
void pmm_init(uint32_t mmap_addr, uint32_t mmap_len);
void pmm_reserve(uint64_t start, uint64_t end);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t phys);
uint64_t pmm_free_kb(void);
uint64_t pmm_total_kb(void);
