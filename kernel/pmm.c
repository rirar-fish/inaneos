// frame allocator
#include "pmm.h"

#define PMM_MAX_PHYS 0x40000000 // 1GB cap
#define FRAME_SIZE 4096
#define BITMAP_SIZE (PMM_MAX_PHYS / FRAME_SIZE / 8)

typedef struct {
  uint32_t size;
  uint64_t addr;
  uint64_t len;
  uint32_t type;
} __attribute__((packed)) mmap_entry;

#define MEM_AVAILABLE 1

static uint8_t bitmap[BITMAP_SIZE];
static uint64_t total_frames;
static uint64_t free_frames;
static uint64_t cursor;

extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

static void set_used(uint64_t frame) {
  if (!(bitmap[frame / 8] & (1 << (frame % 8)))) {
    bitmap[frame / 8] |= (1 << (frame % 8));
    free_frames--;
  }
}

void pmm_init(uint32_t mmap_addr, uint32_t mmap_len) {
  uint8_t *cur = (uint8_t *)(uintptr_t)mmap_addr;
  uint8_t *end = cur + mmap_len;

  total_frames = PMM_MAX_PHYS / FRAME_SIZE;
  free_frames = 0;
  cursor = 0;
  for (uint64_t i = 0; i < sizeof(bitmap); i++)
    bitmap[i] = 0xFF; // all used

  while (cur < end) {
    mmap_entry *e = (mmap_entry *)cur;
    if (e->type == MEM_AVAILABLE) {
      uint64_t s = e->addr;
      uint64_t l = e->len;
      if (s < 0x100000) { // skip low mem
        uint64_t cut = 0x100000 - s;
        if (cut >= l)
          l = 0;
        else {
          s += cut;
          l -= cut;
        }
      }
      if (s + l > PMM_MAX_PHYS) { // cap 1GB
        if (s >= PMM_MAX_PHYS)
          l = 0;
        else
          l = PMM_MAX_PHYS - s;
      }
      uint64_t f0 = (s + FRAME_SIZE - 1) / FRAME_SIZE;
      uint64_t f1 = (s + l) / FRAME_SIZE;
      for (uint64_t f = f0; f < f1; f++) {
        bitmap[f / 8] &= ~(1 << (f % 8));
        free_frames++;
      }
    }
    cur += e->size + sizeof(e->size);
  }

  uint64_t k0 = (uint64_t)_kernel_start / FRAME_SIZE; // reserve kernel
  uint64_t k1 = ((uint64_t)_kernel_end + FRAME_SIZE - 1) / FRAME_SIZE;
  for (uint64_t f = k0; f < k1; f++)
    set_used(f);
}

void pmm_reserve(uint64_t start, uint64_t end) {
  uint64_t f0 = start / FRAME_SIZE;
  uint64_t f1 = (end + FRAME_SIZE - 1) / FRAME_SIZE;
  if (f0 >= total_frames)
    return;
  if (f1 > total_frames)
    f1 = total_frames;
  for (uint64_t f = f0; f < f1; f++)
    set_used(f);
}

uint64_t pmm_alloc_frame(void) {
  for (uint64_t i = 0; i < total_frames; i++) {
    uint64_t f = (cursor + i) % total_frames;
    if (!(bitmap[f / 8] & (1 << (f % 8)))) {
      bitmap[f / 8] |= (1 << (f % 8));
      free_frames--;
      cursor = (f + 1) % total_frames;
      return f * FRAME_SIZE;
    }
  }
  return 0;
}

void pmm_free_frame(uint64_t phys) {
  uint64_t f = phys / FRAME_SIZE;
  if (f < total_frames && (bitmap[f / 8] & (1 << (f % 8)))) {
    bitmap[f / 8] &= ~(1 << (f % 8));
    free_frames++;
  }
}

uint64_t pmm_free_kb(void) { return free_frames * 4; }
uint64_t pmm_total_kb(void) { return total_frames * 4; }
