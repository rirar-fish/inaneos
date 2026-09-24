// gdt + tss
#include "gdt.h"
#include <stdint.h>

#define GDT_N 7
#define TSS_SEL 0x28

static uint64_t gdt[GDT_N];

typedef struct {
  uint32_t reserved0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t reserved1;
  uint64_t ist[7];
  uint64_t reserved2;
  uint16_t reserved3;
  uint16_t iomap_base;
} __attribute__((packed)) tss64;

_Static_assert(sizeof(tss64) == 104, "tss size");

static tss64 tss;
extern uint8_t kstack_irq_top[];

void gdt_install(void) {
  gdt[0] = 0;
  gdt[1] = 0x00209A0000000000; // kcode
  gdt[2] = 0x0000920000000000; // kdata
  gdt[3] = 0x00CFF2000000FFFF; // udata
  gdt[4] = 0x0020FA0000000000; // ucode

  uint64_t base = (uint64_t)&tss;
  gdt[5] = (0x67) | ((base & 0xFFFFFF) << 16) | ((uint64_t)0x89 << 40) |
           (((base >> 24) & 0xFF) << 56);
  gdt[6] = base >> 32;

  tss.rsp0 = (uint64_t)kstack_irq_top;
  tss.iomap_base = sizeof(tss);

  struct {
    uint16_t limit;
    uint64_t base;
  } __attribute__((packed)) gdtr = {sizeof(gdt) - 1, (uint64_t)gdt};

  __asm__ volatile("lgdt %0" ::"m"(gdtr));
  __asm__ volatile("pushq $0x08\n"
                   "leaq 1f(%%rip), %%rax\n"
                   "pushq %%rax\n"
                   "retfq\n"
                   "1:\n" ::: "rax");
  __asm__ volatile("mov $0x10, %%ax\n"
                   "mov %%ax, %%ds\n"
                   "mov %%ax, %%es\n"
                   "mov %%ax, %%fs\n"
                   "mov %%ax, %%gs\n"
                   "mov %%ax, %%ss\n" ::: "ax");
  uint16_t tr = TSS_SEL;
  __asm__ volatile("ltr %0" ::"m"(tr));
}
